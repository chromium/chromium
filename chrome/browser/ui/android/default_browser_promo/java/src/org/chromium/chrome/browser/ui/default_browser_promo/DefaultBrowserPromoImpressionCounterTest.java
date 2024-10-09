// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.text.format.DateUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.search_engines.SearchEngineChoiceService;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DefaultBrowserPromoImpressionCounterTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mClockRule = new FakeTimeTestRule();

    @Mock private SearchEngineChoiceService mMockSearchEngineChoiceService;

    private DefaultBrowserPromoImpressionCounter mCounter;
    private SharedPreferencesManager mSharedPreferenceManager;

    @Before
    public void setUp() {
        mCounter = Mockito.spy(new DefaultBrowserPromoImpressionCounter());

        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        mSharedPreferenceManager.removeKey(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_PROMOED_COUNT);
        mSharedPreferenceManager.removeKey(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_LAST_PROMO_TIME);

        doReturn(false).when(mMockSearchEngineChoiceService).isDefaultBrowserPromoSuppressed();
        SearchEngineChoiceService.setInstanceForTests(mMockSearchEngineChoiceService);
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testGetMaxPromoCount_ExperimentDisabled() {
        Assert.assertEquals(1, mCounter.getMaxPromoCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testGetMaxPromoCount_ExperimentEnabled() {
        Assert.assertEquals(3, mCounter.getMaxPromoCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testMinPromoInterval_FirstPromo() {
        when(mCounter.getPromoCount()).thenReturn(0);
        Assert.assertEquals(0, mCounter.getMinPromoInterval());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testMinPromoInterval_SecondPromo() {
        when(mCounter.getPromoCount()).thenReturn(1);
        // 3 days in minutes = 4320.
        Assert.assertEquals(4320, mCounter.getMinPromoInterval());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testMinPromoInterval_ThirdPromo() {
        when(mCounter.getPromoCount()).thenReturn(2);
        // 6 days (2 prior promos * 3 day interval) in minutes is 8640.
        Assert.assertEquals(8640, mCounter.getMinPromoInterval());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testSessionInterval_FirstPromo() {
        when(mCounter.getPromoCount()).thenReturn(0);
        Assert.assertEquals(3, mCounter.getMinSessionCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testSessionInterval_SecondPromo() {
        when(mCounter.getPromoCount()).thenReturn(1);
        // Code assumes 3 session for 1st promo shown + 2 session interval = 5.
        Assert.assertEquals(5, mCounter.getMinSessionCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testSessionInterval_ThirdPromo() {
        when(mCounter.getPromoCount()).thenReturn(2);
        when(mCounter.getLastPromoSessionCount()).thenReturn(10);
        // 10 session count at last promo + 2 session interval = 12.
        Assert.assertEquals(12, mCounter.getMinSessionCount());
    }

    @Test
    public void testFeatureParams() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID,
                DefaultBrowserPromoImpressionCounter.MAX_PROMO_COUNT_PARAM,
                "5");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID,
                DefaultBrowserPromoImpressionCounter.PROMO_TIME_INTERVAL_DAYS_PARAM,
                "6");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID,
                DefaultBrowserPromoImpressionCounter.PROMO_SESSION_INTERVAL_PARAM,
                "5");
        FeatureList.setTestValues(testValues);

        when(mCounter.getPromoCount()).thenReturn(4);
        when(mCounter.getLastPromoSessionCount()).thenReturn(20);

        Assert.assertEquals("Incorrect max promo count.", 5, mCounter.getMaxPromoCount());

        // Min interval is 24 days in minutes: promo count (4) times interval of 6 days in minutes
        // (8640 minutes).
        Assert.assertEquals("Incorrect min promo interval.", 34560, mCounter.getMinPromoInterval());

        // Min session count is session count at least promo (20) plus min interval of 5.
        Assert.assertEquals("Incorrect min session count.", 25, mCounter.getMinSessionCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testOnPromoShown() {
        int testSessionCount = 3;
        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, testSessionCount);

        mCounter.onPromoShown();
        Assert.assertEquals(mCounter.getPromoCount(), 1);
        Assert.assertEquals(mCounter.getLastPromoInterval(), 0);
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount);

        // Advance 3 days, last interval is 3 days = 4320 minutes
        mClockRule.advanceMillis(DateUtils.DAY_IN_MILLIS * 3);
        Assert.assertEquals(mCounter.getLastPromoInterval(), 4320);

        // Increase session count, getLastPromoSessionCount stays the same.
        DefaultBrowserPromoUtils.incrementSessionCount();
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount);

        // Show another promo
        mCounter.onPromoShown();
        Assert.assertEquals(mCounter.getPromoCount(), 2);
        Assert.assertEquals(mCounter.getLastPromoInterval(), 0);
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount + 1);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID)
    public void testShouldShowPromo() {
        // Initial state, should not show promo immediately.
        Assert.assertEquals(mCounter.getSessionCount(), 0);
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), 0);
        Assert.assertEquals(mCounter.getLastPromoInterval(), Integer.MAX_VALUE);
        Assert.assertEquals(mCounter.getMinPromoInterval(), 0);
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // Increase session to 3, can show promo
        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, 3);
        Assert.assertTrue(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // Show promo, and check immediately, should not show promo immediately.
        mCounter.onPromoShown();
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // Increase session count by 2, still not show promo
        DefaultBrowserPromoUtils.incrementSessionCount();
        DefaultBrowserPromoUtils.incrementSessionCount();
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // Advance 3 days, can show the 2nd promo
        mClockRule.advanceMillis(DateUtils.DAY_IN_MILLIS * 3);
        Assert.assertTrue(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // If the SearchEngineChoiceService suppresses it, it should not show.
        doReturn(true).when(mMockSearchEngineChoiceService).isDefaultBrowserPromoSuppressed();
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        // Lift the suppression and trigger the 2nd display
        doReturn(false).when(mMockSearchEngineChoiceService).isDefaultBrowserPromoSuppressed();
        mCounter.onPromoShown();

        // Advance 6 days, not showing the 3rd promo
        mClockRule.advanceMillis(DateUtils.DAY_IN_MILLIS * 6);
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));
        // Increase session count by 2, can show the 3rd promo
        DefaultBrowserPromoUtils.incrementSessionCount();
        DefaultBrowserPromoUtils.incrementSessionCount();
        Assert.assertTrue(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));

        mCounter.onPromoShown();

        // Increase session count by 2, advance 9 days, not showing the promo because reaching the
        // max count.
        DefaultBrowserPromoUtils.incrementSessionCount();
        DefaultBrowserPromoUtils.incrementSessionCount();
        mClockRule.advanceMillis(DateUtils.DAY_IN_MILLIS * 9);
        Assert.assertFalse(mCounter.shouldShowPromo(/* ignoreMaxCount= */ false));
        Assert.assertTrue(mCounter.shouldShowPromo(/* ignoreMaxCount= */ true));
    }
}
