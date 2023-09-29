// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * JNI wrapper for the native SyncServiceImpl.
 *
 * This class mostly makes calls to native and contains a minimum of business logic. It is only
 * usable from the UI thread as the native SyncServiceImpl requires its access to be on the
 * UI thread. See components/sync/service/sync_service_impl.h for more details.
 * TODO(crbug.com/1451811): Update to no reference UI thread.
 */
public class SyncServiceImpl extends SyncService {
    private final long mSyncServiceAndroidBridge;

    private int mSetupInProgressCounter;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    /**
     * UserSelectableTypes that the user can directly select in settings.
     * This is a subset of the native UserSelectableTypeSet.
     */
    private static final int[] ALL_SELECTABLE_TYPES = new int[] {UserSelectableType.AUTOFILL,
            UserSelectableType.PAYMENTS, UserSelectableType.BOOKMARKS, UserSelectableType.PASSWORDS,
            UserSelectableType.PREFERENCES, UserSelectableType.TABS, UserSelectableType.HISTORY};

    @CalledByNative
    private SyncServiceImpl(long ptr) {
        ThreadUtils.assertOnUiThread();
        assert ptr != 0;
        mSyncServiceAndroidBridge = ptr;
    }

    @Override
    public boolean isEngineInitialized() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isEngineInitialized(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTransportStateActive() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isTransportStateActive(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean canSyncFeatureStart() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().canSyncFeatureStart(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncFeatureEnabled() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isSyncFeatureEnabled(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncFeatureActive() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isSyncFeatureActive(mSyncServiceAndroidBridge);
    }

    @Override
    public @GoogleServiceAuthError.State int getAuthError() {
        ThreadUtils.assertOnUiThread();
        int authErrorCode = SyncServiceImplJni.get().getAuthError(mSyncServiceAndroidBridge);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    @Override
    public boolean isSyncDisabledByEnterprisePolicy() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isSyncDisabledByEnterprisePolicy(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean hasUnrecoverableError() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().hasUnrecoverableError(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean requiresClientUpgrade() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().requiresClientUpgrade(mSyncServiceAndroidBridge);
    }

    @Override
    public @Nullable CoreAccountInfo getAccountInfo() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().getAccountInfo(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean hasSyncConsent() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().hasSyncConsent(mSyncServiceAndroidBridge);
    }

    @Override
    public Set<Integer> getActiveDataTypes() {
        ThreadUtils.assertOnUiThread();
        int[] activeDataTypes =
                SyncServiceImplJni.get().getActiveDataTypes(mSyncServiceAndroidBridge);
        return modelTypeArrayToSet(activeDataTypes);
    }

    @Override
    public Set<Integer> getSelectedTypes() {
        ThreadUtils.assertOnUiThread();
        int[] userSelectableTypeArray =
                SyncServiceImplJni.get().getSelectedTypes(mSyncServiceAndroidBridge);
        return userSelectableTypeArrayToSet(userSelectableTypeArray);
    }

    @Override
    public boolean isTypeManagedByPolicy(@UserSelectableType int type) {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isTypeManagedByPolicy(mSyncServiceAndroidBridge, type);
    }

    @Override
    public boolean isTypeManagedByCustodian(@UserSelectableType int type) {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isTypeManagedByCustodian(mSyncServiceAndroidBridge, type);
    }

    @Override
    public boolean hasKeepEverythingSynced() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().hasKeepEverythingSynced(mSyncServiceAndroidBridge);
    }

    @Override
    public void setSelectedTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().setSelectedTypes(mSyncServiceAndroidBridge, syncEverything,
                syncEverything ? ALL_SELECTABLE_TYPES : userSelectableTypeSetToArray(enabledTypes));
    }

    @Override
    public void setInitialSyncFeatureSetupComplete(int syncFirstSetupCompleteSource) {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().setInitialSyncFeatureSetupComplete(
                mSyncServiceAndroidBridge, syncFirstSetupCompleteSource);
    }

    @Override
    public boolean isInitialSyncFeatureSetupComplete() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isInitialSyncFeatureSetupComplete(
                mSyncServiceAndroidBridge);
    }

    @Override
    public void setSyncRequested() {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().setSyncRequested(mSyncServiceAndroidBridge);
    }

    @Override
    public SyncSetupInProgressHandle getSetupInProgressHandle() {
        ThreadUtils.assertOnUiThread();
        if (++mSetupInProgressCounter == 1) {
            setSetupInProgress(true);
        }

        return new SyncSetupInProgressHandle() {
            private boolean mClosed;

            @Override
            public void close() {
                ThreadUtils.assertOnUiThread();
                if (mClosed) return;
                mClosed = true;

                assert mSetupInProgressCounter > 0;
                if (--mSetupInProgressCounter == 0) {
                    setSetupInProgress(false);
                }
            }
        };
    }

    private void setSetupInProgress(boolean inProgress) {
        SyncServiceImplJni.get().setSetupInProgress(mSyncServiceAndroidBridge, inProgress);
    }

    @Override
    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.add(listener);
    }

    @Override
    public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.remove(listener);
    }

