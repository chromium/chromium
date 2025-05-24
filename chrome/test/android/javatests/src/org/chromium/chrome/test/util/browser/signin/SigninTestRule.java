// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import static org.hamcrest.Matchers.is;

import androidx.annotation.NonNull;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * This test rule mocks AccountManagerFacade and manages sign-in/sign-out.
 *
 * <p>Calling the sign-in functions will invoke native code, therefore this should only be used in
 * on-device tests. In Robolectric tests, use the {@link AccountManagerTestRule} instead as a simple
 * AccountManagerFacade mock.
 */
public class SigninTestRule extends AccountManagerTestRule {
    private boolean mIsSignedIn;
    private final SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher =
            new SigninTestUtil.CustomDeviceLockActivityLauncher();

    public SigninTestRule() {}

    public SigninTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade) {
        super(fakeAccountManagerFacade);
    }

    @Override
    public void setUpRule() {
        super.setUpRule();
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncher);
    }

    /** Signs out if user is signed in. */
    @Override
    public void tearDownRule() {
        if (mIsSignedIn && getPrimaryAccount(ConsentLevel.SIGNIN) != null) {
            // For android_browsertests that sign out during the test body, like
            // UkmBrowserTest.SingleSyncSignoutCheck, we should sign out during tear-down test stage
            // only if an account is signed in. Otherwise, tearDownRule() ultimately results a crash
            // in SignoutManager::signOut(). This is because sign out is attempted when a sign-out
            // operation is already in progress. See crbug/1102746 for more details.
            //
            // We call the force sign out version to make it easier for test writers to write tests
            // which cleanly tear down (eg. for supervised users who otherwise are not allowed to
            // sign out).
            forceSignOut();
        }
        DeviceLockActivityLauncherImpl.setInstanceForTesting(null);
        super.tearDownRule();
    }

    /**
     * Adds and signs in an account with the default name.
     *
     * @deprecated Use the version with {@link AccountInfo}.
     */
    @Deprecated
    public CoreAccountInfo addTestAccountThenSignin() {
        assert !mIsSignedIn : "An account is already signed in!";
        CoreAccountInfo coreAccountInfo = addAccount(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signin(coreAccountInfo);
        mIsSignedIn = true;
        return coreAccountInfo;
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
        CoreAccountInfo coreAccountInfo = addAccount(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signinWithConsentLevelSync(coreAccountInfo);
        mIsSignedIn = true;
        return coreAccountInfo;
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
