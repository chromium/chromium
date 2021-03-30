// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.signin.base.CoreAccountId;

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
    private final List<Runnable> mRunnablesWaitingForAccountsSeeding = new ArrayList<>();
    private @AccountsSeedingStatus int mAccountsSeedingStatus;
    private Callback<CoreAccountId> mOnAccountSeededListener;
    private AccountsChangeObserver mAccountsChangeObserver;

    @VisibleForTesting
    @CalledByNative
    AccountTrackerService(long nativeAccountTrackerService) {
        mNativeAccountTrackerService = nativeAccountTrackerService;
        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
    }

    /**
     * Sets a listener that gets executed when an account is seeded.
     */
    public void setOnAccountSeededListener(Callback<CoreAccountId> onAccountSeededListener) {
        mOnAccountSeededListener = onAccountSeededListener;
    }

    /**
     * Checks whether the account id <-> email mapping has been seeded into C++ layer.
     * If not, it automatically starts fetching the mapping and seeds it.
     * @return Whether the accounts have been seeded already.
     *
     * TODO(crbug/1185162): Remove this method after removing all the callers
     * Use {@link #seedAccountsIfNeeded(Runnable)} instead.
     */
    @Deprecated
    private boolean checkAndSeedSystemAccounts() {
        ThreadUtils.assertOnUiThread();
        if (mAccountsSeedingStatus == AccountsSeedingStatus.DONE) {
            return true;
        }
        seedAccounts();
        return false;
    }

    /**
     * Seeds the accounts if they are not seeded yet.
     * The given runnable will run after the accounts are seeded. If the accounts
     * are already seeded, the runnable will be executed immediately.
     */
    public void seedAccountsIfNeeded(Runnable onAccountsSeeded) {
        ThreadUtils.assertOnUiThread();
        if (mAccountsSeedingStatus == AccountsSeedingStatus.DONE) {
            onAccountsSeeded.run();
            return;
        }
        mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
        seedAccounts();
    }

    public void seedAccounts() {
        ThreadUtils.assertOnUiThread();
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (!accountManagerFacade.isGooglePlayServicesAvailable()
                || mAccountsSeedingStatus == AccountsSeedingStatus.IN_PROGRESS) {
            return;
        }
        mAccountsSeedingStatus = AccountsSeedingStatus.IN_PROGRESS;

        if (mAccountsChangeObserver == null) {
            mAccountsChangeObserver = this::seedAccounts;
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
                    if (gaiaIds.size() == emails.size()) {
                        finishSeedingAccounts(gaiaIds, emails);
                    } else {
                        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
                        seedAccounts();
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        });
    }

    private void finishSeedingAccounts(List<String> gaiaIds, List<String> emails) {
        assert gaiaIds.size() == emails.size() : "gaia IDs and emails should have the same size!";
        AccountTrackerServiceJni.get().seedAccountsInfo(mNativeAccountTrackerService,
                gaiaIds.toArray(new String[0]), emails.toArray(new String[0]));
        mAccountsSeedingStatus = AccountsSeedingStatus.DONE;
        for (Runnable runnable : mRunnablesWaitingForAccountsSeeding) {
            runnable.run();
        }
        mRunnablesWaitingForAccountsSeeding.clear();

        if (mOnAccountSeededListener != null) {
            for (String gaiaId : gaiaIds) {
                mOnAccountSeededListener.onResult(new CoreAccountId(gaiaId));
            }
        }
    }

    /**
     * Notifies the AccountTrackerService about changed system accounts. without actually triggering
     * @param reSeedAccounts Whether to also start seeding the new account information immediately.
     *
     * TODO(crbug/1185712): Replace the only caller of this method SigninManagerImpl to call
     * seedAccounts() directly.
     */
    @Deprecated
    public void invalidateAccountSeedStatus(boolean reSeedAccounts) {
        ThreadUtils.assertOnUiThread();
        mAccountsSeedingStatus = AccountsSeedingStatus.NOT_STARTED;
        if (reSeedAccounts) checkAndSeedSystemAccounts();
    }

    @NativeMethods
    interface Natives {
        void seedAccountsInfo(long nativeAccountTrackerService, String[] gaiaIds, String[] emails);
    }
}
