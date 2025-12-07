// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;

import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManagerImpl;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;

/**
 * This test rule mocks AccountManagerFacade & AccountInfoService, and manages sign-in/sign-out.
 *
 * <p>Calling the sign-in functions will invoke native code, therefore this should only be used in
 * on-device tests. In Robolectric tests, use the {@link AccountManagerTestRule} instead.
 */
public class SigninTestRule implements TestRule {
    // The matcher for the add account button in the fake add account activity.
    public static final Matcher<View> ADD_ACCOUNT_BUTTON_MATCHER =
            withId(FakeAccountManagerFacade.AddAccountActivityStub.OK_BUTTON_ID);
    // The matcher for the cancel button in the fake add account activity.
    public static final Matcher<View> CANCEL_ADD_ACCOUNT_BUTTON_MATCHER =
            withId(FakeAccountManagerFacade.AddAccountActivityStub.CANCEL_BUTTON_ID);

    private boolean mIsSignedIn;
    private final SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher =
            new SigninTestUtil.CustomDeviceLockActivityLauncher();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade;
    // TODO(crbug.com/341948846): Remove AccountInfoService.
    private final @Nullable FakeAccountInfoService mFakeAccountInfoService =
            new FakeAccountInfoService();

    private final boolean mSerializeToPrefs;

    public SigninTestRule() {
        this(false);
    }

