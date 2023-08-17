// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.Mockito.mock;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Tests for {@link SyncPromoController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
        ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS,
        ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS,
})
public class SyncPromoControllerTest {
    private static final int TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS = 100;
    private static final long TIME_SINCE_FIRST_SHOWN_LIMIT_MS =
            TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS * DateUtils.HOUR_IN_MILLIS;
    private static final int RESET_AFTER_HOURS = 10;
    private static final long RESET_AFTER_MS = RESET_AFTER_HOURS * DateUtils.HOUR_IN_MILLIS;
    private static final int MAX_SIGN_IN_PROMO_IMPRESSIONS = 10;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock
    private IdentityManager mIdentityManagerMock;

    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();
    private final AccountCapabilitiesBuilder mAccountCapabilitiesBuilder =
            new AccountCapabilitiesBuilder();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(Profile.getLastUsedRegularProfile()))
                .thenReturn(mIdentityManagerMock);
        mSharedPreferencesManager.writeInt(SyncPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                0);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNoAccountOnDevice() {
        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCannotOfferSyncPromos() {
        mAccountManagerTestRule.addAccount("test1@gmail.com",
                mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(false).build());
        mAccountManagerTestRule.addAccount("test2@gmail.com",
                mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(true).build());

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCapabilityIsNotFetched() {
        mAccountManagerTestRule.addAccount("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount("test.account.secondary@gmail.com");

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenSecondaryAccountCannotOfferSyncPromos() {
        mAccountManagerTestRule.addAccount("test1@gmail.com",
                mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(true).build());
        mAccountManagerTestRule.addAccount("test2@gmail.com",
                mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(false).build());

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWhenCountLimitIsNotExceeded() {
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT.setForTesting(
                MAX_SIGN_IN_PROMO_IMPRESSIONS);
        mSharedPreferencesManager.writeInt(SyncPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS - 1);

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideNTPSyncPromoWhenCountLimitIsExceeded() {
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_COUNT_LIMIT.setForTesting(
                MAX_SIGN_IN_PROMO_IMPRESSIONS);
        mSharedPreferencesManager.writeInt(SyncPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS);

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWithoutTimeSinceFirstShownLimit() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                -1);

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWhenTimeSinceFirstShownLimitIsNotExceeded() {
        final long firstShownTime = System.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideNTPSyncPromoWhenTimeSinceFirstShownLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldNotResetLimitsWithoutResetTimeLimit() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis() - RESET_AFTER_MS - 1;
        disableNTPSyncPromoBySettingLimits(
                firstShownTime, lastShownTime, /*signinPromoResetAfterHours=*/-1);
        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SyncPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(firstShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(lastShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(MAX_SIGN_IN_PROMO_IMPRESSIONS,
                SharedPreferencesManager.getInstance().readInt(
                        SyncPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldNotResetLimitsWhenResetTimeLimitIsNotExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis();
        disableNTPSyncPromoBySettingLimits(firstShownTime, lastShownTime, RESET_AFTER_HOURS);
        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SyncPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(firstShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(lastShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(MAX_SIGN_IN_PROMO_IMPRESSIONS,
                SharedPreferencesManager.getInstance().readInt(
                        SyncPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldResetLimitsWhenResetTimeLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis() - RESET_AFTER_MS - 1;
        disableNTPSyncPromoBySettingLimits(firstShownTime, lastShownTime, RESET_AFTER_HOURS);
        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SyncPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(0L,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(0L,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(0,
                SharedPreferencesManager.getInstance().readInt(
                        SyncPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNotDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);

        Assert.assertTrue(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);

        Assert.assertFalse(
                SyncPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    private void disableNTPSyncPromoBySettingLimits(
            long firstShownTime, long lastShownTime, int signinPromoResetAfterHours) {
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS.setForTesting(
                signinPromoResetAfterHours);

        SharedPreferencesManager.getInstance().writeInt(
                SyncPromoController.getPromoShowCountPreferenceName(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, lastShownTime);
    }
}
