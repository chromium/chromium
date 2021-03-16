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
import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of AccountTrackerService which provides access from the java layer.
 * It offers the capability of fetching and seeding system accounts into AccountTrackerService in
 * C++ layer, and notifies observers when it is complete.
 *
 * TODO(crbug/1176136): Move this class to components/signin/internal
 */
public class AccountTrackerService {
    private static final String TAG = "AccountService";

    private final long mNativeAccountTrackerService;
    private @SystemAccountsSeedingStatus int mSystemAccountsSeedingStatus;
    private boolean mSystemAccountsChanged;
    private AccountsChangeObserver mAccountsChangeObserver;

    @IntDef({SystemAccountsSeedingStatus.SEEDING_NOT_STARTED,
            SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS,
            SystemAccountsSeedingStatus.SEEDING_DONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SystemAccountsSeedingStatus {
        int SEEDING_NOT_STARTED = 0;
        int SEEDING_IN_PROGRESS = 1;
        int SEEDING_DONE = 2;
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

    @VisibleForTesting
    @CalledByNative
    AccountTrackerService(long nativeAccountTrackerService) {
        mNativeAccountTrackerService = nativeAccountTrackerService;
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
        mSystemAccountsChanged = false;
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
            seedAccounts();
        }
        return false;
    }

    /**
     * Checks whether system accounts are seeded without changing the state.
     * @return Whether account list in {@link AccountManagerFacade} is consistent with accounts in
     *         the native AccountTrackerService.
     */
    private boolean areSystemAccountsSeeded() {
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

    private void seedAccounts() {
        ThreadUtils.assertOnUiThread();
        mSystemAccountsChanged = false;
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (accountManagerFacade.isGooglePlayServicesAvailable()) {
            mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_IN_PROGRESS;
        } else {
            mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_NOT_STARTED;
            return;
        }

        if (mAccountsChangeObserver == null) {
            mAccountsChangeObserver =
                    () -> invalidateAccountSeedStatus(false /* don't reseed right now */);
            accountManagerFacade.addObserver(mAccountsChangeObserver);
        }

        accountManagerFacade.tryGetGoogleAccounts(accounts -> {
            final List<String> emails = AccountUtils.toAccountNames(accounts);
            new AsyncTask<List<String>>() {
                @Override
                public List<String> doInBackground() {
                    Log.d(TAG, "Getting id/email mapping");
                    final long seedingStartTime = SystemClock.elapsedRealtime();
                    final List<String> gaiaIds = new ArrayList<>();
                    for (String email : emails) {
                        final String gaiaId = accountManagerFacade.getAccountGaiaId(email);
                        if (gaiaId == null) {
                            return gaiaIds;
                        }
                        gaiaIds.add(gaiaId);
                    }
                    RecordHistogram.recordTimesHistogram("Signin.AndroidGetAccountIdsTime",
                            SystemClock.elapsedRealtime() - seedingStartTime);
                    return gaiaIds;
                }
                @Override
                public void onPostExecute(List<String> gaiaIds) {
                    if (mSystemAccountsChanged || gaiaIds.size() < emails.size()) {
                        seedAccounts();
                    } else {
                        finishSeedingAccounts(gaiaIds, emails);
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        });
    }

    private void finishSeedingAccounts(List<String> gaiaIds, List<String> emails) {
        assert gaiaIds.size() == emails.size() : "gaia IDs and emails should have the same size!";
        AccountTrackerServiceJni.get().seedAccountsInfo(mNativeAccountTrackerService,
                gaiaIds.toArray(new String[0]), emails.toArray(new String[0]));
        mSystemAccountsSeedingStatus = SystemAccountsSeedingStatus.SEEDING_DONE;
        // TODO(crbug/1187458): Download account information in the end of account seeding
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsSeedingComplete();
        }
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

    private void notifyObserversOnAccountsChange() {
        for (OnSystemAccountsSeededListener observer : mSystemAccountsSeedingObservers) {
            observer.onSystemAccountsChanged();
        }
    }

    @NativeMethods
    interface Natives {
        void seedAccountsInfo(long nativeAccountTrackerService, String[] gaiaIds, String[] emails);
    }
}
