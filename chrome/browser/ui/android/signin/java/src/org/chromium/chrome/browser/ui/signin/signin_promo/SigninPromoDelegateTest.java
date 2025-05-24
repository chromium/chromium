// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
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

import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoDelegateTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private @Mock Profile mProfile;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock Runnable mOnOpenSettings;
    private @Mock IdentityServicesProvider mIdentityServicesProvider;
    private @Mock IdentityManager mIdentityManager;
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
                mDelegate.getDescription(),
                mContext.getString(R.string.signin_promo_description_bookmarks));
    }

    @Test
    public void testNtpPromoShown() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, /* visibleAccount= */ null);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkAccountSettingsPromoShown_hasPrimaryAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.BOOKMARK_MANAGER, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
        assertEquals(mDelegate.getTitle(), mContext.getString(R.string.sync_promo_title_bookmarks));
        assertEquals(
                mDelegate.getDescription(),
                mContext.getString(R.string.account_settings_promo_description_bookmarks));
    }

    @Test
    public void testNtpPromoHidden_hasPrimaryAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO, TestAccounts.ACCOUNT1);

        assertFalse(mDelegate.canShowPromo());
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
    public void testHistoryPagePromoHidden_signedOut() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoShown_hasPrimaryAccount_historySyncAvailable() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(false).when(mHistorySyncHelper).shouldSuppressHistorySync();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, TestAccounts.ACCOUNT1);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoHidden_hasPrimaryAccount_historySyncSuppressed() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldSuppressHistorySync();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        setupDelegate(SigninAccessPoint.HISTORY_PAGE, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testHistoryPagePromoHidden_hasPrimaryAccount_historySyncDeclinedOften() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
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
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.RECENT_TABS, /* visibleAccount= */ null);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testRecentTabsPromoShown_hasPrimaryAccount() {
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
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
        doReturn(true).when(mHistorySyncHelper).shouldSuppressHistorySync();
        doReturn(true).when(mSigninManager).isSigninAllowed();
        setupDelegate(SigninAccessPoint.RECENT_TABS, /* visibleAccount= */ null);

        assertFalse(mDelegate.canShowPromo());
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