    public SigninTestRule(boolean serializeToPrefs) {
        mSerializeToPrefs = serializeToPrefs;
        mFakeAccountManagerFacade = new FakeAccountManagerFacade(mSerializeToPrefs);
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

    public void setUpRule() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mFakeAccountInfoService != null) {
                        AccountInfoServiceProvider.setInstanceForTests(mFakeAccountInfoService);
                    }
                });
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncher);
    }

    public void tearDownRule() {
        DeviceLockActivityLauncherImpl.setInstanceForTesting(null);
        if (mFakeAccountInfoService != null) AccountInfoServiceProvider.resetForTests();
    }

    /**
     * Adds an account to the fake AccountManagerFacade and {@link AccountInfo} to {@link
     * FakeAccountInfoService}.
     */
    public void addAccount(AccountInfo accountInfo) {
        mFakeAccountManagerFacade.addAccount(accountInfo);
        // TODO(crbug.com/40234741): Revise this test rule and remove the condition here.
        if (mFakeAccountInfoService != null) mFakeAccountInfoService.addAccountInfo(accountInfo);
    }

    /** Updates an account in the fake AccountManagerFacade and {@link FakeAccountInfoService}. */
    public void updateAccount(AccountInfo accountInfo) {
        mFakeAccountManagerFacade.updateAccount(accountInfo);
    }

    /**
     * Initializes the next add account flow with a given account to add.
     *
     * @param newAccount The account that should be added by the add account flow.
     */
    public void setAddAccountFlowResult(@Nullable AccountInfo newAccount) {
        mFakeAccountManagerFacade.setAddAccountFlowResult(newAccount);
    }

    /** Removes an account with the given {@link CoreAccountId}. */
    public void removeAccount(CoreAccountId accountId) {
        mFakeAccountManagerFacade.removeAccount(accountId);
    }

    /** See {@link FakeAccountManagerFacade#setAccountFetchFailed()}. */
    public void setAccountFetchFailed() {
        mFakeAccountManagerFacade.setAccountFetchFailed();
    }

    /** See {@link FakeAccountManagerFacade#blockGetAccounts(boolean)}. */
    public FakeAccountManagerFacade.UpdateBlocker blockGetAccountsUpdate(boolean populateCache) {
        return mFakeAccountManagerFacade.blockGetAccounts(populateCache);
    }

    /**
     * Sets an error for the given `accountId` when requesting an access token through {@link
     * AccountManagerFacade}. Future access token requests will return the `authError` provided.
     * This method will propagate the error to native code as well through {@link
     * IdentityManagerImpl}.
     *
     * <p>If the `authError` has the state {@link GoogleServiceAuthErrorState#NONE} then {@link
     * AccountManagerFacade} will return valid access tokens instead of returning an error. Errors
     * must be set through a previous call to {@link #addOrUpdateAccessTokenError} before they can
     * be cleared this way.
     *
     * @param identityManager {@link IdentityManagerImpl} object to pass the error to native.
     * @param accountId The {@link CoreAccountId} to set the authError to.
     * @param authError A {@link GoogleServiceAuthError} to return on access token requests.
     */
    public void addOrUpdateAccessTokenError(
            IdentityManagerImpl identityManager,
            CoreAccountId accountId,
            GoogleServiceAuthError authError) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeAccountManagerFacade.addOrUpdateAccessTokenError(accountId, authError);
                    identityManager.updateAuthErrorForTesting(accountId, authError);
                });
    }

    /**
     * Resolves the minor mode of {@param accountInfo} to restricted, so that the UI will be safe to
     * show to minors.
     */
    public void resolveMinorModeToRestricted(CoreAccountId accountId) {
        mFakeAccountManagerFacade.updateAccountCapabilities(
                accountId, TestAccounts.MINOR_MODE_REQUIRED);
    }

    /**
     * Adds and signs in an account with the default name.
     *
     * @deprecated Use the version with {@link AccountInfo}.
     */
    @Deprecated
    public CoreAccountInfo addTestAccountThenSignin() {
        assert !mIsSignedIn : "An account is already signed in!";
        AccountInfo accountInfo = TestAccounts.ACCOUNT1;
        addAccount(accountInfo);
        SigninTestUtil.signin(accountInfo);
        mIsSignedIn = true;
        return accountInfo;
    }

    /** Adds and signs in with the provided account. */
    public void addAccountThenSignin(AccountInfo accountInfo) {
        assert !mIsSignedIn : "An account is already signed in!";
        addAccount(accountInfo);
        SigninTestUtil.signin(accountInfo);
        mIsSignedIn = true;
    }

    /** Adds and signs in with the provided account with consent level Sync. */
    // TODO(crbug.com/40066949): Remove once Sync-the-feature is fully removed.
    public void addAccountThenSigninWithConsentLevelSync(AccountInfo accountInfo) {
        assert !mIsSignedIn : "An account is already signed in!";
        addAccount(accountInfo);
        SigninTestUtil.signinWithConsentLevelSync(accountInfo);
        mIsSignedIn = true;
    }

    /** Adds and signs in with the provided account and opts into history sync. */
    public void addAccountThenSigninAndEnableHistorySync(AccountInfo accountInfo) {
        assert !mIsSignedIn : "An account is already signed in!";
        addAccount(accountInfo);
        SigninTestUtil.signinAndEnableHistorySync(accountInfo);
        mIsSignedIn = true;
    }

    /** Adds and signs in an account with the default name using consent level Sync. */
    // TODO(crbug.com/40066949): Remove once Sync-the-feature is fully removed.
    public CoreAccountInfo addTestAccountThenSigninWithConsentLevelSync() {
        assert !mIsSignedIn : "An account is already signed in!";
        AccountInfo accountInfo = TestAccounts.ACCOUNT1;
        addAccount(accountInfo);
        SigninTestUtil.signinWithConsentLevelSync(accountInfo);
        mIsSignedIn = true;
        return accountInfo;
    }

    /** Waits for the account corresponding to coreAccountInfo to finish signin. */
    public void waitForSignin(CoreAccountInfo coreAccountInfo) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                    .getPrimaryAccountInfo(ConsentLevel.SIGNIN),
                            is(coreAccountInfo));
                });
        mIsSignedIn = true;
    }

    /** Adds a child account, and waits for auto-signin to complete. */
    public AccountInfo addChildTestAccountThenWaitForSignin() {
        return addChildTestAccountThenWaitForSignin(new AccountCapabilitiesBuilder());
    }

    /** Adds a child account, and waits for auto-signin to complete with specified capabilities. */
    public AccountInfo addChildTestAccountThenWaitForSignin(AccountCapabilitiesBuilder builder) {
        assert !mIsSignedIn : "An account is already signed in!";

        AccountInfo testChildAccount =
                new AccountInfo.Builder(TestAccounts.CHILD_ACCOUNT)
                        .accountCapabilities(builder.setIsSubjectToParentalControls(true).build())
                        .build();

        addAccount(testChildAccount);

        // The account will be force signed in (by SigninChecker).
        // Wait for this to complete before enabling sync.
        waitForSignin(testChildAccount);

        // Wait for child status properties to be populated through asynchronous callbacks triggered
        // after sign-in completes.
        waitForChildSettingPropagation(testChildAccount);
        return testChildAccount;
    }

    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     */
    public CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return SigninTestUtil.getPrimaryAccount(consentLevel);
    }

    /** Sign out from the current account. */
    public void signOut() {
        SigninTestUtil.signOut();
        mIsSignedIn = false;
    }

    /**
     * Sign out from the current account, ignoring usual checks (suitable for eg. test teardown, but
     * not feature testing).
     */
    public void forceSignOut() {
        SigninTestUtil.forceSignOut();
        mIsSignedIn = false;
    }

    /** Completes the device lock flow when on automotive devices. */
    public void completeDeviceLockIfOnAutomotive() {
        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);
    }

    /** Waits for the account manager to set corresponding child properties. */
    private void waitForChildSettingPropagation(AccountInfo accountInfo) {
        CriteriaHelper.pollUiThread(
                () -> {
                    // The child sign-in triggers two changes to preferences in native code used
                    // to determine the child status to trigger Android UI changes.
                    // Check that `IsSubjectToParentalControls` is updated to `Tribool.TRUE` as
                    // expected for supervised accounts.
                    Criteria.checkThat(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                    .findExtendedAccountInfoByEmailAddress(accountInfo.getEmail())
                                    .getAccountCapabilities()
                                    .isSubjectToParentalControls(),
                            is(Tribool.TRUE));
                    // Check that the `kSupervisedUserId` preference is populated, which backs the
                    // Java `Profile.isChild` implementation.
                    Criteria.checkThat(
                            ProfileManager.getLastUsedRegularProfile().isChild(), is(true));
                });
    }
}
