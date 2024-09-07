// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DefaultBrowserPromoImpressionCounterTest {
    private DefaultBrowserPromoImpressionCounter mCounter;

    @Before
    public void setUp() {
        mCounter = Mockito.spy(new DefaultBrowserPromoImpressionCounter());
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
}
