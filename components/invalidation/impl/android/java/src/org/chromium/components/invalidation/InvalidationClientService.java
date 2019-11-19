// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.accounts.Account;
import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types.ClientType;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelTypeHelper;
import org.chromium.components.sync.SyncConstants;
import org.chromium.components.sync.notifier.InvalidationClientNameProvider;
import org.chromium.components.sync.notifier.InvalidationIntentProtocol;
import org.chromium.components.sync.notifier.InvalidationPreferences;
import org.chromium.components.sync.notifier.InvalidationPreferences.EditContext;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Service that controls notifications for sync.
 * <p>
 * This service serves two roles. On the one hand, it is a client for the notification system
 * used to trigger sync. It receives invalidations and converts them into
 * {@link ContentResolver#requestSync} calls, and it supplies the notification system with the set
 * of desired registrations when requested.
 * <p>
 * On the other hand, this class is controller for the notification system. It starts it and stops
 * it, and it requests that it perform (un)registrations as the set of desired sync types changes.
 * <p>
 * This class is an {@code IntentService}. All methods are assumed to be executing on its single
 * execution thread.
 *
 * @author dsmyers@google.com
 */
public class InvalidationClientService extends AndroidListener {
    /* This class must be public because it is exposed as a service. */

    /** Notification client typecode. */
    @VisibleForTesting
    static final int CLIENT_TYPE = ClientType.CHROME_SYNC_ANDROID;

    private static final String TAG = "invalidation";
    private static final String CLIENT_SERVICE_KEY = "ipc.invalidation.ticl.listener_service_class";

    protected static boolean sShouldCreateService = true;

    /**
     * Whether the underlying notification client has been started. This boolean is updated when a
     * start or stop intent is issued to the underlying client, not when the intent is actually
     * processed.
     */
    private static boolean sIsClientStarted;

    /**
     * The id of the client in use, if any. May be {@code null} if {@link #sIsClientStarted} is
     * true if the client has not yet gone ready.
     */
    @Nullable private static byte[] sClientId;

    private static AtomicReference<Class<? extends InvalidationClientService>> sServiceClass =
            new AtomicReference<>();

    private static Class<? extends InvalidationClientService>
            findRegisteredInvalidationClientService() {
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ApplicationInfo appInfo;
        try {
            appInfo = packageManager.getApplicationInfo(context.getPackageName(),
                    PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                String serviceMetadata = appInfo.metaData.getString(CLIENT_SERVICE_KEY, null);
                if (serviceMetadata == null) return InvalidationClientService.class;

                Class<?> foundClass = Class.forName(serviceMetadata);
                Class<? extends InvalidationClientService> serviceClass =
                        foundClass.asSubclass(InvalidationClientService.class);
                return serviceClass;
            }
        } catch (NameNotFoundException | ClassNotFoundException | ClassCastException e) {
            Log.e(TAG, "Unable to find registered client service", e);
        }
        return InvalidationClientService.class;
    }

    /**
     * @return The registered {@link InvalidationClientService} class reference to use for
     *         interacting with the service.
     */
    public static Class<? extends InvalidationClientService> getRegisteredClass() {
        if (sServiceClass.get() == null) {
            sServiceClass.compareAndSet(null, findRegisteredInvalidationClientService());
        }
        return sServiceClass.get();
    }

    @Override
    public void onHandleIntent(Intent intent) {
        if (intent == null) {
            return;
        }

        // Ensure that a client is or is not running, as appropriate, and that it is for the
        // correct account. ensureAccount will stop the client if account is non-null and doesn't
        // match the stored account. Then, if a client should be running, ensureClientStartState
        // will start a new one if needed. I.e., these two functions work together to restart the
        // client when the account changes.
        Account account = intent.hasExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT)
                ? (Account) intent.getParcelableExtra(InvalidationIntentProtocol.EXTRA_ACCOUNT)
                : null;

        ensureAccount(account);
        ensureClientStartState();

        // Handle the intent.
        if (InvalidationIntentProtocol.isStop(intent) && sIsClientStarted) {
            // If the intent requests that the client be stopped, stop it.
            stopClient();
        } else if (InvalidationIntentProtocol.isRegisteredTypesChange(intent)) {
            // If the intent requests a change in registrations, change them.
            List<String> regTypes = intent.getStringArrayListExtra(
                    InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES);
            setRegisteredTypes(regTypes != null ? new HashSet<String>(regTypes) : null,
                    InvalidationIntentProtocol.getRegisteredObjectIds(intent));
        } else {
            // Otherwise, we don't recognize the intent. Pass it to the notification client service.
            super.onHandleIntent(intent);
        }
    }

    @Override
    public void invalidate(Invalidation invalidation, byte[] ackHandle) {
        byte[] payload = invalidation.getPayload();
        String payloadStr = (payload == null) ? null : new String(payload);
        requestSync(invalidation.getObjectId(), invalidation.getVersion(), payloadStr);
        acknowledge(ackHandle);
    }

    @Override
    public void invalidateUnknownVersion(ObjectId objectId, byte[] ackHandle) {
        requestSync(objectId, 0L, null);
        acknowledge(ackHandle);
    }

    @Override
    public void invalidateAll(byte[] ackHandle) {
        requestSync(null, 0L, null);
        acknowledge(ackHandle);
    }

    @Override
    public void informRegistrationFailure(
            byte[] clientId, ObjectId objectId, boolean isTransient, String errorMessage) {
        Log.w(TAG, "Registration failure on " + objectId + " ; transient = " + isTransient
                + ": " + errorMessage);
        if (isTransient) {
          // Retry immediately on transient failures. The base AndroidListener will handle
          // exponential backoff if there are repeated failures.
            List<ObjectId> objectIdAsList = CollectionUtil.newArrayList(objectId);
            if (readRegistrationsFromPrefs().contains(objectId)) {
                register(clientId, objectIdAsList);
            } else {
                unregister(clientId, objectIdAsList);
            }
        }
    }

    @Override
    public void informRegistrationStatus(
            byte[] clientId, ObjectId objectId, RegistrationState regState) {
        Log.d(TAG, "Registration status for " + objectId + ": " + regState);
        List<ObjectId> objectIdAsList = CollectionUtil.newArrayList(objectId);
        boolean registrationisDesired = readRegistrationsFromPrefs().contains(objectId);
        if (regState == RegistrationState.REGISTERED) {
            if (!registrationisDesired) {
                Log.i(TAG, "Unregistering for object we're no longer interested in");
                unregister(clientId, objectIdAsList);
            }
        } else {
            if (registrationisDesired) {
                Log.i(TAG, "Registering for an object");
                register(clientId, objectIdAsList);
            }
        }
    }

    @Override
    public void informError(ErrorInfo errorInfo) {
        Log.w(TAG, "Invalidation client error:" + errorInfo);
        if (!errorInfo.isTransient() && sIsClientStarted) {
            // It is important not to stop the client if it is already stopped. Otherwise, the
            // possibility exists to go into an infinite loop if the stop call itself triggers an
            // error (e.g., because no client actually exists).
            stopClient();
        }
    }

    @Override
    public void ready(byte[] clientId) {
        setClientId(clientId);

        // We might have accumulated some registrations to do while we were waiting for the client
        // to become ready.
        reissueRegistrations(clientId);
    }

    @Override
    public void reissueRegistrations(byte[] clientId) {
        Set<ObjectId> desiredRegistrations = readRegistrationsFromPrefs();
        if (!desiredRegistrations.isEmpty()) {
            register(clientId, desiredRegistrations);
        }
    }

    @SuppressWarnings("NoContextGetApplicationContext")
    @Override
    public void requestAuthToken(
            final PendingIntent pendingIntent, @Nullable String invalidAuthToken) {
        @Nullable
        Account account = ChromeSigninController.get().getSignedInUser();
        if (account == null) {
            // This should never happen, because this code should only be run if a user is
            // signed-in.
            Log.w(TAG, "No signed-in user; cannot send message to data center");
            return;
        }

        ThreadUtils.runOnUiThread(() -> {
            // Attempt to retrieve a token for the user. This method will also invalidate
            // invalidAuthToken if it is non-null.
            IdentityManager.getNewAccessTokenWithFacade(AccountManagerFacade.get(), account,
                    invalidAuthToken, SyncConstants.CHROME_SYNC_OAUTH2_SCOPE,
                    new IdentityManager.GetAccessTokenCallback() {
                        @Override
                        public void onGetTokenSuccess(String token) {
                            setAuthToken(InvalidationClientService.this.getApplicationContext(),
                                    pendingIntent, token, SyncConstants.CHROME_SYNC_OAUTH2_SCOPE);
                        }

                        @Override
                        public void onGetTokenFailure(boolean isTransientError) {}
                    });
        });
    }

    @Override
    public void writeState(byte[] data) {
        InvalidationPreferences invPreferences = new InvalidationPreferences();
        EditContext editContext = invPreferences.edit();
        invPreferences.setInternalNotificationClientState(editContext, data);
        invPreferences.commit(editContext);
    }

    @Override
    @Nullable public byte[] readState() {
        return new InvalidationPreferences().getInternalNotificationClientState();
    }

    /**
     * Ensures that the client is running or not running as appropriate, based on the value of
     * {@link #shouldClientBeRunning}.
     */
    private void ensureClientStartState() {
        final boolean shouldClientBeRunning = shouldClientBeRunning();
        if (!shouldClientBeRunning && sIsClientStarted) {
            // Stop the client if it should not be running and is.
            stopClient();
        } else if (shouldClientBeRunning && !sIsClientStarted) {
            // Start the client if it should be running and isn't.
            startClient();
        }
    }

    /**
     * If {@code intendedAccount} is non-{@null} and differs from the account stored in preferences,
     * then stops the existing client (if any) and updates the stored account.
     */
    private void ensureAccount(@Nullable Account intendedAccount) {
        if (intendedAccount == null) {
            return;
        }
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        if (!intendedAccount.equals(invPrefs.getSavedSyncedAccount())) {
            if (sIsClientStarted) {
                stopClient();
            }
            setAccount(intendedAccount);
        }
    }

    /**
     * Starts a new client, destroying any existing client. {@code owningAccount} is the account
     * of the user for which the client is being created; it will be persisted using
     * {@link InvalidationPreferences#setAccount}.
     */
    private void startClient() {
        byte[] clientName = InvalidationClientNameProvider.get().getInvalidatorClientName();
        Intent startIntent = AndroidListener.createStartIntent(this, CLIENT_TYPE, clientName);
        startServiceIfPossible(startIntent);
        setIsClientStarted(true);
    }

    /** Stops the notification client. */
    private void stopClient() {
        startServiceIfPossible(AndroidListener.createStopIntent(this));
        setIsClientStarted(false);
        setClientId(null);
    }

    /** Sets the saved sync account in {@link InvalidationPreferences} to {@code owningAccount}. */
    private void setAccount(Account owningAccount) {
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        EditContext editContext = invPrefs.edit();
        invPrefs.setAccount(editContext, owningAccount);
        invPrefs.commit(editContext);
    }

    /**
     * Reads the saved sync types from storage (if any) and returns a set containing the
     * corresponding object ids.
     */
    private Set<ObjectId> readSyncRegistrationsFromPrefs() {
        Set<String> savedTypes = new InvalidationPreferences().getSavedSyncedTypes();
        if (savedTypes == null) return Collections.emptySet();
        return ModelTypeHelper.notificationTypesToObjectIds(savedTypes);
    }

    /**
     * Reads the saved non-sync object ids from storage (if any) and returns a set containing the
     * corresponding object ids.
     */
    private Set<ObjectId> readNonSyncRegistrationsFromPrefs() {
        Set<ObjectId> objectIds = new InvalidationPreferences().getSavedObjectIds();
        if (objectIds == null) return Collections.emptySet();
        return objectIds;
    }

    /**
     * Reads the object registrations from storage (if any) and returns a set containing the
     * corresponding object ids.
     */
    @VisibleForTesting
    Set<ObjectId> readRegistrationsFromPrefs() {
        return joinRegistrations(readSyncRegistrationsFromPrefs(),
                readNonSyncRegistrationsFromPrefs());
    }

    /**
     * Join Sync object registrations with non-Sync object registrations to get the full set of
     * desired object registrations.
     */
    private static Set<ObjectId> joinRegistrations(Set<ObjectId> syncRegistrations,
                                                   Set<ObjectId> nonSyncRegistrations) {
        if (nonSyncRegistrations.isEmpty()) {
            return syncRegistrations;
        }
        if (syncRegistrations.isEmpty()) {
            return nonSyncRegistrations;
        }
        Set<ObjectId> registrations = new HashSet<ObjectId>(
                syncRegistrations.size() + nonSyncRegistrations.size());
        registrations.addAll(syncRegistrations);
        registrations.addAll(nonSyncRegistrations);
        return registrations;
    }

    /**
     * Sets the types for which notifications are required.
     * @param syncTypes the sync types for which notifications are required. Null if the required
     *    registrations are not changing.
     * @param objectIds non-sync object ids for which notifications are required. Null if the
     *    required registrations are not changing.
     */
    private void setRegisteredTypes(Set<String> syncTypes, Set<ObjectId> objectIds) {
        // If we have a ready client and will be making registration change calls on it, then
        // read the current registrations from preferences before we write the new values, so that
        // we can take the diff of the two registration sets and determine which registration change
        // calls to make.
        Set<ObjectId> existingSyncRegistrations = (sClientId == null)
                ? null : readSyncRegistrationsFromPrefs();
        Set<ObjectId> existingNonSyncRegistrations = (sClientId == null)
                ? null : readNonSyncRegistrationsFromPrefs();

        // Write the new sync types/object ids to preferences.
        InvalidationPreferences prefs = new InvalidationPreferences();
        EditContext editContext = prefs.edit();
        if (syncTypes != null) {
            prefs.setSyncTypes(editContext, syncTypes);
        }
        if (objectIds != null) {
            prefs.setObjectIds(editContext, objectIds);
        }
        prefs.commit(editContext);

        // If we do not have a ready invalidation client, we cannot change its registrations, so
        // return. Later, when the client is ready, we will supply the new registrations.
        if (sClientId == null) {
            return;
        }

        // We do have a ready client. Unregister any existing registrations not present in the
        // new set and register any elements in the new set not already present.
        // When computing the desired set of object ids, if only sync types were provided, then
        // keep the existing non-sync types, and vice-versa.
        Set<ObjectId> desiredSyncRegistrations = syncTypes != null
                ? ModelTypeHelper.notificationTypesToObjectIds(syncTypes)
                : existingSyncRegistrations;
        Set<ObjectId> desiredNonSyncRegistrations = objectIds != null
                ? objectIds : existingNonSyncRegistrations;
        Set<ObjectId> desiredRegistrations = joinRegistrations(desiredNonSyncRegistrations,
                desiredSyncRegistrations);
        Set<ObjectId> existingRegistrations = joinRegistrations(existingNonSyncRegistrations,
                existingSyncRegistrations);

        Set<ObjectId> unregistrations = new HashSet<ObjectId>();
        Set<ObjectId> registrations = new HashSet<ObjectId>();
        computeRegistrationOps(existingRegistrations, desiredRegistrations,
                registrations, unregistrations);
        unregister(sClientId, unregistrations);
        register(sClientId, registrations);
    }

    /**
     * Computes the set of (un)registrations to perform so that the registrations active in the
     * Ticl will be {@code desiredRegs}, given that {@existingRegs} already exist.
     *
     * @param regAccumulator registrations to perform
     * @param unregAccumulator unregistrations to perform.
     */
    @VisibleForTesting
    static void computeRegistrationOps(Set<ObjectId> existingRegs, Set<ObjectId> desiredRegs,
            Set<ObjectId> regAccumulator, Set<ObjectId> unregAccumulator) {

        // Registrations to do are elements in the new set but not the old set.
        regAccumulator.addAll(desiredRegs);
        regAccumulator.removeAll(existingRegs);

        // Unregistrations to do are elements in the old set but not the new set.
        unregAccumulator.addAll(existingRegs);
        unregAccumulator.removeAll(desiredRegs);
    }

    /**
     * Requests that the sync system perform a sync.
     *
     * @param objectId the object that changed, if known.
     * @param version the version of the object that changed, if known.
     * @param payload the payload of the change, if known.
     */
    private void requestSync(@Nullable ObjectId objectId, long version, @Nullable String payload) {
        int objectSource = 0;
        String objectName = null;
        if (objectId != null) {
            objectName = new String(objectId.getName());
            objectSource = objectId.getSource();
        }
        Bundle bundle =
                PendingInvalidation.createBundle(objectName, objectSource, version, payload);
        Account account = ChromeSigninController.get().getSignedInUser();
        String contractAuthority = AndroidSyncSettings.get().getContractAuthority();
        requestSyncFromContentResolver(bundle, account, contractAuthority);
    }

    /**
     * Calls {@link ContentResolver#requestSync(Account, String, Bundle)} to trigger a sync. Split
     * into a separate method so that it can be overriden in tests.
     */
    @VisibleForTesting
    void requestSyncFromContentResolver(
            Bundle bundle, Account account, String contractAuthority) {
        Log.d(TAG, "Request sync: " + account + " / " + contractAuthority + " / "
                + new PendingInvalidation(bundle).toDebugString());
        ContentResolver.requestSync(account, contractAuthority, bundle);
    }

    /**
     * Returns whether the notification client should be running, i.e., whether Chrome is in the
     * foreground and sync is enabled.
     */
    @VisibleForTesting
    boolean shouldClientBeRunning() {
        return isSyncEnabled() && isChromeInForeground();
    }

    /** Returns whether sync is enabled. LLocal method so it can be overridden in tests. */
    @VisibleForTesting
    boolean isSyncEnabled() {
        return AndroidSyncSettings.get().isSyncEnabled();
    }

    /**
     * Returns whether Chrome is in the foreground. Local method so it can be overridden in tests.
     */
    @VisibleForTesting
    boolean isChromeInForeground() {
        return ApplicationStatus.hasVisibleActivities();
    }

    /** Returns whether the notification client has been started, for tests. */
    @VisibleForTesting
    static boolean getIsClientStartedForTest() {
        return sIsClientStarted;
    }

    /** Returns the notification client id, for tests. */
    @VisibleForTesting
    @Nullable static byte[] getClientIdForTest() {
        return sClientId;
    }

    private static void setClientId(byte[] clientId) {
        sClientId = clientId;
    }

    private static void setIsClientStarted(boolean isStarted) {
        sIsClientStarted = isStarted;
    }

    private void startServiceIfPossible(Intent intent) {
        if (!sShouldCreateService) return;
        // The use of background services is restricted when the application is not in foreground
        // for O. See crbug.com/680812.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                startService(intent);
            } catch (IllegalStateException exception) {
                Log.e(TAG, "Failed to start service from exception: ", exception);
            }
        } else {
            startService(intent);
        }
    }

    public void setShouldCreateService(boolean shouldCreate) {
        sShouldCreateService = shouldCreate;
    }
}
