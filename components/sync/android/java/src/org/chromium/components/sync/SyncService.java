// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.json.JSONArray;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

import java.util.HashMap;
import java.util.Set;

/**
 * Java version of the native SyncService interface. Must only be used on the UI thread.
 * TODO(crbug.com/40161455): Document the remaining methods.
 */
public interface SyncService {
    /** Listener for the underlying sync status. */
    public interface SyncStateChangedListener {
        // Invoked when the status has changed.
        public void syncStateChanged();
    }

    /**
     * Checks if the sync engine is initialized. Note that this refers to Sync-the-transport, i.e.
     * it can be true even if the user has *not* enabled Sync-the-feature. This mostly needs to be
     * checked as a precondition for the various encryption-related methods (see below).
     *
     * @return true if the sync engine is initialized.
     */
    public boolean isEngineInitialized();

    /**
     * Returns whether all conditions are satisfied for Sync-the-feature to start. This means that
     * there is a Sync-consented account, no disable reasons, and first-time Sync setup has been
     * completed by the user.
     *
     * <p>Note: This does not imply that Sync is actually running. Check IsSyncFeatureActive or
     * GetTransportState to get the current state.
     *
     * @return true if the sync feature is enabled.
     */
    // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is deleted from the
    // codebase. See ConsentLevel::kSync documentation for details.
    public boolean isSyncFeatureEnabled();

    /**
     * Checks whether Sync-the-feature is currently active. Note that Sync-the-transport may be
     * active even if this is false.
     *
     * @return true if Sync is active, false otherwise.
     */
    // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is deleted from the
    // codebase. See ConsentLevel::kSync documentation for details.
    public boolean isSyncFeatureActive();

    public @GoogleServiceAuthError.State int getAuthError();

    /**
     * Checks whether Sync is disabled by enterprise policy (through prefs) or account policy
     * received from the sync server.
     *
     * @return true if Sync is disabled, false otherwise.
     */
    public boolean isSyncDisabledByEnterprisePolicy();

    public boolean hasUnrecoverableError();

    public boolean requiresClientUpgrade();

    public @Nullable CoreAccountInfo getAccountInfo();

    /**
     * Checks whether the primary account is consented to run Sync (the feature). Note that even if
     * this is true, other reasons might prevent Sync from actually starting up.
     *
     * @return true if the primary account is consented to Sync (the feature), false otherwise.
     */
    // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is deleted from the
    // codebase. See ConsentLevel::kSync documentation for details.
    public boolean hasSyncConsent();

    /**
     * Gets the set of data types that are currently syncing.
     *
     * <p>This is affected by whether sync is on.
     *
     * @return DataType set of active data types.
     */
    public Set<Integer> getActiveDataTypes();

    /**
     * Gets the set of types that the user has selected.
     *
     * @return UserSelectableType set of selected types.
     */
    public Set<Integer> getSelectedTypes();

    /**
     * Returns the datatypes which have local changes that have not yet been synced with the server.
     * Note: This includes deletions as well.
     */
    public void getTypesWithUnsyncedData(Callback<Set<Integer>> callback);

    /**
     * Queries the count and description/preview of existing local data for `types` data types. This
     * is an asynchronous method which returns the result via the callback `callback` once the
     * information for all the data types in `types` is available. Note: Only data types that are
     * enabled and support this functionality are part of the response. Note: Only data types that
     * are ready for migration are returned.
     */
    public void getLocalDataDescriptions(
            Set<Integer> types, Callback<HashMap<Integer, LocalDataDescription>> callback);

    public void triggerLocalDataMigration(Set<Integer> types);

    public boolean hasKeepEverythingSynced();

    public boolean isTypeManagedByPolicy(@UserSelectableType int type);

    public boolean isTypeManagedByCustodian(@UserSelectableType int type);

    /**
     * Enables syncing for the passed types.
     *
     * @param syncEverything Set to true if the user wants to sync all data types (including new
     *     data types we add in the future).
     * @param enabledTypes The set of types to enable.
     */
    public void setSelectedTypes(boolean syncEverything, Set<Integer> enabledTypes);

    /**
     * Sets an individual type selection. For Sync-the-feature mode, invoking this function is only
     * allowed while IsSyncEverythingEnabled() returns false.
     *
     * @param type The type that should be enabled or disabled.
     * @param isTypeOn Set to true if the type should be enabled, false otherwise.
     */
    public void setSelectedType(@UserSelectableType int type, boolean isTypeOn);

    public void setInitialSyncFeatureSetupComplete(int syncFirstSetupCompleteSource);

    public boolean isInitialSyncFeatureSetupComplete();

    public void setSyncRequested();

    /**
     * Instances of this class keep sync paused until {@link #close} is called. Use
     * {@link SyncService#getSetupInProgressHandle} to create. Please note that
     * {@link #close} should be called on every instance of this class.
     */
    public interface SyncSetupInProgressHandle {
        public void close();
    }

