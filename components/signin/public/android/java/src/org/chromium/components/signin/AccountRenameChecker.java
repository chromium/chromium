// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import com.google.android.gms.auth.AccountChangeEvent;
import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.task.AsyncTask;

import java.io.IOException;
import java.util.List;

/**
 * A checker of the account rename event.
 */
public final class AccountRenameChecker {
    private static final String TAG = "AccountRenameChecker";
    private static AccountRenameChecker sInstance;

    private static final class SystemDelegate {
        /**
         * Gets the new account name of the renamed account.
         * @return The new name that the given account email is renamed to or null if the given
         *         email is not renamed.
         */
        @WorkerThread
        @Nullable
        String getNewNameOfRenamedAccount(String accountEmail) {
            final Context context = ContextUtils.getApplicationContext();
            try {
                final List<AccountChangeEvent> accountChangeEvents =
                        GoogleAuthUtil.getAccountChangeEvents(context, 0, accountEmail);
                for (AccountChangeEvent event : accountChangeEvents) {
                    if (event.getChangeType() == GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO) {
                        return event.getChangeData();
                    }
                }
            } catch (IOException | GoogleAuthException e) {
                Log.w(TAG, "Failed to get change events", e);
            }
            return null;
        }
    }

    private final SystemDelegate mDelegate;

    private AccountRenameChecker() {
        mDelegate = new SystemDelegate();
    }

    /**
     * @return The Singleton instance of {@link AccountRenameChecker}.
     */
    public static AccountRenameChecker get() {
        if (sInstance == null) {
            sInstance = new AccountRenameChecker();
        }
        return sInstance;
    }

    /**
     * Gets the new account name of the renamed account asynchronously.
     *
     * If the old account is renamed to an account that exists in the given list of
     * accounts, the callback will be executed with the renamed-to account name.
     *
     * Otherwise the callback will be executed with null.
     */
    public Promise<String> getNewNameOfRenamedAccountAsync(
            String oldAccountEmail, List<Account> accounts) {
        final Promise<String> newNamePromise = new Promise<>();
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                return getNewNameOfRenamedAccount(oldAccountEmail, accounts);
            }

            @Override
            protected void onPostExecute(String newAccountName) {
                newNamePromise.fulfill(newAccountName);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        return newNamePromise;
    }

    @WorkerThread
    private @Nullable String getNewNameOfRenamedAccount(
            String oldAccountEmail, List<Account> accounts) {
        String newAccountEmail = mDelegate.getNewNameOfRenamedAccount(oldAccountEmail);
        while (newAccountEmail != null) {
            if (AccountUtils.findAccountByName(accounts, newAccountEmail) != null) {
                break;
            }
            // When the new name does not exist in the list, continue to search if it is
            // renamed to another account existing in the list.
            newAccountEmail = mDelegate.getNewNameOfRenamedAccount(newAccountEmail);
        }
        return newAccountEmail;
    }
}
