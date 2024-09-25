// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import static org.hamcrest.Matchers.is;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncService;

/**
 * This test rule mocks AccountManagerFacade and manages sign-in/sign-out.
 *
 * <p>TODO(crbug.com/40228092): Migrate usage of {@link AccountManagerTestRule} that need native to
 * this rule, then inline the methods that call native.
 *
 * <p>Calling the sign-in functions will invoke native code, therefore this should only be used in
 * on-device tests. In Robolectric tests, use the {@link AccountManagerTestRule} instead as a simple
 * AccountManagerFacade mock.
 */
public class SigninTestRule extends AccountManagerTestRule {
    private boolean mIsSignedIn;

    public SigninTestRule() {}

    public SigninTestRule(@NonNull FakeAccountManagerFacade fakeAccountManagerFacade) {
        super(fakeAccountManagerFacade);
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
        super.tearDownRule();
    }

    /**
     * Adds and signs in an account with the default name without sync consent.
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

    /** Adds and signs in with the provided account without sync consent. */
    public void addAccountThenSignin(AccountInfo accountInfo) {
        assert !mIsSignedIn : "An account is already signed in!";
        addAccount(accountInfo);
        SigninTestUtil.signin(accountInfo);
        mIsSignedIn = true;
    }

    /** Adds and signs in an account with the default name and enables sync. */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync() {
        return addTestAccountThenSigninAndEnableSync(
                SyncTestUtil.getSyncServiceForLastUsedProfile());
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't start.
     */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync(
            @Nullable SyncService syncService) {
        assert !mIsSignedIn : "An account is already signed in!";
        CoreAccountInfo coreAccountInfo = addAccount(TEST_ACCOUNT_EMAIL);
        SigninTestUtil.signinAndEnableSync(coreAccountInfo, syncService);
        mIsSignedIn = true;
        return coreAccountInfo;
    }

    /** Adds and signs in an account with the specified name and enables sync. */
    public void addAccountThenSigninAndEnableSync(AccountInfo accountInfo) {
        assert !mIsSignedIn : "An account is already signed in!";
        addAccount(accountInfo);
        SigninTestUtil.signinAndEnableSync(
                accountInfo, SyncTestUtil.getSyncServiceForLastUsedProfile());
        mIsSignedIn = true;
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
                new AccountInfo.Builder(
                                generateChildEmail("test@gmail.com"),
                                FakeAccountManagerFacade.toGaiaId("test-gaia-id"))
                        .fullName("ChildTest Full")
                        .givenName("ChildTest Given")
                        .accountCapabilities(builder.setIsSubjectToParentalControls(true).build())
                        .build();

        addAccount(testChildAccount);

        // The child will be force signed in (by SigninChecker).
        // Wait for this to complete before enabling sync.
        waitForSignin(testChildAccount);
        return testChildAccount;
    }

    /**
     * Adds a child account, waits for auto-signin to complete, and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't start.
     */
    public CoreAccountInfo addChildTestAccountThenEnableSync(@Nullable SyncService syncService) {
        CoreAccountInfo coreAccountInfo = addChildTestAccountThenWaitForSignin();

        // The auto sign-in should leave the user in signed-in, non-syncing state - check this and
        // enable sync.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assert IdentityServicesProvider.get()
                                            .getIdentityManager(
                                                    ProfileManager.getLastUsedRegularProfile())
                                            .getPrimaryAccountInfo(ConsentLevel.SYNC)
                                    == null
                            : "Sync should not be enabled";
                });
        SigninTestUtil.signinAndEnableSync(coreAccountInfo, syncService);

        return coreAccountInfo;
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't
     *         start.
     * @param isChild Whether this is a supervised child account.
     */
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync(
            @Nullable SyncService syncService, boolean isChild) {
        return isChild
                ? addChildTestAccountThenEnableSync(syncService)
                : addTestAccountThenSigninAndEnableSync(syncService);
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
}
