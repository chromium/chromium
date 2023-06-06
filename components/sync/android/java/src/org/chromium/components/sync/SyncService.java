// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

import java.util.Date;
import java.util.Set;

/**
 * Java version of the native SyncService interface. Must only be used on the UI thread.
 * TODO(crbug.com/1451811): Update to no reference UI thread.
 * TODO(crbug.com/1158816): Document the remaining methods.
 */
public abstract class SyncService {
    /**
     * Listener for the underlying sync status.
     */
    public interface SyncStateChangedListener {
        // Invoked when the status has changed.
        public void syncStateChanged();
    }

    /**
     * Checks if the sync engine is initialized. Note that this refers to
     * Sync-the-transport, i.e. it can be true even if the user has *not*
     * enabled Sync-the-feature.
     * This mostly needs to be checked as a precondition for the various
     * encryption-related methods (see below).
     *
     * @return true if the sync engine is initialized.
     */
    public abstract boolean isEngineInitialized();

    /**
     * Checks whether sync machinery is active.
     *
     * @return true if the transport state is active.
     */
    public abstract boolean isTransportStateActive();

    /**
     * Checks whether Sync-the-feature can (attempt to) start. This means that there is a primary
     * account and no disable reasons. Note that the Sync machinery may start up in transport-only
     * mode even if this is false.
     *
     * @return true if Sync can start, false otherwise.
     */
    public abstract boolean canSyncFeatureStart();

    /**
     * Returns whether all conditions are satisfied for Sync-the-feature to start.
     * This means that there is a Sync-consented account, no disable reasons, and
     * first-time Sync setup has been completed by the user.
     *
     * Note: This does not imply that Sync is actually running. Check
     * IsSyncFeatureActive or GetTransportState to get the current state.
     *
     * @return true if the sync feature is enabled.
     */
    public abstract boolean isSyncFeatureEnabled();

    /**
     * Checks whether Sync-the-feature is currently active. Note that Sync-the-transport may be
     * active even if this is false.
     *
     * @return true if Sync is active, false otherwise.
     */
    public abstract boolean isSyncFeatureActive();

    public abstract @GoogleServiceAuthError.State int getAuthError();

    /**
     * Checks whether Sync is disabled by enterprise policy (through prefs) or account policy
     * received from the sync server.
     *
     * @return true if Sync is disabled, false otherwise.
     */
    public abstract boolean isSyncDisabledByEnterprisePolicy();

    public abstract boolean hasUnrecoverableError();

    public abstract boolean requiresClientUpgrade();

    public abstract @Nullable CoreAccountInfo getAccountInfo();

    /**
     * Checks whether the primary account is consented to run Sync (the feature). Note that even if
     * this is true, other reasons might prevent Sync from actually starting up.
     *
     * @return true if the primary account is consented to Sync (the feature), false otherwise.
     */
    public abstract boolean hasSyncConsent();

    /**
     * Gets the set of data types that are currently syncing.
     *
     * This is affected by whether sync is on.
     *
     * @return ModelType set of active data types.
     */
    public abstract Set<Integer> getActiveDataTypes();

    /**
     * Gets the set of types that the user has selected.
     *
     * NOTE: This returns "all types" by default, even if the user has never
     *       enabled Sync, or if only Sync-the-transport is running.
     *
     * @return UserSelectableType set of selected types.
     */
    public abstract Set<Integer> getSelectedTypes();

    public abstract boolean hasKeepEverythingSynced();

    public abstract boolean isTypeManagedByPolicy(@UserSelectableType int type);

    /**
     * Enables syncing for the passed types.
     *
     * @param syncEverything Set to true if the user wants to sync all data types
     *                       (including new data types we add in the future).
     * @param enabledTypes   The set of types to enable. Ignored (can be null) if
     *                       syncEverything is true.
     */
    public abstract void setSelectedTypes(boolean syncEverything, Set<Integer> enabledTypes);

    public abstract void setInitialSyncFeatureSetupComplete(int syncFirstSetupCompleteSource);

