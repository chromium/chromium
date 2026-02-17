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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
public class SigninPromoMediatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private @Mock SyncService mSyncService;
    private @Mock SigninPromoDelegate mPromoDelegate;
    private @Mock SigninPromoMediator.Delegate mMediatorDelegate;
    private @Mock Profile mProfile;
    private @Mock SigninManager mSigninManager;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock HistorySyncHelper mHistorySyncHelper;
    private ProfileDataCache mProfileDataCache;
    private final Context mContext = ApplicationProvider.getApplicationContext();
    // TODO(crbug.com/374683682): Add tests for sign-in and sign-out events
    private final FakeIdentityManager mIdentityManager =
            mAccountManagerTestRule.getIdentityManager();

    private SigninPromoMediator mMediator;

    @Before
    public void setUp() {
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        lenient().doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        lenient().doReturn(true).when(mPromoDelegate).canShowPromo();
    }

    @Test
    public void testSecondaryButtonHiddenByDelegate() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mPromoDelegate).shouldHideSecondaryButton();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        createSigninPromoMediator(mPromoDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonHiddenByNullProfileData() {
        createSigninPromoMediator(mPromoDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertTrue(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromIdentityManager() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(false).when(mPromoDelegate).shouldHideSecondaryButton();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        createSigninPromoMediator(mPromoDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testSecondaryButtonShown_visibleAccountFromAccountManager() {
        doReturn(false).when(mPromoDelegate).shouldHideSecondaryButton();
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        createSigninPromoMediator(mPromoDelegate);

        boolean isSecondaryButtonHidden =
                mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON);
        assertFalse(isSecondaryButtonHidden);
    }

    @Test
    public void testDefaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        createSigninPromoMediator(mPromoDelegate);
        verify(mProfileDataCache, atLeastOnce())
                .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail());

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mProfileDataCache).getProfileDataOrDefault(TestAccounts.ACCOUNT2.getEmail());
    }

    @Test
    public void testDelegateUpdated_defaultAccountRemoved() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        createSigninPromoMediator(mPromoDelegate);

        // Set the mock delegate to return non-default values.
        String newTitle = "newTitle";
        String newDescription = "newDescription";
        String newPrimaryButtonText = "newPrimaryButtonText";
        String newSecondaryButtonText = "newSecondaryButtonText";
        doReturn(true).when(mPromoDelegate).refreshPromoState(any());
        doReturn(false).when(mPromoDelegate).canBeDismissedPermanently();
        doReturn(newTitle).when(mPromoDelegate).getTitle();
        doReturn(newDescription).when(mPromoDelegate).getDescription(any());
        doReturn(newPrimaryButtonText).when(mPromoDelegate).getTextForPrimaryButton(any());
        doReturn(newSecondaryButtonText).when(mPromoDelegate).getTextForSecondaryButton();
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
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mLauncher,
                        () -> {},
                        () -> false);

        createSigninPromoMediator(delegate);

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        assertEquals(
                mContext.getString(R.string.signin_promo_title_ntp_sign_in_as_button),
                mMediator.getModel().get(SigninPromoProperties.TITLE_TEXT));
        assertEquals(
                mContext.getString(R.string.custom_tabs_signed_out_message_subtitle),
                mMediator.getModel().get(SigninPromoProperties.DESCRIPTION_TEXT));
        assertEquals(
                mContext.getString(R.string.signin_promo_sign_in),
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
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mLauncher,
                        () -> {},
                        () -> false);
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

    @Test
    public void testCannotDismissPromo_UndoSigninNeverDismissesPromo() {
        doReturn(false).when(mPromoDelegate).canBeDismissedPermanently();
        createSigninPromoMediator(mPromoDelegate);

        mMediator.onSigninUndone();

        verify(mPromoDelegate, never()).permanentlyDismissPromo();
    }

    @Test
    public void testCanDismissPromo_UndoSigninDismissesPromo() {
        doReturn(true).when(mPromoDelegate).canBeDismissedPermanently();
        createSigninPromoMediator(mPromoDelegate);

        mMediator.onSigninUndone();

        verify(mPromoDelegate).permanentlyDismissPromo();
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testHideDismissButtonInLoadingState_Ntp() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSigninManager.isSigninAllowed()).thenReturn(true);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        NtpSigninPromoDelegate delegate =
                new NtpSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mLauncher,
                        () -> {},
                        () -> false);
        createSigninPromoMediator(delegate);

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        // In NTP the promo can be permanently dismissed
        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        String primaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String expectedPrimaryButtonText =
                mContext.getString(
                        R.string.signin_promo_sign_in_as, TestAccounts.ACCOUNT1.getGivenName());
        assertEquals(expectedPrimaryButtonText, primaryButtonText);

        mMediator.onFlowStarted();

        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        // In NTP the promo can be permanently dismissed
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        primaryButtonText = mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String expectedLoadingStatePrimaryButtonText =
                mContext.getString(R.string.signin_account_picker_bottom_sheet_signin_title);
        assertEquals(expectedLoadingStatePrimaryButtonText, primaryButtonText);

        mMediator.onFlowCompleted();

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        primaryButtonText = mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        assertEquals(expectedPrimaryButtonText, primaryButtonText);
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testHideDismissButtonInLoadingState_RecentTabs() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mSigninManager.isSigninAllowed()).thenReturn(true);
        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        RecentTabsSigninPromoDelegate delegate =
                new RecentTabsSigninPromoDelegate(
                        ApplicationProvider.getApplicationContext(), mProfile, mLauncher, () -> {});
        createSigninPromoMediator(delegate);

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        // In Recent Tabs the promo cannot be permanently dismissed
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        String primaryButtonText =
                mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String expectedPrimaryButtonText =
                mContext.getString(
                        R.string.signin_promo_sign_in_as, TestAccounts.ACCOUNT1.getGivenName());
        assertEquals(expectedPrimaryButtonText, primaryButtonText);

        mMediator.onFlowStarted();

        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        primaryButtonText = mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        String expectedLoadingStatePrimaryButtonText =
                mContext.getString(R.string.signin_account_picker_bottom_sheet_signin_title);
        assertEquals(expectedLoadingStatePrimaryButtonText, primaryButtonText);

        mMediator.onFlowCompleted();

        assertFalse(mMediator.getModel().get(SigninPromoProperties.SHOULD_SHOW_LOADING_STATE));
        // In Recent Tabs the promo cannot be permanently dismissed
        assertTrue(mMediator.getModel().get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON));
        primaryButtonText = mMediator.getModel().get(SigninPromoProperties.PRIMARY_BUTTON_TEXT);
        assertEquals(expectedPrimaryButtonText, primaryButtonText);
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
                        delegate,
                        mMediatorDelegate);
    }
}
