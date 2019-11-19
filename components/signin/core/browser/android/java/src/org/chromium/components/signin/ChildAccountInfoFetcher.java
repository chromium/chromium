// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * ChildAccountInfoFetcher for the Android platform.
 * Checks whether an account is a child account from the AccountManager.
 */
public final class ChildAccountInfoFetcher {
    private static final String TAG = "signin";

    private static final String ACCOUNT_SERVICES_CHANGED_FILTER =
            "com.google.android.gms.auth.ACCOUNT_SERVICES_CHANGED";

    private static final String ACCOUNT_CHANGE_PERMISSION =
            "com.google.android.gms.auth.permission.GOOGLE_ACCOUNT_CHANGE";

    private static final String ACCOUNT_KEY = "account";

    private final long mNativeAccountFetcherService;
    private final String mAccountId;
    private final Account mAccount;
    private final BroadcastReceiver mAccountFlagsChangedReceiver;

    private ChildAccountInfoFetcher(
            long nativeAccountFetcherService, String accountId, String accountName) {
        mNativeAccountFetcherService = nativeAccountFetcherService;
        mAccountId = accountId;
        mAccount = AccountManagerFacade.createAccountFromName(accountName);

        // Register for notifications about flag changes in the future.
        mAccountFlagsChangedReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                ThreadUtils.assertOnUiThread();
                Account changedAccount = intent.getParcelableExtra(ACCOUNT_KEY);
                Log.d(TAG, "Received account flag change broadcast for %s", changedAccount.name);

                if (!mAccount.equals(changedAccount)) return;

                fetch();
            }
        };
        ContextUtils.getApplicationContext().registerReceiver(mAccountFlagsChangedReceiver,
                new IntentFilter(ACCOUNT_SERVICES_CHANGED_FILTER), ACCOUNT_CHANGE_PERMISSION, null);

        // Fetch once now to update the status in case it changed before we registered for updates.
        fetch();
    }

    @CalledByNative
    private static ChildAccountInfoFetcher create(
            long nativeAccountFetcherService, String accountId, String accountName) {
        return new ChildAccountInfoFetcher(nativeAccountFetcherService, accountId, accountName);
    }

    private void fetch() {
        Log.d(TAG, "Checking child account status for %s", mAccount.name);
        AccountManagerFacade.get().checkChildAccountStatus(
                mAccount, status -> setIsChildAccount(ChildAccountStatus.isChild(status)));
    }

    @CalledByNative
    private void destroy() {
        ContextUtils.getApplicationContext().unregisterReceiver(mAccountFlagsChangedReceiver);
    }

    private void setIsChildAccount(boolean isChildAccount) {
        Log.d(TAG, "Setting child account status for %s to %s", mAccount.name,
                Boolean.toString(isChildAccount));
        ChildAccountInfoFetcherJni.get().setIsChildAccount(
                mNativeAccountFetcherService, mAccountId, isChildAccount);
    }

    @CalledByNative
    private static void initializeForTests() {
        AccountManagerDelegate delegate = new SystemAccountManagerDelegate();
        AccountManagerFacade.overrideAccountManagerFacadeForTests(delegate);
    }

    @NativeMethods
    interface Natives {
        void setIsChildAccount(
                long accountFetcherServicePtr, String accountId, boolean isChildAccount);
    }
}
