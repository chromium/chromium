// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;

@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoMediatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    // TODO(crbug.com/374683682): Create and use FakeIdentityManager and add tests for sign-in and
    // sign-out events.
    private @Mock IdentityManager mIdentityManager;
    private @Mock SyncService mSyncService;
    private @Mock SigninPromoDelegate mDelegate;
    private @Mock Profile mProfile;
    private @Mock SigninManager mSigninManager;
    private @Mock IdentityServicesProvider mIdentityServicesProvider;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private ProfileDataCache mProfileDataCache;
    private final Context mContext = ApplicationProvider.getApplicationContext();

    private SigninPromoMediator mMediator;

    @Before
    public void setUp() {
        lenient().doReturn(true).when(mDelegate).canShowPromo();
    }

    @Test
    public void testSecondaryButtonHiddenByDelegate() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mDelegate).shouldHideSecondaryButton();
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        createSigninPromoMediator(mDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonHiddenByNullProfileData() {
        createSigninPromoMediator(mDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromIdentityManager() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(false).when(mDelegate).shouldHideSecondaryButton();
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        createSigninPromoMediator(mDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromAccountManager() {
        doReturn(false).when(mDelegate).shouldHideSecondaryButton();
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        createSigninPromoMediator(mDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testDefaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        createSigninPromoMediator(mDelegate);
        verify(mProfileDataCache, atLeastOnce())
                .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail());

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mProfileDataCache).getProfileDataOrDefault(TestAccounts.ACCOUNT2.getEmail());
    }

    @Test
    public void testDelegateUpdated_defaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createSigninPromoMediator(mDelegate);

        // Set the mock delegate to return non-default values.
        String newTitle = "newTitle";
        String newDescription = "newDescription";
        String newPrimaryButtonText = "newPrimaryButtonText";
        String newSecondaryButtonText = "newSecondaryButtonText";
        doReturn(true).when(mDelegate).refreshPromoState(any());
        doReturn(true).when(mDelegate).shouldHideDismissButton();
        doReturn(newTitle).when(mDelegate).getTitle();
        doReturn(newDescription).when(mDelegate).getDescription(any());
        doReturn(newPrimaryButtonText).when(mDelegate).getTextForPrimaryButton(any());
        doReturn(newSecondaryButtonText).when(mDelegate).getTextForSecondaryButton();
        // Remove the default account to trigger a promo content refresh.
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // Verify that the promo's model uses the new values returned by the delegate.
        boolean shouldHideDismissButton =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON);
        String title = mMediator.getModel().get(SigninPromoProperties.TITLE_TEXT);
        String description = mMediator.getModel().get(SigninPromoProperties.DESCRIPTION_TEXT);
        String primaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String secondaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.SECONDARY_BUTTON_TEXT);
        assertTrue(shouldHideDismissButton);
        assertEquals(newTitle, title);
        assertEquals(newDescription, description);
        assertEquals(newPrimaryButtonText, primaryButtonText);
        assertEquals(newSecondaryButtonText, secondaryButtonText);
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testModelValuesNtp_noAccountsOnDevice() {
        when(mSigninManager.isSigninAllowed()).thenReturn(true);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);

        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(), mProfile, mLauncher, () -> {});

        createSigninPromoMediator(delegate);

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        assertEquals(
                mContext.getString(R.string.signin_promo_title_ntp_sign_in_as_button),
                mMediator.getModel().get(SigninPromoProperties.TITLE_TEXT));
        assertEquals(
                mContext.getString(R.string.custom_tabs_signed_out_message_subtitle),
                mMediator.getModel().get(SigninPromoProperties.DESCRIPTION_TEXT));
        assertEquals(
                mContext.getString(R.string.sign_in_to_chrome),
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT));
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON));
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testModelValuesNtp_accountAvailableOnDevice() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSigninManager.isSigninAllowed()).thenReturn(true);
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(
                        TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(TestAccounts.ACCOUNT1);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);

        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(), mProfile, mLauncher, () -> {});
        createSigninPromoMediator(delegate);

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        assertEquals(
                mContext.getString(R.string.signin_promo_title_ntp_sign_in_as_button),
                mMediator.getModel().get(SigninPromoProperties.TITLE_TEXT));
        assertEquals(
                mContext.getString(R.string.signin_promo_description_ntp_group4),
                mMediator.getModel().get(SigninPromoProperties.DESCRIPTION_TEXT));
        assertEquals(
                mContext.getString(
                        R.string.signin_promo_sign_in_as, TestAccounts.ACCOUNT1.getGivenName()),
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.signin_promo_choose_another_account),
                mMediator.getModel().get(SigninPromoProperties.SECONDARY_BUTTON_TEXT));
        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON));
    }

    private void createSigninPromoMediator(SigninPromoDelegate delegate) {
        mProfileDataCache =
                spy(
                        ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                mContext, mIdentityManager));
        mMediator =
                new SigninPromoMediator(
                        mIdentityManager,
                        mSyncService,
                        AccountManagerFacadeProvider.getInstance(),
                        mProfileDataCache,
                        delegate);
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testPromoLayoutUpdatesAfterAccountAdded() {
        // Initial state: No accounts on the device.
        when(mSigninManager.isSigninAllowed()).thenReturn(true);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);

        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(), mProfile, mLauncher, () -> {});
        createSigninPromoMediator(delegate);

        // Verify initial cold state
        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_ACCOUNT_PICKER));
        assertEquals(null, mMediator.getModel().get(SigninPromoProperties.PROFILE_DATA));

        // Simulate adding an account
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(
                        TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(TestAccounts.ACCOUNT1);

        // Ensure ProfileDataCache spy returns data for the new account
        DisplayableProfileData profileData =
                new DisplayableProfileData(
                        TestAccounts.ACCOUNT1.getEmail(),
                        new BitmapDrawable(TestAccounts.ACCOUNT1.getAccountImage()),
                        TestAccounts.ACCOUNT1.getFullName(),
                        TestAccounts.ACCOUNT1.getGivenName(),
                        true);
        doReturn(profileData)
                .when(mProfileDataCache)
                .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail());

        // Simulate the ProfileDataCache observer notification
        mMediator.onProfileDataUpdated(TestAccounts.ACCOUNT1.getEmail());

        // Verify UI properties are updated for the signed-in state
        assertEquals(profileData, mMediator.getModel().get(SigninPromoProperties.PROFILE_DATA));
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_ACCOUNT_PICKER));
    }
}