    public abstract boolean isInitialSyncFeatureSetupComplete();

    public abstract void setSyncRequested();

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
    public abstract SyncSetupInProgressHandle getSetupInProgressHandle();

    public abstract void addSyncStateChangedListener(SyncStateChangedListener listener);

    public abstract void removeSyncStateChangedListener(SyncStateChangedListener listener);

    /**
     * Returns the actual passphrase type being used for encryption. The sync engine must be
     * running (isEngineInitialized() returns true) before calling this function.
     * <p/>
     * This method should only be used if you want to know the raw value. For checking whether
     * we should ask the user for a passphrase, use isPassphraseRequiredForPreferredDataTypes().
     */
    public abstract @PassphraseType int getPassphraseType();

    /**
     * Returns the time the current explicit passphrase was set (if any). Null if no explicit
     * passphrase is in use, or no time is available.
     */
    public abstract @Nullable Date getExplicitPassphraseTime();

    /**
     * Checks if sync is currently set to use a custom passphrase (or the similar -and legacy-
     * frozen implicit passphrase). The sync engine must be running (isEngineInitialized() returns
     * true) before calling this function.
     *
     * @return true if sync is using a custom passphrase.
     */
    public abstract boolean isUsingExplicitPassphrase();

    /**
     * Checks if we need a passphrase to decrypt a currently-enabled data type. This returns false
     * if a passphrase is needed for a type that is not currently enabled.
     *
     * @return true if we need a passphrase.
     */
    public abstract boolean isPassphraseRequiredForPreferredDataTypes();

    /**
     * Checks if trusted vault encryption keys are needed, independently of the currently-enabled
     * data types.
     *
     * @return true if we need an encryption key.
     */
    public abstract boolean isTrustedVaultKeyRequired();

    /**
     * Checks if trusted vault encryption keys are needed to decrypt a currently-enabled data type.
     *
     * @return true if we need an encryption key for a type that is currently enabled.
     */
    public abstract boolean isTrustedVaultKeyRequiredForPreferredDataTypes();

    /**
     * Checks if recoverability of the trusted vault keys is degraded and user action is required,
     * affecting currently enabled data types.
     *
     * @return true if recoverability is degraded.
     */
    public abstract boolean isTrustedVaultRecoverabilityDegraded();

    /**
     * @return Whether setting a custom passphrase is allowed.
     */
    public abstract boolean isCustomPassphraseAllowed();

    /**
     * Checks if the user has chosen to encrypt all data types. Note that some data types (e.g.
     * DEVICE_INFO) are never encrypted.
     *
     * @return true if all data types are encrypted, false if only passwords are encrypted.
     */
    public abstract boolean isEncryptEverythingEnabled();

    public abstract void setEncryptionPassphrase(String passphrase);

    public abstract boolean setDecryptionPassphrase(String passphrase);

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
    public abstract boolean isPassphrasePromptMutedForCurrentProductVersion();

    /**
     * Mutes passphrase error via the android system notifications until the
     * browser is upgraded to a new major version.
     *
     * Can be called whether or not sync is initialized.
     */
    public abstract void markPassphrasePromptMutedForCurrentProductVersion();

    /** @return Whether the user should be offered to opt in to trusted vault encryption. */
    public abstract boolean shouldOfferTrustedVaultOptIn();

    /**
     * @return Whether sync is enabled to sync urls with a non custom passphrase.
     */
    public abstract boolean isSyncingUnencryptedUrls();

    /**
     * Returns the time when the last sync cycle was completed.
     *
     * @return The difference measured in microseconds, between last sync cycle completion time
     * and 1 January 1970 00:00:00 UTC.
     */
    @VisibleForTesting
    public abstract long getLastSyncedTimeForDebugging();

    @VisibleForTesting
    public abstract void triggerRefresh();

    /**
     * Retrieves a JSON version of local Sync data via the native GetAllNodes method.
     * This method is asynchronous; the result will be sent to the callback.
     */
    @VisibleForTesting
    public abstract void getAllNodes(Callback<JSONArray> callback);
}
