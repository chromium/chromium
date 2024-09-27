// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * JNI wrapper for the native SyncServiceImpl.
 *
 * <p>This class mostly makes calls to native and contains a minimum of business logic. It is only
 * usable from the same sequence as the native SyncServiceImpl. See
 * components/sync/service/sync_service_impl.h for more details.
 */
@JNINamespace("syncer")
public class SyncServiceImpl implements SyncService, AccountsChangeObserver {
    // Pointer to the C++ counterpart object. Set on construction and reset on destroy() to avoid
    // a dangling pointer.
    private long mSyncServiceAndroidBridge;

    private int mSetupInProgressCounter;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    private final ThreadChecker mThreadChecker = new ThreadChecker();

    @CalledByNative
    private SyncServiceImpl(long ptr) {
        mThreadChecker.assertOnValidThread();
        assert ptr != 0;
        mSyncServiceAndroidBridge = ptr;
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        accountManagerFacade.addObserver(this);
        Promise<List<CoreAccountInfo>> accountsPromise =
                AccountManagerFacadeProvider.getInstance().getCoreAccountInfos();
        if (accountsPromise.isFulfilled()) {
            // The promise is already fulfilled - call immediately. If the promise is not fulfilled,
            // `keepSettingsOnlyForAccountManagerAccounts` will be invoked by
            // `onCoreAccountInfosChanged` when `AccountManagerFacade` cache gets populated.
            keepSettingsOnlyForAccountManagerAccounts(accountsPromise.getResult());
        }
    }

    /** Signals the native SyncService is being shutdown and this object mustn't be used anymore. */
    @CalledByNative
    private void destroy() {
        AccountManagerFacadeProvider.getInstance().removeObserver(this);
        mSyncServiceAndroidBridge = 0;
    }

    @Override
    public boolean isEngineInitialized() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isEngineInitialized(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncFeatureEnabled() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isSyncFeatureEnabled(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncFeatureActive() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isSyncFeatureActive(mSyncServiceAndroidBridge);
    }

    @Override
    public @GoogleServiceAuthError.State int getAuthError() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        int authErrorCode = SyncServiceImplJni.get().getAuthError(mSyncServiceAndroidBridge);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    @Override
    public boolean isSyncDisabledByEnterprisePolicy() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isSyncDisabledByEnterprisePolicy(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean hasUnrecoverableError() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().hasUnrecoverableError(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean requiresClientUpgrade() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().requiresClientUpgrade(mSyncServiceAndroidBridge);
    }

    @Override
    public @Nullable CoreAccountInfo getAccountInfo() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().getAccountInfo(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean hasSyncConsent() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().hasSyncConsent(mSyncServiceAndroidBridge);
    }

    @Override
    public Set<Integer> getActiveDataTypes() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        int[] activeDataTypes =
                SyncServiceImplJni.get().getActiveDataTypes(mSyncServiceAndroidBridge);
        return dataTypeArrayToSet(activeDataTypes);
    }

    @Override
    public Set<Integer> getSelectedTypes() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        int[] userSelectableTypeArray =
                SyncServiceImplJni.get().getSelectedTypes(mSyncServiceAndroidBridge);
        return userSelectableTypeArrayToSet(userSelectableTypeArray);
    }

