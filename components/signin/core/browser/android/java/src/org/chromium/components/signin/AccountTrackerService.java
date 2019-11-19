// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Android wrapper of AccountTrackerService which provides access from the java layer.
 * It offers the capability of fetching and seeding system accounts into AccountTrackerService in
 * C++ layer, and notifies observers when it is complete.
 */
public class AccountTrackerService {
    private static final String TAG = "AccountService";

    private final long mNativeAccountTrackerService;
    private @SystemAccountsSeedingStatus int mSystemAccountsSeedingStatus;
    private boolean mSystemAccountsChanged;
    private boolean mSyncForceRefreshedForTest;
    private AccountsChangeObserver mAccountsChangeObserver;

    @IntDef({SystemAccountsSeedingStatus.SEEDING_NOT_STARTED,
            SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS,
            SystemAccountsSeedingStatus.SEEDING_DONE,
            SystemAccountsSeedingStatus.SEEDING_VALIDATING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SystemAccountsSeedingStatus {
        int SEEDING_NOT_STARTED = 0;
        int SEEDING_IN_PROGRESS = 1;
        int SEEDING_DONE = 2;
        int SEEDING_VALIDATING = 3;
    }

    /**
     * Classes that want to listen for system accounts fetching and seeding should implement
     * this interface and register with {@link #addSystemAccountsSeededListener}.
     */
    public interface OnSystemAccountsSeededListener {
        // Called at the end of seedSystemAccounts().
        void onSystemAccountsSeedingComplete();
        // Called in invalidateAccountSeedStatus() indicating that accounts have changed.
        default void onSystemAccountsChanged() {}
    }

    private final ObserverList<OnSystemAccountsSeededListener> mSystemAccountsSeedingObservers =
            new ObserverList<>();

    private AccountTrackerService(long nativeAccountTrackerService) {
        mNativeAccountTrackerService = nativeAccountTrackerService;
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
        mSystemAccountsChanged = false;
    }

    @CalledByNative
    private static AccountTrackerService create(long nativeAccountTrackerService) {
        ThreadUtils.assertOnUiThread();
        return new AccountTrackerService(nativeAccountTrackerService);
    }

    /**
     * Checks whether the account id <-> email mapping has been seeded into C++ layer.
     * If not, it automatically starts fetching the mapping and seeds it.
     * @return Whether the accounts have been seeded already.
     */
    public boolean checkAndSeedSystemAccounts() {
        ThreadUtils.assertOnUiThread();
        if (areSystemAccountsSeeded()) {
            return true;
        }
        if ((mSystemAccountsSeedingStatus == SystemAccountsSeedingStatus.SEEDING_NOT_STARTED
                    || mSystemAccountsChanged)
                && mSystemAccountsSeedingStatus
                        != SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS) {
            seedSystemAccounts();
        }
        return false;
    }

    /**
     * Checks whether system accounts are seeded without changing the state.
     * @return Whether account list in {@link AccountManagerFacade} is consistent with accounts in
     *         the native AccountTrackerService.
     */
    boolean areSystemAccountsSeeded() {
        return mSystemAccountsSeedingStatus == SystemAccountsSeedingStatus.SEEDING_DONE
                && !mSystemAccountsChanged;
    }

    /**
     * Register an |observer| to observe system accounts seeding status.
     */
    public void addSystemAccountsSeededListener(OnSystemAccountsSeededListener observer) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingObservers.addObserver(observer);
    }

    /**
     * Remove an |observer| from the list of observers.
     */
    public void removeSystemAccountsSeededListener(OnSystemAccountsSeededListener observer) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingObservers.removeObserver(observer);
    }