    /**
     * Called when the state of the native sync engine has changed, so various
     * UI elements can update themselves.
     */
    @CalledByNative
    public void syncStateChanged() {
        for (SyncStateChangedListener listener : mListeners) {
            listener.syncStateChanged();
        }
    }

    @Override
    public @PassphraseType int getPassphraseType() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        int passphraseType = SyncServiceImplJni.get().getPassphraseType(mSyncServiceAndroidBridge);
        if (passphraseType < 0 || passphraseType > PassphraseType.MAX_VALUE) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    @Override
    public @Nullable Date getExplicitPassphraseTime() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        long timeInMilliseconds =
                SyncServiceImplJni.get().getExplicitPassphraseTime(mSyncServiceAndroidBridge);
        return timeInMilliseconds != 0 ? new Date(timeInMilliseconds) : null;
    }

    @Override
    public boolean isUsingExplicitPassphrase() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isUsingExplicitPassphrase(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isPassphraseRequiredForPreferredDataTypes(
                mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultKeyRequired() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultKeyRequired(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultKeyRequiredForPreferredDataTypes(
                mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultRecoverabilityDegraded() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultRecoverabilityDegraded(
                mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isCustomPassphraseAllowed() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isCustomPassphraseAllowed(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isEncryptEverythingEnabled(mSyncServiceAndroidBridge);
    }

    @Override
    public void setEncryptionPassphrase(String passphrase) {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        SyncServiceImplJni.get().setEncryptionPassphrase(mSyncServiceAndroidBridge, passphrase);
    }

    @Override
    public boolean setDecryptionPassphrase(String passphrase) {
        ThreadUtils.assertOnUiThread();
        assert isEngineInitialized();
        return SyncServiceImplJni.get().setDecryptionPassphrase(
                mSyncServiceAndroidBridge, passphrase);
    }

    @Override
    public boolean isPassphrasePromptMutedForCurrentProductVersion() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().isPassphrasePromptMutedForCurrentProductVersion(
                mSyncServiceAndroidBridge);
    }

    @Override
    public void markPassphrasePromptMutedForCurrentProductVersion() {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().markPassphrasePromptMutedForCurrentProductVersion(
                mSyncServiceAndroidBridge);
    }

    @Override
    public boolean shouldOfferTrustedVaultOptIn() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().shouldOfferTrustedVaultOptIn(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncingUnencryptedUrls() {
        return isEngineInitialized() && getActiveDataTypes().contains(ModelType.HISTORY)
                && (getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE
                        || getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE);
    }

    @VisibleForTesting
    @Override
    public long getLastSyncedTimeForDebugging() {
        ThreadUtils.assertOnUiThread();
        return SyncServiceImplJni.get().getLastSyncedTimeForDebugging(mSyncServiceAndroidBridge);
    }

    @VisibleForTesting
    @Override
    public void triggerRefresh() {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().triggerRefresh(mSyncServiceAndroidBridge);
    }

    /**
     * Invokes the onResult method of the callback from native code.
     */
    @CalledByNative
    private static void onGetAllNodesResult(Callback<JSONArray> callback, String serializedNodes) {
        try {
            callback.onResult(new JSONArray(serializedNodes));
        } catch (JSONException e) {
            callback.onResult(new JSONArray());
        }
    }

    @VisibleForTesting
    @Override
    public void getAllNodes(Callback<JSONArray> callback) {
        ThreadUtils.assertOnUiThread();
        SyncServiceImplJni.get().getAllNodes(mSyncServiceAndroidBridge, callback);
    }

    private static Set<Integer> modelTypeArrayToSet(int[] modelTypeArray) {
        Set<Integer> modelTypeSet = new HashSet<Integer>();
        for (int i = 0; i < modelTypeArray.length; i++) {
            modelTypeSet.add(modelTypeArray[i]);
        }
        return modelTypeSet;
    }

    private static Set<Integer> userSelectableTypeArrayToSet(int[] userSelectableTypeArray) {
        Set<Integer> userSelectableTypeSet = new HashSet<Integer>();
        for (int i = 0; i < userSelectableTypeArray.length; i++) {
            userSelectableTypeSet.add(userSelectableTypeArray[i]);
        }
        return userSelectableTypeSet;
    }

    private static int[] userSelectableTypeSetToArray(Set<Integer> userSelectableTypeSet) {
        int[] userSelectableTypeArray = new int[userSelectableTypeSet.size()];
        int i = 0;
        for (int userSelectableType : userSelectableTypeSet) {
            userSelectableTypeArray[i++] = userSelectableType;
        }
        return userSelectableTypeArray;
    }

    @NativeMethods
    interface Natives {
        // Please keep all methods below in the same order as sync_service_android_bridge.h.
        void setSyncRequested(long nativeSyncServiceAndroidBridge);
        boolean canSyncFeatureStart(long nativeSyncServiceAndroidBridge);
        boolean isSyncFeatureEnabled(long nativeSyncServiceAndroidBridge);
        boolean isSyncFeatureActive(long nativeSyncServiceAndroidBridge);
        boolean isSyncDisabledByEnterprisePolicy(long nativeSyncServiceAndroidBridge);
        boolean isEngineInitialized(long nativeSyncServiceAndroidBridge);
        boolean isTransportStateActive(long nativeSyncServiceAndroidBridge);
        void setSetupInProgress(long nativeSyncServiceAndroidBridge, boolean inProgress);
        boolean isInitialSyncFeatureSetupComplete(long nativeSyncServiceAndroidBridge);
        void setInitialSyncFeatureSetupComplete(
                long nativeSyncServiceAndroidBridge, int syncFirstSetupCompleteSource);
        int[] getActiveDataTypes(long nativeSyncServiceAndroidBridge);
        int[] getSelectedTypes(long nativeSyncServiceAndroidBridge);
        boolean isTypeManagedByPolicy(long nativeSyncServiceAndroidBridge, int type);
        boolean isTypeManagedByCustodian(long nativeSyncServiceAndroidBridge, int type);
        void setSelectedTypes(long nativeSyncServiceAndroidBridge, boolean syncEverything,
                int[] userSelectableTypeArray);
        boolean isCustomPassphraseAllowed(long nativeSyncServiceAndroidBridge);
        boolean isEncryptEverythingEnabled(long nativeSyncServiceAndroidBridge);
        boolean isPassphraseRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultKeyRequired(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultKeyRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultRecoverabilityDegraded(long nativeSyncServiceAndroidBridge);
        boolean isUsingExplicitPassphrase(long nativeSyncServiceAndroidBridge);
        int getPassphraseType(long nativeSyncServiceAndroidBridge);
        void setEncryptionPassphrase(long nativeSyncServiceAndroidBridge, String passphrase);
        boolean setDecryptionPassphrase(long nativeSyncServiceAndroidBridge, String passphrase);
        long getExplicitPassphraseTime(long nativeSyncServiceAndroidBridge);
        void getAllNodes(long nativeSyncServiceAndroidBridge, Callback<JSONArray> callback);
        int getAuthError(long nativeSyncServiceAndroidBridge);
        boolean hasUnrecoverableError(long nativeSyncServiceAndroidBridge);
        boolean requiresClientUpgrade(long nativeSyncServiceAndroidBridge);
        @Nullable
        CoreAccountInfo getAccountInfo(long nativeSyncServiceAndroidBridge);
        boolean hasSyncConsent(long nativeSyncServiceAndroidBridge);
        boolean isPassphrasePromptMutedForCurrentProductVersion(
                long nativeSyncServiceAndroidBridge);
        void markPassphrasePromptMutedForCurrentProductVersion(long nativeSyncServiceAndroidBridge);
        boolean hasKeepEverythingSynced(long nativeSyncServiceAndroidBridge);
        boolean shouldOfferTrustedVaultOptIn(long nativeSyncServiceAndroidBridge);
        void triggerRefresh(long nativeSyncServiceAndroidBridge);
        long getLastSyncedTimeForDebugging(long nativeSyncServiceAndroidBridge);
    }
}