    @Override
    public void getTypesWithUnsyncedData(Callback<Set<Integer>> callback) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().getTypesWithUnsyncedData(mSyncServiceAndroidBridge, callback);
    }

    @Override
    public void getLocalDataDescriptions(
            Set<Integer> types, Callback<HashMap<Integer, LocalDataDescription>> callback) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get()
                .getLocalDataDescriptions(
                        mSyncServiceAndroidBridge, userSelectableTypeSetToArray(types), callback);
    }

    @Override
    public void triggerLocalDataMigration(Set<Integer> types) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get()
                .triggerLocalDataMigration(
                        mSyncServiceAndroidBridge, userSelectableTypeSetToArray(types));
    }

    @Override
    public boolean isTypeManagedByPolicy(@UserSelectableType int type) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isTypeManagedByPolicy(mSyncServiceAndroidBridge, type);
    }

    @Override
    public boolean isTypeManagedByCustodian(@UserSelectableType int type) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().isTypeManagedByCustodian(mSyncServiceAndroidBridge, type);
    }

    @Override
    public boolean hasKeepEverythingSynced() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().hasKeepEverythingSynced(mSyncServiceAndroidBridge);
    }

    @Override
    public void setSelectedTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get()
                .setSelectedTypes(
                        mSyncServiceAndroidBridge,
                        syncEverything,
                        userSelectableTypeSetToArray(enabledTypes));
    }

    @Override
    public void setSelectedType(@UserSelectableType int type, boolean isTypeOn) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().setSelectedType(mSyncServiceAndroidBridge, type, isTypeOn);
    }

    @Override
    public void setInitialSyncFeatureSetupComplete(int syncFirstSetupCompleteSource) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get()
                .setInitialSyncFeatureSetupComplete(
                        mSyncServiceAndroidBridge, syncFirstSetupCompleteSource);
    }

    @Override
    public boolean isInitialSyncFeatureSetupComplete() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get()
                .isInitialSyncFeatureSetupComplete(mSyncServiceAndroidBridge);
    }

    @Override
    public void setSyncRequested() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().setSyncRequested(mSyncServiceAndroidBridge);
    }

    @Override
    public SyncSetupInProgressHandle getSetupInProgressHandle() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        if (++mSetupInProgressCounter == 1) {
            setSetupInProgress(true);
        }

        return new SyncSetupInProgressHandle() {
            private boolean mClosed;

            @Override
            public void close() {
                mThreadChecker.assertOnValidThread();
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
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().setSetupInProgress(mSyncServiceAndroidBridge, inProgress);
    }

    @Override
    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        mListeners.add(listener);
    }

    @Override
    public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        mListeners.remove(listener);
    }

    /**
     * Called when the state of the native sync engine has changed, so various UI elements can
     * update themselves.
     */
    @CalledByNative
    public void syncStateChanged() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        for (SyncStateChangedListener listener : mListeners) {
            listener.syncStateChanged();
        }
    }

    @Override
    public @PassphraseType int getPassphraseType() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        int passphraseType = SyncServiceImplJni.get().getPassphraseType(mSyncServiceAndroidBridge);
        if (passphraseType < 0 || passphraseType > PassphraseType.MAX_VALUE) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    @Override
    public @TransportState int getTransportState() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().getTransportState(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isUsingExplicitPassphrase() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isUsingExplicitPassphrase(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get()
                .isPassphraseRequiredForPreferredDataTypes(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultKeyRequired() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultKeyRequired(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get()
                .isTrustedVaultKeyRequiredForPreferredDataTypes(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isTrustedVaultRecoverabilityDegraded() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get()
                .isTrustedVaultRecoverabilityDegraded(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isCustomPassphraseAllowed() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isCustomPassphraseAllowed(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isEncryptEverythingEnabled(mSyncServiceAndroidBridge);
    }

    @Override
    public void setEncryptionPassphrase(String passphrase) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        SyncServiceImplJni.get().setEncryptionPassphrase(mSyncServiceAndroidBridge, passphrase);
    }

    @Override
    public boolean setDecryptionPassphrase(String passphrase) {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        assert isEngineInitialized();
        return SyncServiceImplJni.get()
                .setDecryptionPassphrase(mSyncServiceAndroidBridge, passphrase);
    }

    @Override
    public boolean isPassphrasePromptMutedForCurrentProductVersion() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get()
                .isPassphrasePromptMutedForCurrentProductVersion(mSyncServiceAndroidBridge);
    }

    @Override
    public void markPassphrasePromptMutedForCurrentProductVersion() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get()
                .markPassphrasePromptMutedForCurrentProductVersion(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean shouldOfferTrustedVaultOptIn() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().shouldOfferTrustedVaultOptIn(mSyncServiceAndroidBridge);
    }

    @Override
    public boolean isSyncingUnencryptedUrls() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return isEngineInitialized()
                && getActiveDataTypes().contains(DataType.HISTORY)
                && (getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE
                        || getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE);
    }

    @VisibleForTesting
    @Override
    public long getLastSyncedTimeForDebugging() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        return SyncServiceImplJni.get().getLastSyncedTimeForDebugging(mSyncServiceAndroidBridge);
    }

    @VisibleForTesting
    @Override
    public void triggerRefresh() {
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().triggerRefresh(mSyncServiceAndroidBridge);
    }

    @Override
    /* AccountsChangeObserver implementation. */
    public void onCoreAccountInfosChanged() {
        Promise<List<CoreAccountInfo>> accountsPromise =
                AccountManagerFacadeProvider.getInstance().getCoreAccountInfos();
        assert accountsPromise.isFulfilled();
        keepSettingsOnlyForAccountManagerAccounts(accountsPromise.getResult());
    }

    private void keepSettingsOnlyForAccountManagerAccounts(List<CoreAccountInfo> accounts) {
        String[] gaiaIds = accounts.stream().map(CoreAccountInfo::getGaiaId).toArray(String[]::new);
        SyncServiceImplJni.get()
                .keepAccountSettingsPrefsOnlyForUsers(mSyncServiceAndroidBridge, gaiaIds);
    }

    @CalledByNative
    private static void onGetTypesWithUnsyncedDataResult(
            Callback<Set<Integer>> callback, int[] types) {
        callback.onResult(dataTypeArrayToSet(types));
    }

    @CalledByNative
    private static void onGetLocalDataDescriptionsResult(
            Callback<HashMap<Integer, LocalDataDescription>> callback,
            int[] dataTypes,
            LocalDataDescription[] localDataDescriptions) {
        HashMap<Integer, LocalDataDescription> localDataDescription =
                new HashMap<Integer, LocalDataDescription>();
        for (int i = 0; i < dataTypes.length; i++) {
            localDataDescription.put(dataTypes[i], localDataDescriptions[i]);
        }
        callback.onResult(localDataDescription);
    }

    /** Invokes the onResult method of the callback from native code. */
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
        mThreadChecker.assertOnValidThread();
        assert mSyncServiceAndroidBridge != 0;
        SyncServiceImplJni.get().getAllNodes(mSyncServiceAndroidBridge, callback);
    }

    private static Set<Integer> dataTypeArrayToSet(int[] dataTypeArray) {
        Set<Integer> dataTypeSet = new HashSet<Integer>();
        for (int i = 0; i < dataTypeArray.length; i++) {
            dataTypeSet.add(dataTypeArray[i]);
        }
        return dataTypeSet;
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

    @Override
    public long getNativeSyncServiceAndroidBridge() {
        return mSyncServiceAndroidBridge;
    }

    @NativeMethods
    interface Natives {
        // Please keep all methods below in the same order as sync_service_android_bridge.h.
        void setSyncRequested(long nativeSyncServiceAndroidBridge);

        boolean isSyncFeatureEnabled(long nativeSyncServiceAndroidBridge);

        boolean isSyncFeatureActive(long nativeSyncServiceAndroidBridge);

        boolean isSyncDisabledByEnterprisePolicy(long nativeSyncServiceAndroidBridge);

        boolean isEngineInitialized(long nativeSyncServiceAndroidBridge);

        void setSetupInProgress(long nativeSyncServiceAndroidBridge, boolean inProgress);

        boolean isInitialSyncFeatureSetupComplete(long nativeSyncServiceAndroidBridge);

        void setInitialSyncFeatureSetupComplete(
                long nativeSyncServiceAndroidBridge, int syncFirstSetupCompleteSource);

        int[] getActiveDataTypes(long nativeSyncServiceAndroidBridge);

        int[] getSelectedTypes(long nativeSyncServiceAndroidBridge);

        void getTypesWithUnsyncedData(
                long nativeSyncServiceAndroidBridge, Callback<Set<Integer>> callback);

        void getLocalDataDescriptions(
                long nativeSyncServiceAndroidBridge,
                int[] types,
                Callback<HashMap<Integer, LocalDataDescription>> callback);

        void triggerLocalDataMigration(long nativeSyncServiceAndroidBridge, int[] types);

        boolean isTypeManagedByPolicy(long nativeSyncServiceAndroidBridge, int type);

        boolean isTypeManagedByCustodian(long nativeSyncServiceAndroidBridge, int type);

        void setSelectedTypes(
                long nativeSyncServiceAndroidBridge,
                boolean syncEverything,
                int[] userSelectableTypeArray);

        void setSelectedType(
                long nativeSyncServiceAndroidBridge,
                @UserSelectableType int type,
                boolean isTypeOn);

        boolean isCustomPassphraseAllowed(long nativeSyncServiceAndroidBridge);

        boolean isEncryptEverythingEnabled(long nativeSyncServiceAndroidBridge);

        boolean isPassphraseRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);

        boolean isTrustedVaultKeyRequired(long nativeSyncServiceAndroidBridge);

        boolean isTrustedVaultKeyRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);

        boolean isTrustedVaultRecoverabilityDegraded(long nativeSyncServiceAndroidBridge);

        boolean isUsingExplicitPassphrase(long nativeSyncServiceAndroidBridge);

        int getPassphraseType(long nativeSyncServiceAndroidBridge);

        int getTransportState(long nativeSyncServiceAndroidBridge);

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

        void keepAccountSettingsPrefsOnlyForUsers(
                long nativeSyncServiceAndroidBridge, String[] gaiaIds);
    }
}
