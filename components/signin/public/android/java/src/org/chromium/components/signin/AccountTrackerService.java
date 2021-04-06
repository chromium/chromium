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
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Queue;

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
    private final Queue<Runnable> mRunnablesWaitingForAccountsSeeding = new ArrayDeque<>();
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
     * Seeds the accounts only if they are not seeded yet.
     * The given runnable will run after the accounts are seeded. If the accounts
     * are already seeded, the runnable will be executed immediately.
     */
    public void seedAccountsIfNeeded(Runnable onAccountsSeeded) {
        ThreadUtils.assertOnUiThread();
        switch (mAccountsSeedingStatus) {
            case AccountsSeedingStatus.NOT_STARTED:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                seedAccounts();
                break;
            case AccountsSeedingStatus.IN_PROGRESS:
                mRunnablesWaitingForAccountsSeeding.add(onAccountsSeeded);
                break;
            case AccountsSeedingStatus.DONE:
                onAccountsSeeded.run();
                break;
        }
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     * This is invoked when accounts change on device. When there is already a seeding in
     * progress, the {@link #seedAccounts()} task will be added to the pending task list so
     * that we will seed the accounts again in the end of the current seeding to avoid
     * the race condition.
     */
    public void onAccountsChanged() {
        if (mAccountsSeedingStatus == AccountsSeedingStatus.IN_PROGRESS) {
            mRunnablesWaitingForAccountsSeeding.add(this::seedAccounts);
        } else {
            seedAccounts();
        }
    }

    private void seedAccounts() {
        ThreadUtils.assertOnUiThread();
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (!accountManagerFacade.isGooglePlayServicesAvailable()) {
            return;
        }
        assert mAccountsSeedingStatus
                != AccountsSeedingStatus.IN_PROGRESS : "There is already a seeding in progress!";
        mAccountsSeedingStatus = AccountsSeedingStatus.IN_PROGRESS;

        if (mAccountsChangeObserver == null) {
            mAccountsChangeObserver = this::onAccountsChanged;
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

        while (!mRunnablesWaitingForAccountsSeeding.isEmpty()) {
            Runnable runnable = mRunnablesWaitingForAccountsSeeding.remove();
            runnable.run();
        }

        if (mOnAccountSeededListener != null) {
            for (String gaiaId : gaiaIds) {
                mOnAccountSeededListener.onResult(new CoreAccountId(gaiaId));
            }
        }
    }

    @NativeMethods
    interface Natives {
        void seedAccountsInfo(long nativeAccountTrackerService, String[] gaiaIds, String[] emails);
    }
}
