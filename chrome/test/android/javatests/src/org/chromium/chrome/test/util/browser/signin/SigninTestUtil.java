// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;

import androidx.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninHelper;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.OAuth2TokenService;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Utility class for test signin functionality.
 */
public final class SigninTestUtil {
    private static final String TAG = "Signin";

    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    @SuppressLint("StaticFieldLeak")
    private static FakeAccountManagerDelegate sAccountManager;
    @SuppressLint("StaticFieldLeak")
    private static List<AccountHolder> sAddedAccounts = new ArrayList<>();

    /**
     * Sets up the test authentication environment.
     *
     * This must be called before native is loaded.
     */
    @WorkerThread
    public static void setUpAuthForTest() {
        sAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(sAccountManager);
        overrideAccountIdProvider();
        resetSigninState();
        SigninHelper.resetSharedPrefs();
    }

    /**
     * Tears down the test authentication environment.
     */
    @WorkerThread
    public static void tearDownAuthForTest() {
        for (AccountHolder accountHolder : sAddedAccounts) {
            sAccountManager.removeAccountHolderBlocking(accountHolder);
        }
        sAddedAccounts.clear();
        resetSigninState();
        SigninHelper.resetSharedPrefs();
    }

    /**
     * Returns the currently signed in account.
     */
    public static Account getCurrentAccount() {
        return ChromeSigninController.get().getSignedInUser();
    }

    /**
     * Add an account with the default name.
     */
    public static Account addTestAccount() {
        return addTestAccount(DEFAULT_ACCOUNT);
    }

    /**
     * Add an account with a given name.
     */
    public static Account addTestAccount(String name) {
        Account account = AccountManagerFacade.createAccountFromName(name);
        AccountHolder accountHolder = AccountHolder.builder(account).alwaysAccept(true).build();
        sAccountManager.addAccountHolderBlocking(accountHolder);
        sAddedAccounts.add(accountHolder);
        TestThreadUtils.runOnUiThreadBlocking(SigninTestUtil::seedAccounts);
        return account;
    }

    /**
     * Add and sign in an account with the default name.
     */
    public static Account addAndSignInTestAccount() {
        Account account = addTestAccount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSigninController.get().setSignedInAccountName(DEFAULT_ACCOUNT);
            seedAccounts();
        });
        return account;
    }

    private static void seedAccounts() {
        AccountIdProvider accountIdProvider = AccountIdProvider.getInstance();
        Account[] accounts = sAccountManager.getAccountsSyncNoThrow();
        String[] accountNames = new String[accounts.length];
        String[] accountIds = new String[accounts.length];
        for (int i = 0; i < accounts.length; i++) {
            accountNames[i] = accounts[i].name;
            accountIds[i] = accountIdProvider.getAccountId(accounts[i].name);
        }
        IdentityServicesProvider.getAccountTrackerService().syncForceRefreshForTest(
                accountIds, accountNames);
    }

    private static void overrideAccountIdProvider() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
                @Override
                public String getAccountId(String accountName) {
                    return "gaia-id-" + accountName.replace("@", "_at_");
                }

                @Override
                public boolean canBeUsed() {
                    return true;
                }
            });
        });
    }

    /**
     * Should be called at setUp and tearDown so that the signin state is not leaked across tests.
     * The setUp call is implicit inside the constructor.
     */
    public static void resetSigninState() {
        // Clear cached signed account name and accounts list.
        ChromeSigninController.get().setSignedInAccountName(null);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet(OAuth2TokenService.STORED_ACCOUNTS_KEY, new HashSet<>())
                .apply();
    }

    private SigninTestUtil() {}
}
