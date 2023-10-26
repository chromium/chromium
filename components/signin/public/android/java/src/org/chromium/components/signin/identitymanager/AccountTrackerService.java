// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedDeque;

/**
 * Android wrapper of AccountTrackerService which provides access from the java layer. It offers the
 * capability of fetching and seeding system accounts into AccountTrackerService in C++ layer, and
 * notifies observers when it is complete.
 *
 * <p>TODO(crbug/1176136): Move this class to components/signin/internal
 */
public class AccountTrackerService implements AccountsChangeObserver {
    /**
     * Observers the account seeding.
     */
    public interface Observer {
        /**
         * This method is invoked every time the accounts on device are seeded.
         *
         * @param accountInfos List of all the accounts on device.
         * @param accountsChanged Whether this seeding is triggered by an accounts changed event.
         *     <p>The seeding can be triggered when Chrome starts, user signs in/signs out and when
         *     accounts change. Only the last scenario should trigger this call with {@param
         *     accountsChanged} equal true.
         */
        void legacyOnAccountsSeeded(List<CoreAccountInfo> accountInfos, boolean accountsChanged);
    }

    private static final String TAG = "AccountService";

    @IntDef({
            AccountsSeedingStatus.NOT_STARTED,
            AccountsSeedingStatus.IN_PROGRESS,
            AccountsSeedingStatus.DONE,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface AccountsSeedingStatus {
        int NOT_STARTED = 0;
        int IN_PROGRESS = 1;
        int DONE = 2;
    }

    private final long mNativeAccountTrackerService;
    private final Queue<Runnable> mRunnablesWaitingForAccountsSeeding;
    private @AccountsSeedingStatus int mAccountsSeedingStatus;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final AccountManagerFacade mAccountManagerFacade;

    @VisibleForTesting
    @CalledByNative
    AccountTrackerService(long nativeAccountTrackerService) {
        mNativeAccountTrackerService = nativeAccountTrackerService;
        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
        mRunnablesWaitingForAccountsSeeding = new ConcurrentLinkedDeque<>();
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
    }

    @VisibleForTesting
    @CalledByNative
    void destroy() {
        mAccountManagerFacade.removeObserver(this);
    }

    /**
     * Adds an observer to observe the accounts seeding changes.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to observe the accounts seeding changes.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Seeds the accounts only if they are not seeded yet. The given runnable will run after the
     * accounts are seeded. If the accounts are already seeded, the runnable will be executed
     * immediately.
     */
    @MainThread
    public void legacySeedAccountsIfNeeded(Runnable onAccountsSeeded) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SeedAccountsRevamp is enabled");
        }
        ThreadUtils.checkUiThread();
        switch (mAccountsSeedingStatus) {
            case AccountsSeedingStatus.NOT_STARTED:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                legacySeedAccounts(/* accountsChanged= */ false);
                break;
            case AccountsSeedingStatus.IN_PROGRESS:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                break;
            case AccountsSeedingStatus.DONE:
                onAccountsSeeded.run();
                break;
        }
    }

    /** Implements {@link AccountsChangeObserver}. */
    // TODO(crbug/1491005): Move the AccountInfoChanged logic to the SigninManager.
    @Override
    public void onCoreAccountInfosChanged() {
        // If mAccountsSeedingStatus is IN_PROGRESS do nothing. The promise in seedAccounts() will
        // be fulfilled with updated list of CoreAccountInfo's.
        if (mAccountsSeedingStatus != AccountsSeedingStatus.IN_PROGRESS) {
            mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
            legacySeedAccounts(true);
        }
    }

    @MainThread
    void invalidateAccountsSeedingStatus() {
        // If mAccountsSeedingStatus is IN_PROGRESS do nothing. The old invalidated seeding status
        // will be overwritten by the new seeding process.
        if (mAccountsSeedingStatus != AccountsSeedingStatus.IN_PROGRESS) {
            mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
            legacySeedAccounts(false);
        }
    }

    /**
     * Seeds the accounts on device.
     *
     * @param accountsChanged Whether this seeding is triggered by an accounts changed event.
     *     <p>The seeding can be triggered when Chrome starts, user signs in/signs out and when
     *     accounts change. Only the last scenario should trigger this call with {@param
     *     accountsChanged} equal true. When Chrome starts, we should trigger seedAccounts(false)
     *     because the accounts are already loaded in PO2TS in this flow. accountsChanged=false will
     *     avoid it to get triggered again from SigninChecker.
     */
    private void legacySeedAccounts(boolean accountsChanged) {
        ThreadUtils.checkUiThread();
        // The revamped signin flow will not seed accounts here.
        if (SigninFeatureMap.isEnabled(SigninFeatures.SEED_ACCOUNTS_REVAMP)) {
            throw new IllegalStateException(
                    "This method should never be called when SeedAccountsRevamp is enabled");
        }
        assert mAccountsSeedingStatus != AccountsSeedingStatus.IN_PROGRESS
                : "There is already a seeding in progress!";
        mAccountsSeedingStatus = AccountsSeedingStatus.IN_PROGRESS;

        Promise<List<CoreAccountInfo>> coreAccountInfosPromise =
                mAccountManagerFacade.getCoreAccountInfos();
        if (coreAccountInfosPromise.isFulfilled()) {
            finishSeedingAccounts(coreAccountInfosPromise.getResult(), accountsChanged);
        } else {
            coreAccountInfosPromise.then(
                    coreAccountInfos -> {
                        finishSeedingAccounts(coreAccountInfos, accountsChanged);
                    });
        }
    }

    private void finishSeedingAccounts(
            List<CoreAccountInfo> coreAccountInfos, boolean accountsChanged) {
        ThreadUtils.checkUiThread();
        AccountTrackerServiceJni.get()
                .legacySeedAccountsInfo(
                        mNativeAccountTrackerService,
                        coreAccountInfos.toArray(new CoreAccountInfo[0]));

        mAccountsSeedingStatus = AccountsSeedingStatus.DONE;

        for (@Nullable Runnable runnable = mRunnablesWaitingForAccountsSeeding.poll();
                runnable != null; runnable = mRunnablesWaitingForAccountsSeeding.poll()) {
            runnable.run();
        }

        for (Observer observer : mObservers) {
            observer.legacyOnAccountsSeeded(coreAccountInfos, accountsChanged);
        }
    }

    @NativeMethods
    interface Natives {
        void legacySeedAccountsInfo(
                long nativeAccountTrackerService, CoreAccountInfo[] coreAccountInfos);
    }
}
