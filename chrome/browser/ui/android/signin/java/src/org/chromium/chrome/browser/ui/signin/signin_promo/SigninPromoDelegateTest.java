// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;

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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoDelegateTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock Profile mProfile;
    private @Mock SigninAndHistorySyncActivityLauncher mLauncher;
    private @Mock Runnable mOnPromoStateChange;
    private @Mock IdentityServicesProvider mIdentityServicesProvider;
    private @Mock IdentityManager mIdentityManager;
    private @Mock SigninManager mSigninManager;
    private @Mock SyncService mSyncService;

    private SigninPromoDelegate mDelegate;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(mProfile);
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(mProfile);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SyncPromoAccessPointId.BOOKMARKS));
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SyncPromoAccessPointId.NTP));
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED);
    }

    @Test
    public void testBookmarkPromoShown() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        createDelegate(SigninAccessPoint.BOOKMARK_MANAGER);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoShown() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        createDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        assertTrue(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_hasPrimaryAccount() {
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        createDelegate(SigninAccessPoint.BOOKMARK_MANAGER);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_hasPrimaryAccount() {
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(ConsentLevel.SIGNIN);
        createDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_signInNotAllowed() {
        createDelegate(SigninAccessPoint.BOOKMARK_MANAGER);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_signInNotAllowed() {
        createDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_typeManagedByPolicy() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        doReturn(true).when(mSyncService).isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        createDelegate(SigninAccessPoint.BOOKMARK_MANAGER);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testBookmarkPromoHidden_dismissedBefore() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, true);
        createDelegate(SigninAccessPoint.BOOKMARK_MANAGER);

        assertFalse(mDelegate.canShowPromo());
    }

    @Test
    public void testNtpPromoHidden_dismissedBefore() {
        doReturn(true).when(mSigninManager).isSigninAllowed();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);
        createDelegate(SigninAccessPoint.NTP_FEED_TOP_PROMO);

        assertFalse(mDelegate.canShowPromo());
    }

    private void createDelegate(@SigninAccessPoint int accessPoint) {
        Context context = ApplicationProvider.getApplicationContext();
        mDelegate =
                switch (accessPoint) {
                    case SigninAccessPoint.BOOKMARK_MANAGER -> new BookmarkSigninPromoDelegate(
                            context, mProfile, mLauncher, mOnPromoStateChange);
                    case SigninAccessPoint.NTP_FEED_TOP_PROMO -> new NtpSigninPromoDelegate(
                            context, mProfile, mLauncher, mOnPromoStateChange);
                    case SigninAccessPoint.RECENT_TABS -> new RecentTabsSigninPromoDelegate(
                            context, mProfile, mLauncher, mOnPromoStateChange);
                    default -> throw new IllegalArgumentException();
                };
    }
}
