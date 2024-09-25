// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.text.format.DateUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.List;
import java.util.Set;

/** Tests for {@link SyncPromoController}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS,
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
})
@EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
public class SyncPromoControllerTest {
    private static final int TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS =
            SyncPromoController.NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS;
    private static final long TIME_SINCE_FIRST_SHOWN_LIMIT_MS =
            TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS * DateUtils.HOUR_IN_MILLIS;
    private static final int RESET_AFTER_DAYS = SyncPromoController.NTP_SYNC_PROMO_RESET_AFTER_DAYS;
    private static final long RESET_AFTER_MS = RESET_AFTER_DAYS * DateUtils.DAY_IN_MILLIS;
    private static final int MAX_SIGN_IN_PROMO_IMPRESSIONS =
            SyncPromoController.SYNC_ANDROID_NTP_PROMO_MAX_IMPRESSIONS;

    private static final AccountPickerBottomSheetStrings BOTTOM_SHEET_STRINGS =
            new AccountPickerBottomSheetStrings.Builder(
                            R.string.signin_account_picker_bottom_sheet_title)
                    .build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock private Profile mProfile;

    @Mock private IdentityServicesProvider mIdentityServicesProvider;

    @Mock private IdentityManager mIdentityManager;

    @Mock private SigninManager mSigninManager;

    @Mock private SyncService mSyncService;

    @Mock private PrefService mPrefService;

    @Mock private HistorySyncHelper mHistorySyncHelper;

    @Mock private SyncConsentActivityLauncher mSyncConsentActivityLauncher;

    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;

    private final SharedPreferencesManager mSharedPreferencesManager =
            ChromeSharedPreferences.getInstance();
    private final AccountCapabilitiesBuilder mAccountCapabilitiesBuilder =
            new AccountCapabilitiesBuilder();

    private SyncPromoController mSyncPromoController;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(mProfile))
                .thenReturn(mIdentityManager);
        mSharedPreferencesManager.writeInt(
                SyncPromoController.getPromoShowCountPreferenceName(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO),
                0);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
        when(mPrefService.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_GAIA_ID)).thenReturn("");

        mSyncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldShowSigninPromoForNTP_noAccountsOnDevice() {
        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldNotShowNTPSigninPromo_alreadySignedIn() {
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);

        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNoAccountOnDevice() {
        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldHideSyncPromoForNTPWhenDefaultAccountCannotShowHistoryOptInWithoutRestrictions() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldShowPromoForNTPWhenDefaultAccountCannotShowHistoryOptInWithoutRestrictions_replaceSyncBySigninEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCapabilityIsNotFetched() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldShowPromoForNTPWhenDefaultAccountCapabilityIsNotFetched_replaceSyncBySigninEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void
            shouldShowPromoForNTPWhenSecondaryAccountCannotShowHistoryOptInWithoutRestrictions() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowNTPSyncPromoWhenCountLimitIsNotExceeded() {
        mSharedPreferencesManager.writeInt(
                SyncPromoController.getPromoShowCountPreferenceName(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO),
                MAX_SIGN_IN_PROMO_IMPRESSIONS - 1);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldHideNTPSyncPromoWhenCountLimitIsExceeded() {
        mSharedPreferencesManager.writeInt(
                SyncPromoController.getPromoShowCountPreferenceName(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO),
                MAX_SIGN_IN_PROMO_IMPRESSIONS);

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowNTPSyncPromoWhenTimeSinceFirstShownLimitIsNotExceeded() {
        final long firstShownTime = System.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldHideNTPSyncPromoWhenTimeSinceFirstShownLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldNotResetLimitsWhenResetTimeLimitIsNotExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis();
        disableNTPSyncPromoBySettingLimits(firstShownTime, lastShownTime);
        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());

        SyncPromoController.resetNtpSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
        Assert.assertEquals(
                firstShownTime,
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(
                lastShownTime,
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(
                MAX_SIGN_IN_PROMO_IMPRESSIONS,
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                SyncPromoController.getPromoShowCountPreferenceName(
                                        SigninAccessPoint.NTP_FEED_TOP_PROMO)));
    }

    @Test
    public void shouldResetLimitsWhenResetTimeLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis() - RESET_AFTER_MS - 1;
        disableNTPSyncPromoBySettingLimits(firstShownTime, lastShownTime);
        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());

        SyncPromoController.resetNtpSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
        Assert.assertEquals(
                0L,
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(
                0L,
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                SyncPromoController.getPromoShowCountPreferenceName(
                                        SigninAccessPoint.NTP_FEED_TOP_PROMO)));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNotDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);

        Assert.assertTrue(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);

        Assert.assertFalse(mSyncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldHideBookmarksSyncPromoIfBookmarksIsManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(true);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowBookmarksSyncPromoIfBookmarksIsNotManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldHideBookmarksSyncPromoIfDataTypesSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST));

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowBookmarksSyncPromoIfBookmarkNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.READING_LIST));

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    public void shouldShowBookmarksSyncPromoIfReadingListNotSyncing() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS)).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldHideRecentTabsSyncPromoIfTabsIsManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(true);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldHideRecentTabsSyncPromoIfTabsIsManagedByPolicy_replaceSyncPromosWithSigninPromosEnabled() {
        when(IdentityServicesProvider.get().getSigninManager(mProfile)).thenReturn(mSigninManager);
        doReturn(true).when(mSigninManager).isSigninAllowed();
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        when(mHistorySyncHelper.shouldSuppressHistorySync()).thenReturn(true);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldShowRecentTabsSyncPromoIfTabsIsNotManagedByPolicy() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS)).thenReturn(false);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldHideRecentTabsIfUserAlreadyOptedIn_replaceSyncPromosWithSigninPromosEnabled() {
        when(IdentityServicesProvider.get().getSigninManager(mProfile)).thenReturn(mSigninManager);
        doReturn(false).when(mSigninManager).isSigninAllowed();
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        when(mHistorySyncHelper.shouldSuppressHistorySync()).thenReturn(true);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertFalse(syncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldShowRecentTabsHistorySyncPromoIfTabsIsNotManagedByPolicyAndUserIsNotSignedIn_replaceSyncPromosWithSigninPromosEnabled() {
        when(IdentityServicesProvider.get().getSigninManager(mProfile)).thenReturn(mSigninManager);
        doReturn(true).when(mSigninManager).isSigninAllowed();
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            shouldShowRecentTabsSyncPromoIfUserSignedInButDidNotOptIn_replaceSyncPromosWithSigninPromosEnabled() {
        when(IdentityServicesProvider.get().getSigninManager(mProfile)).thenReturn(mSigninManager);
        doReturn(false).when(mSigninManager).isSigninAllowed();
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);

        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        BOTTOM_SHEET_STRINGS,
                        SigninAccessPoint.RECENT_TABS,
                        mSyncConsentActivityLauncher,
                        mSigninAndHistorySyncActivityLauncher);
        Assert.assertTrue(syncPromoController.canShowSyncPromo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void shouldLaunchBookmarksSigninFlowReturnsTrue_UnoFeatureEnabled() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertTrue(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    public void shouldLaunchBookmarksSigninFlowReturnsTrue_AccountStorageFeatureEnabled() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertTrue(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    @DisableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_AccountStorageFeatureDisabled() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_NotBookmarkAccessPoint() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.NTP_FEED_TOP_PROMO,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_SignedIn() {
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_SyncDataLeft() {
        when(mPrefService.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_GAIA_ID))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_1.getGaiaId());
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_ACCOUNT_1),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_NonGmailDomain() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn("nongmail.com").when(mSigninManager).extractDomainName(anyString());
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_NON_GMAIL_ACCOUNT);

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(AccountManagerTestRule.TEST_NON_GMAIL_ACCOUNT),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_EmptyAccountList() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        List.of(),
                        mPrefService));
    }

    @Test
    @EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
    public void shouldLaunchBookmarksSigninFlowReturnsFalse_NullAccountList() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(SyncPromoController.GMAIL_DOMAIN)
                .when(mSigninManager)
                .extractDomainName(anyString());

        Assert.assertFalse(
                SyncPromoController.shouldLaunchSigninFlow(
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        mIdentityManager,
                        mSigninManager,
                        null,
                        mPrefService));
    }

    private void disableNTPSyncPromoBySettingLimits(long firstShownTime, long lastShownTime) {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        SyncPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_FEED_TOP_PROMO),
                        MAX_SIGN_IN_PROMO_IMPRESSIONS);
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, lastShownTime);
    }
}
