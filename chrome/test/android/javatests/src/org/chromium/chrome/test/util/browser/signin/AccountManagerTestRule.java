// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;

import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * This test rule mocks AccountManagerFacade and manages sign-in/sign-out.
 *
 * When the user does not invoke any sign-in functions with this rule, the rule will not
 * invoke any native code, therefore it is safe to use it in Robolectric tests just as
 * a simple AccountManagerFacade mock.
 */
public class AccountManagerTestRule implements TestRule {
    public static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade;
    private boolean mIsSignedIn;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(null));
    }

    public AccountManagerTestRule(FakeProfileDataSource fakeProfileDataSource) {
        this(new FakeAccountManagerFacade(fakeProfileDataSource));
    }

    public AccountManagerTestRule(FakeAccountManagerFacade fakeAccountManagerFacade) {
        mFakeAccountManagerFacade = fakeAccountManagerFacade;
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpRule();
                try {
                    statement.evaluate();
                } finally {
                    tearDownRule();
                }
            }
        };
    }

    /**
     * Sets up the AccountManagerFacade mock.
     */
    public void setUpRule() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /**
     * Tears down the AccountManagerFacade mock and signs out if user is signed in.
     */
    public void tearDownRule() {
        if (mIsSignedIn && getCurrentSignedInAccount() != null) {
            // For android_browsertests that sign out during the test body, like
            // UkmBrowserTest.SingleSyncSignoutCheck, we should sign out during tear-down test stage
            // only if an account is signed in. Otherwise, tearDownRule() ultimately results a crash
            // in SignoutManager::signOut(). This is because sign out is attempted when a sign-out
            // operation is already in progress. See crbug/1102746 for more details.
            signOut();
        }
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    /**
     * TODO(https://crbug.com/1117006): Change the return type of addAccount() to CoreAccountInfo
     *
     * Add an account to the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(Account account) {
        mFakeAccountManagerFacade.addAccount(account);
        return account;
    }

    /**
     * Add an account of the given accountName to the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(String accountName) {
        return addAccount(AccountUtils.createAccountFromName(accountName));
    }

    /**
     * Add an account to the fake AccountManagerFacade and its profileData to the
     * ProfileDataSource of the fake AccountManagerFacade.
     * @return The account added.
     */
    public Account addAccount(ProfileDataSource.ProfileData profileData) {
        Account account = addAccount(profileData.getAccountName());
        mFakeAccountManagerFacade.setProfileData(profileData.getAccountName(), profileData);
        return account;
    }

    /**
     * Waits for the AccountTrackerService to seed system accounts.
     */
    public void waitForSeeding() {
        SigninTestUtil.seedAccounts();
    }

    /**
     * Adds an account and seed it in native code.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account addAccountAndWaitForSeeding(String accountName) {
        Account account = addAccount(accountName);
        waitForSeeding();
        return account;
    }

    /**
     * Removes an account and seed it in native code.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public void removeAccountAndWaitForSeeding(String accountName) {
        mFakeAccountManagerFacade.removeAccount(AccountUtils.createAccountFromName(accountName));
        waitForSeeding();
    }

    /**
     * Add and sign in an account with the default name.
     *
     * This method does not enable sync.
     */
    public CoreAccountInfo addTestAccountThenSignin() {
        assert !mIsSignedIn : "An account is already signed in!";
        Account account = addAccountAndWaitForSeeding(TEST_ACCOUNT_EMAIL);
        CoreAccountInfo coreAccountInfo = toCoreAccountInfo(account.name);
        SigninTestUtil.signin(coreAccountInfo);
        mIsSignedIn = true;
        return coreAccountInfo;
    }

    /**
     * Add and sign in an account with the default name.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account addTestAccountThenSigninAndEnableSync() {
        return addTestAccountThenSigninAndEnableSync(
                TestThreadUtils.runOnUiThreadBlockingNoException(ProfileSyncService::get));
    }

    /**
     * Add and sign in an account with the default name.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     *
     * @param profileSyncService ProfileSyncService object to set up sync, if null, sync won't
     *         start.
     */
    public Account addTestAccountThenSigninAndEnableSync(
            @Nullable ProfileSyncService profileSyncService) {
        assert !mIsSignedIn : "An account is already signed in!";
        Account account = addAccountAndWaitForSeeding(TEST_ACCOUNT_EMAIL);
        CoreAccountInfo coreAccountInfo = toCoreAccountInfo(account.name);
        SigninTestUtil.signinAndEnableSync(coreAccountInfo, profileSyncService);
        mIsSignedIn = true;
        return account;
    }

    /**
     * Returns the currently signed in account.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public Account getCurrentSignedInAccount() {
        return SigninTestUtil.getCurrentAccount();
    }

    /**
     * Converts an account email to its corresponding CoreAccountInfo object.
     */
    public CoreAccountInfo toCoreAccountInfo(String accountEmail) {
        String accountGaiaId = mFakeAccountManagerFacade.getAccountGaiaId(accountEmail);
        return new CoreAccountInfo(new CoreAccountId(accountGaiaId), accountEmail, accountGaiaId);
    }

    /**
     * Sign out from the current account.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public void signOut() {
        SigninTestUtil.signOut();
        mIsSignedIn = false;
    }
}
