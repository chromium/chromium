// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.format.DateUtils;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Collections;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoDelegateTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Captor private ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> mConfigCaptor;

    private @Mock Profile mProfile;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock Runnable mOnOpenSettings;
    private @Mock IdentityServicesProvider mIdentityServicesProvider;
    // TODO(crbug.com/374683682): Use fake IdentityManager instead.
    private @Mock(strictness = Mock.Strictness.LENIENT) IdentityManager mIdentityManager;
    private @Mock SigninManager mSigninManager;
    private @Mock SyncService mSyncService;
    private @Mock HistorySyncHelper mHistorySyncHelper;

    private Context mContext;
    private SigninPromoDelegate mDelegate;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        lenient()
                .doReturn(mIdentityManager)
                .when(mIdentityServicesProvider)
                .getIdentityManager(mProfile);
        lenient()
                .doReturn(mSigninManager)
                .when(mIdentityServicesProvider)
                .getSigninManager(mProfile);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS));
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.NTP));
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED);
    }

    @Test
    public void testBookmarkSigninPromoShown() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, /* visibleAccount= */ null);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(
                mDelegate.getTitle(), mContext.getString(R.string.signin_promo_title_bookmarks));
        assertEquals(
                mDelegate.getDescription(null),
                mContext.getString(R.string.signin_promo_description_bookmarks));
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/continueButton"
    })
    public void testNtpPromoShown() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);
        DisplayableProfileData profileData =
                new DisplayableProfileData(
                        "testemail@gmail.com",
                        mock(Drawable.class),
                        "TestName LastName",
                        "TestName",
                        true);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(
                mDelegate.getTitle(),
                mContext.getString(R.string.signin_account_picker_bottom_sheet_title));
        assertEquals(
                mDelegate.getDescription(profileData.getAccountEmail()),
                mContext.getString(
                        R.string.signin_promo_description_ntp_group1, "testemail@gmail.com"));
        assertEquals(
                mDelegate.getTextForPrimaryButton(profileData),
                mContext.getString(R.string.sync_promo_continue_as, "TestName"));
    }

    @Test
    public void testBookmarkAccountSettingsPromoShown_hasPrimaryAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(mDelegate.getTitle(), mContext.getString(R.string.sync_promo_title_bookmarks));
        assertEquals(
                mDelegate.getDescription(null),
                mContext.getString(R.string.account_settings_promo_description_bookmarks));
    }

    @Test
    public void
            testBookmarkAccountSettingsPromoHidden_readingListManagedByPolicyAndBookmarksEnabled() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(true).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.READING_LIST);
        doReturn(false).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        doReturn(Set.of(UserSelectableType.BOOKMARKS)).when(mSyncService).getSelectedTypes();
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void
            testBookmarkAccountSettingsPromoShown_readingListManagedByPolicyAndBookmarksDisabled() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        lenient()
                .doReturn(true)
                .when(mSyncService)
                .isTypeManagedByPolicy(UserSelectableType.READING_LIST);
        doReturn(false).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        doReturn(Collections.emptySet()).when(mSyncService).getSelectedTypes();
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_hasPrimaryAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void testNtpPromoShown_noAccountsOnDevice() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(
                mDelegate.getTitle(),
                mContext.getString(R.string.signin_account_picker_bottom_sheet_title));
        assertEquals(
                mDelegate.getDescription(null),
                mContext.getString(R.string.custom_tabs_signed_out_message_subtitle));
        assertEquals(
                mDelegate.getTextForPrimaryButton(null),
                mContext.getString(R.string.custom_tabs_signed_out_message_title));
    }

    @Test
    public void testBookmarkPromoHidden_signInNotAllowed() {
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_signInNotAllowed() {
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_typeManagedByPolicy() {
        doReturn(true).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        doReturn(true).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.READING_LIST);
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_typesAlreadyEnabled() {
        doReturn(Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST))
                .when(mSyncService)
                .getSelectedTypes();
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_dismissedBefore() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, true);
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testBookmarkPromoShown_accountAvailableOnDevice() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        DisplayableProfileData profileData =
                new DisplayableProfileData(
                        TestAccounts.ACCOUNT1.getEmail(),
                        mock(Drawable.class),
                        TestAccounts.ACCOUNT1.getFullName(),
                        TestAccounts.ACCOUNT1.getGivenName(),
                        true);
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(
                mDelegate.getTitle(), mContext.getString(R.string.signin_promo_title_bookmarks));
        assertEquals(
                mDelegate.getDescription(/* accountEmail= */ TestAccounts.ACCOUNT1.getEmail()),
                mContext.getString(
                        R.string.signin_promo_description_bookmarks_group3,
                        TestAccounts.ACCOUNT1.getEmail()));
        assertEquals(
                mDelegate.getTextForPrimaryButton(/* profileData= */ profileData),
                mContext.getString(
                        R.string.signin_promo_sign_in_as, TestAccounts.ACCOUNT1.getGivenName()));
    }

    @Test
    public void testHistoryPagePromoHidden_signedOut() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoShown_hasPrimaryAccount_historySyncAvailable() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoHidden_hasPrimaryAccount_historySyncSuppressed() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(false).when(mHistorySyncHelper).shouldDisplayHistorySync();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoHidden_hasPrimaryAccount_historySyncDeclinedOften() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        doReturn(true).when(mHistorySyncHelper).isDeclinedOften();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoHidden_hasPrimaryAccount_cct() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mDelegate =
                new HistoryPageSigninPromoDelegate(
                        mContext,
                        mProfile,
                        mLauncher,
                        mOnPromoStateChange,
                        /* isCreatedInCct= */ true);
        mDelegate.refreshPromoState(TestAccounts.ACCOUNT1);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_dismissedBefore() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_timeElapsedSinceFirstShownExceedsLimit() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        final long timeSinceFirstShownLimitMs =
                NtpSigninPromoDelegate.NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS
                        * DateUtils.HOUR_IN_MILLIS;
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME,
                        System.currentTimeMillis() - timeSinceFirstShownLimitMs);
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoShown_extendedAccountInfoAvailable() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .findExtendedAccountInfoByEmailAddress(TestAccounts.ACCOUNT1.getEmail());
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_extendedAccountInfoNotAvailable() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testRecentTabsPromoShown_signinAllowed() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.RECENT_TABS, /* visibleAccount= */ null);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testRecentTabsPromoShown_hasPrimaryAccount() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.RECENT_TABS, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testRecentTabsPromoHidden_signinNotAllowed() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        setupDelegate(SigninAccessPoint.RECENT_TABS, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testRecentTabsPromoHidden_suppressed() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(false).when(mHistorySyncHelper).shouldDisplayHistorySync();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.RECENT_TABS, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    public void testRecentTabsPromoShown_accountAvailableOnDevice() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        DisplayableProfileData profileData =
                new DisplayableProfileData(
                        TestAccounts.ACCOUNT1.getEmail(),
                        new BitmapDrawable(TestAccounts.ACCOUNT1.getAccountImage()),
                        TestAccounts.ACCOUNT1.getFullName(),
                        TestAccounts.ACCOUNT1.getGivenName(),
                        true);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.RECENT_TABS, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(
                mDelegate.getTitle(),
                mContext.getString(R.string.signin_history_sync_promo_title_recent_tabs));
        assertEquals(
                mDelegate.getDescription(/* accountEmail= */ TestAccounts.ACCOUNT1.getEmail()),
                mContext.getString(
                        R.string.signin_promo_description_recent_tabs_group3,
                        TestAccounts.ACCOUNT1.getEmail()));
        assertEquals(
                mDelegate.getTextForPrimaryButton(/* profileData= */ profileData),
                mContext.getString(
                        R.string.signin_promo_sign_in_as, TestAccounts.ACCOUNT1.getGivenName()));
    }

    @Test
    @EnableFeatures("EnableSeamlessSignin")
    public void testRecentTabsPromo_seamlessFlow_accountOnDevice_launchesSeamlessSignin() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldDisplayHistorySync();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setupDelegate(SigninAccessPoint.RECENT_TABS, TestAccounts.ACCOUNT1);
        assertTrue(mDelegate.canShowPromo());

        mDelegate.onPrimaryButtonClicked(TestAccounts.ACCOUNT1);

        BottomSheetSigninAndHistorySyncConfig config = getBottomSheetConfiguration();
        assertEquals(WithAccountSigninMode.SEAMLESS_SIGNIN, config.withAccountSigninMode);
    }

    @Test
    @EnableFeatures("EnableSeamlessSignin")
    public void testBookmarkPromo_seamlessFlow_accountOnDevice_launchesSeamlessSignin() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);
        assertTrue(mDelegate.canShowPromo());

        mDelegate.onPrimaryButtonClicked(TestAccounts.ACCOUNT1);

        BottomSheetSigninAndHistorySyncConfig config = getBottomSheetConfiguration();
        assertEquals(WithAccountSigninMode.SEAMLESS_SIGNIN, config.withAccountSigninMode);
    }

    @Test
    @EnableFeatures("EnableSeamlessSignin")
    public void testNtpPromo_seamlessFlow_accountOnDevice_launchesSeamlessSignin() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .findExtendedAccountInfoByEmailAddress(TestAccounts.ACCOUNT1.getEmail());
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);
        assertTrue(mDelegate.canShowPromo());

        mDelegate.onPrimaryButtonClicked(TestAccounts.ACCOUNT1);

        BottomSheetSigninAndHistorySyncConfig config = getBottomSheetConfiguration();
        assertEquals(WithAccountSigninMode.SEAMLESS_SIGNIN, config.withAccountSigninMode);
    }

    @Test
    @EnableFeatures("EnableSeamlessSignin")
    public void testNtpPromoLaunches_seamlessFlow_noAccountOnDevice_fallbacksToBottomSheet() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);
        assertTrue(mDelegate.canShowPromo());

        mDelegate.onPrimaryButtonClicked(null);

        BottomSheetSigninAndHistorySyncConfig config = getBottomSheetConfiguration();
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
    }

    @Test
    @EnableFeatures("EnableSeamlessSignin")
    public void testNtpPromo_seamlessFlow_accountOnDevice_secondaryButtonShowsSnackbar() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(TestAccounts.ACCOUNT1)
                .when(mIdentityManager)
                .findExtendedAccountInfoByEmailAddress(TestAccounts.ACCOUNT1.getEmail());
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);
        assertTrue(mDelegate.canShowPromo());

        mDelegate.onSecondaryButtonClicked();

        BottomSheetSigninAndHistorySyncConfig config = getBottomSheetConfiguration();
        assertEquals(
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertTrue(config.shouldShowSigninSnackbar);
    }

    private BottomSheetSigninAndHistorySyncConfig getBottomSheetConfiguration() {
        verify(mLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mContext),
                        eq(mProfile),
                        mConfigCaptor.capture(),
                        eq(mDelegate.getAccessPoint()));
        return mConfigCaptor.getValue();
    }

    private void setupDelegate(
            @SigninAccessPoint int accessPoint, @Nullable CoreAccountInfo visibleAccount) {
        mDelegate =
                switch (accessPoint) {
                    case SigninAccessPoint.BOOKMARK_MANAGER -> new BookmarkSigninPromoDelegate(
                            mContext, mProfile, mLauncher, mOnPromoStateChange, mOnOpenSettings);
                    case SigninAccessPoint.HISTORY_PAGE -> new HistoryPageSigninPromoDelegate(
                            mContext,
                            mProfile,
                            mLauncher,
                            mOnPromoStateChange,
                            /* isCreatedInCct= */ false);
                    case SigninAccessPoint.NTP_FEED_TOP_PROMO -> new NtpSigninPromoDelegate(
                            mContext, mProfile, mLauncher, mOnPromoStateChange);
                    case SigninAccessPoint.RECENT_TABS -> new RecentTabsSigninPromoDelegate(
                            mContext, mProfile, mLauncher, mOnPromoStateChange);
                    default -> throw new IllegalArgumentException();
                };
        mDelegate.refreshPromoState(visibleAccount);
    }
}
