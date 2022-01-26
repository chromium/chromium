// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.R;
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

    private final @NonNull FakeAccountManagerFacade mFakeAccountManagerFacade;
    private final @Nullable FakeAccountInfoService mFakeAccountInfoService;
    private boolean mIsSignedIn;

    public AccountManagerTestRule() {
        this(new FakeAccountManagerFacade(), new FakeAccountInfoService());
    }

    public AccountManagerTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade) {
        this(fakeAccountManagerFacade, new FakeAccountInfoService());
    }

    public AccountManagerTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade,
            @NonNull FakeAccountInfoService fakeAccountInfoService) {
        mFakeAccountManagerFacade = fakeAccountManagerFacade;
        mFakeAccountInfoService = fakeAccountInfoService;
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { AccountInfoServiceProvider.setInstanceForTests(mFakeAccountInfoService); });
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    /**
     * Tears down the AccountManagerFacade mock and signs out if user is signed in.
     */
    public void tearDownRule() {
        if (mIsSignedIn && getPrimaryAccount(ConsentLevel.SIGNIN) != null) {
            // For android_browsertests that sign out during the test body, like
            // UkmBrowserTest.SingleSyncSignoutCheck, we should sign out during tear-down test stage
            // only if an account is signed in. Otherwise, tearDownRule() ultimately results a crash
            // in SignoutManager::signOut(). This is because sign out is attempted when a sign-out
            // operation is already in progress. See crbug/1102746 for more details.
            signOut();
        }
        AccountManagerFacadeProvider.resetInstanceForTests();
        AccountInfoServiceProvider.resetForTests();
    }

    /**
     * Adds an observer that detects changes in the account state propagated by the
     * IdentityManager object.
     */
    public void observeIdentityManager(IdentityManager identityManager) {
        identityManager.addObserver(mFakeAccountInfoService);
    }

    /**
     * Adds an account of the given accountName to the fake AccountManagerFacade.
     * @return The CoreAccountInfo for the account added.
     */
    public CoreAccountInfo addAccount(String accountName) {
        assert mFakeAccountInfoService != null;
        final String baseEmail = accountName.split("@", 2)[0];
        return addAccount(accountName, baseEmail + ".full", baseEmail + ".given", createAvatar());
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to
     * {@link FakeAccountInfoService}.
     */
    public CoreAccountInfo addAccount(
            String email, String fullName, String givenName, @Nullable Bitmap avatar) {
        assert mFakeAccountInfoService != null;
        mFakeAccountInfoService.addAccountInfo(email, fullName, givenName, avatar);
        final Account account = AccountUtils.createAccountFromName(email);
        mFakeAccountManagerFacade.addAccount(account);
        return toCoreAccountInfo(email);
    }

    /**
     * Removes an account with the given account email.
     */
    public void removeAccount(String accountEmail) {
        mFakeAccountManagerFacade.removeAccount(AccountUtils.createAccountFromName(accountEmail));
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
    public CoreAccountInfo addAccountAndWaitForSeeding(String accountName) {
        final CoreAccountInfo coreAccountInfo = addAccount(accountName);
        waitForSeeding();
        return coreAccountInfo;
    }

    /**
     * Removes an account and seed it in native code.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public void removeAccountAndWaitForSeeding(String accountEmail) {
        removeAccount(accountEmail);
        waitForSeeding();
    }

    /**
     * Adds and signs in an account with the default name without sync consent.
     *
     * This method does not enable sync.
     */
    public CoreAccountInfo addTestAccountThenSignin() {
        assert !mIsSignedIn : "An account is already signed in!";
        CoreAccountInfo coreAccountInfo = addAccountAndWaitForSeeding(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        mIsSignedIn = true;
        return coreAccountInfo;
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync() {
        return addTestAccountThenSigninAndEnableSync(
                TestThreadUtils.runOnUiThreadBlockingNoException(SyncService::get));
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't
     *         start.
     */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync(
            @Nullable SyncService syncService) {
        assert !mIsSignedIn : "An account is already signed in!";
        CoreAccountInfo coreAccountInfo = addAccountAndWaitForSeeding(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signinAndEnableSync(coreAccountInfo, syncService);
        mIsSignedIn = true;
        return coreAccountInfo;
    }

    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     * This method invokes native code. It shouldn't be called in a Robolectric test.
     */
    public CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return SigninTestUtil.getPrimaryAccount(consentLevel);
    }

    /**
     * Converts an account email to its corresponding CoreAccountInfo object.
     */
    public CoreAccountInfo toCoreAccountInfo(String accountEmail) {
        String accountGaiaId = mFakeAccountManagerFacade.getAccountGaiaId(accountEmail);
        return CoreAccountInfo.createFromEmailAndGaiaId(accountEmail, accountGaiaId);
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

    /**
     * Creates an email used to identify child accounts in tests.
     * A child-specific prefix will be appended to the base name so that the created account
     * will be considered as {@link ChildAccountStatus#REGULAR_CHILD} in
     * {@link FakeAccountManagerFacade}.
     */
    public static String generateChildEmail(String baseName) {
        return FakeAccountManagerFacade.generateChildEmail(baseName);
    }

    /**
     * Returns an avatar image created from test resource.
     */
    private static Bitmap createAvatar() {
        Drawable drawable = AppCompatResources.getDrawable(
                ContextUtils.getApplicationContext(), R.drawable.test_profile_picture);
        Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