    /**
     * Called by the UI to prevent changes in sync settings from taking effect while these settings
     * are being modified by the user. When sync settings UI is no longer visible,
     * {@link SyncSetupInProgressHandle#close} has to be invoked for sync settings to be applied.
     * Sync settings will remain paused as long as there are unclosed objects returned by this
     * method. Please note that the behavior of SyncSetupInProgressHandle is slightly different from
     * the equivalent C++ object, as Java instances don't commit sync settings as soon as any
     * instance of SyncSetupInProgressHandle is closed.
     */
    public SyncSetupInProgressHandle getSetupInProgressHandle();

    public void addSyncStateChangedListener(SyncStateChangedListener listener);

    public void removeSyncStateChangedListener(SyncStateChangedListener listener);

    /**
     * Returns the actual passphrase type being used for encryption. The sync engine must be running
     * (isEngineInitialized() returns true) before calling this function.
     *
     * <p>This method should only be used if you want to know the raw value. For checking whether we
     * should ask the user for a passphrase, use isPassphraseRequiredForPreferredDataTypes().
     */
    public @PassphraseType int getPassphraseType();

    /**
     * The overall state of Sync-the-transport, in ascending order of "activeness". Note that this
     * refers to the transport layer, which may be active even if Sync-the-feature is turned off.
     */
    public @TransportState int getTransportState();

    /**
     * Checks if sync is currently set to use a custom passphrase (or the similar -and legacy-
     * frozen implicit passphrase). The sync engine must be running (isEngineInitialized() returns
     * true) before calling this function.
     *
     * @return true if sync is using a custom passphrase.
     */
    public boolean isUsingExplicitPassphrase();

    /**
     * Checks if we need a passphrase to decrypt a currently-enabled data type. This returns false
     * if a passphrase is needed for a type that is not currently enabled.
     *
     * @return true if we need a passphrase.
     */
    public boolean isPassphraseRequiredForPreferredDataTypes();

    /**
     * Checks if trusted vault encryption keys are needed, independently of the currently-enabled
     * data types.
     *
     * @return true if we need an encryption key.
     */
    public boolean isTrustedVaultKeyRequired();

    /**
     * Checks if trusted vault encryption keys are needed to decrypt a currently-enabled data type.
     *
     * @return true if we need an encryption key for a type that is currently enabled.
     */
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes();

    /**
     * Checks if recoverability of the trusted vault keys is degraded and user action is required,
     * affecting currently enabled data types.
     *
     * @return true if recoverability is degraded.
     */
    public boolean isTrustedVaultRecoverabilityDegraded();

    /** @return Whether setting a custom passphrase is allowed. */
    public boolean isCustomPassphraseAllowed();

    /**
     * Checks if the user has chosen to encrypt all data types. Note that some data types (e.g.
     * DEVICE_INFO) are never encrypted.
     *
     * @return true if all data types are encrypted, false if only passwords are encrypted.
     */
    public boolean isEncryptEverythingEnabled();

    public void setEncryptionPassphrase(String passphrase);

    public boolean setDecryptionPassphrase(String passphrase);

    /**
     * Returns whether this client has previously prompted the user for a
     * passphrase error via the android system notifications for the current
     * product major version (i.e. gets reset upon browser upgrade). More
     * specifically, it returns whether the method
     * markPassphrasePromptMutedForCurrentProductVersion() has been invoked
     * before, since the last time the browser was upgraded to a new major
     * version.
     *
     * Can be called whether or not sync is initialized.
     *
     * @return Whether client has prompted for a passphrase error previously for
     * the current product major version.
     */
    public boolean isPassphrasePromptMutedForCurrentProductVersion();

    /**
     * Mutes passphrase error via the android system notifications until the
     * browser is upgraded to a new major version.
     *
     * Can be called whether or not sync is initialized.
     */
    public void markPassphrasePromptMutedForCurrentProductVersion();

    /** @return Whether the user should be offered to opt in to trusted vault encryption. */
    public boolean shouldOfferTrustedVaultOptIn();

    /** @return Whether sync is enabled to sync urls with a non custom passphrase. */
    public boolean isSyncingUnencryptedUrls();

    /** @return Returns the pointer the corresponding native object. */
    @CalledByNative
    public long getNativeSyncServiceAndroidBridge();

    /**
     * Returns the time when the last sync cycle was completed.
     *
     * @return The difference measured in microseconds, between last sync cycle completion time
     * and 1 January 1970 00:00:00 UTC.
     */
    @VisibleForTesting
    public long getLastSyncedTimeForDebugging();

    @VisibleForTesting
    public void triggerRefresh();

    /**
     * Retrieves a JSON version of local Sync data via the native GetAllNodes method.
     * This method is asynchronous; the result will be sent to the callback.
     */
    @VisibleForTesting
    public void getAllNodes(Callback<JSONArray> callback);
}