    private void seedSystemAccounts() {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsChanged = false;
        mSyncForceRefreshedForTest = false;

        final AccountIdProvider accountIdProvider = AccountIdProvider.getInstance();
        if (accountIdProvider.canBeUsed()) {
            mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS;
        } else {
            mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
            return;
        }

        if (mAccountsChangeObserver == null) {
            mAccountsChangeObserver =
                    () -> invalidateAccountSeedStatus(false /* don't reseed right now */);
            AccountManagerFacade.get().addObserver(mAccountsChangeObserver);
        }

        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            new AsyncTask<String[][]>() {
                @Override
                public String[][] doInBackground() {
                    Log.d(TAG, "Getting id/email mapping");

                    long seedingStartTime = SystemClock.elapsedRealtime();

                    String[][] accountIdNameMap = new String[2][accounts.size()];
                    for (int i = 0; i < accounts.size(); ++i) {
                        accountIdNameMap[0][i] =
                                accountIdProvider.getAccountId(accounts.get(i).name);
                        accountIdNameMap[1][i] = accounts.get(i).name;
                    }

                    RecordHistogram.recordTimesHistogram("Signin.AndroidGetAccountIdsTime",
                            SystemClock.elapsedRealtime() - seedingStartTime);

                    return accountIdNameMap;
                }
                @Override
                public void onPostExecute(String[][] accountIdNameMap) {
                    if (mSyncForceRefreshedForTest) return;
                    if (mSystemAccountsChanged) {
                        seedSystemAccounts();
                        return;
                    }
                    if (areAccountIdsValid(accountIdNameMap[0])) {
                        AccountTrackerServiceJni.get().seedAccountsInfo(
                                mNativeAccountTrackerService, accountIdNameMap[0],
                                accountIdNameMap[1]);
                        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
                        notifyObserversOnSeedingComplete();
                    } else {
                        Log.w(TAG, "Invalid mapping of id/email");
                        seedSystemAccounts();
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        });
    }

    private boolean areAccountIdsValid(String[] accountIds) {
        for (String accountId : accountIds) {
            if (accountId == null) return false;
        }
        return true;
    }

    private void notifyObserversOnSeedingComplete() {
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsSeedingComplete();
        }
    }

    /**
     * Seed system accounts into AccountTrackerService synchronously for test purpose.
     */
    @VisibleForTesting
    public void syncForceRefreshForTest(String[] accountIds, String[] accountNames) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS;
        mSystemAccountsChanged = false;
        mSyncForceRefreshedForTest = true;
        AccountTrackerServiceJni.get().seedAccountsInfo(
                mNativeAccountTrackerService, accountIds, accountNames);
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
    }

    /**
     * Notifies the AccountTrackerService about changed system accounts. without actually triggering
     * @param reSeedAccounts Whether to also start seeding the new account information immediately.
     */
    public void invalidateAccountSeedStatus(boolean reSeedAccounts) {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsChanged = true;
        notifyObserversOnAccountsChange();
        if (reSeedAccounts) checkAndSeedSystemAccounts();
    }

    /**
     * Verifies whether seeded accounts in AccountTrackerService are up-to-date with the accounts in
     * Android. It sets seeding status to SEEDING_VALIDATING temporarily to block services depending
     * on it and sets it back to SEEDING_DONE after passing the verification. This function is
     * created because accounts changed notification from Android to Chrome has latency.
     */
    public void validateSystemAccounts() {
        ThreadUtils.assertOnUiThread();
        if (!checkAndSeedSystemAccounts()) {
            // Do nothing if seeding is not done.
            return;
        }

        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_VALIDATING;
        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            if (mSystemAccountsChanged
                    || mSystemAccountsSeedingStatus
                            != SystemAccountsSeedingStatus.SEEDING_VALIDATING) {
                return;
            }

            String[] accountNames = new String[accounts.size()];
            for (int i = 0; i < accounts.size(); ++i) {
                accountNames[i] = accounts.get(i).name;
            }
            if (AccountTrackerServiceJni.get().areAccountsSeeded(
                        mNativeAccountTrackerService, accountNames)) {
                mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
                notifyObserversOnSeedingComplete();
            }
        });
    }

    private void notifyObserversOnAccountsChange() {
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsChanged();
        }
    }

    @NativeMethods
    interface Natives {
        public void seedAccountsInfo(
                long nativeAccountTrackerService, String[] gaiaIds, String[] accountNames);
        public boolean areAccountsSeeded(long nativeAccountTrackerService, String[] accountNames);
    }
}
