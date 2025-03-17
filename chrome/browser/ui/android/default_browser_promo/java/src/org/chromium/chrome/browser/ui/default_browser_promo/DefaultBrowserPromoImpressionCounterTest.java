// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.text.format.DateUtils;

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
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
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

    @Test
    public void testGetMaxPromoCount() {
        Assert.assertEquals(3, mCounter.getMaxPromoCount());
    }

    @Test
    public void testMinPromoInterval_FirstPromo() {
        when(mCounter.getPromoCount()).thenReturn(0);
        Assert.assertEquals(0, mCounter.getMinPromoInterval());
    }

    @Test
    public void testMinPromoInterval_SecondPromo() {
        when(mCounter.getPromoCount()).thenReturn(1);
        // 3 days in minutes = 4320.
        Assert.assertEquals(4320, mCounter.getMinPromoInterval());
    }

    @Test
    public void testMinPromoInterval_ThirdPromo() {
        when(mCounter.getPromoCount()).thenReturn(2);
        // 6 days (2 prior promos * 3 day interval) in minutes is 8640.
        Assert.assertEquals(8640, mCounter.getMinPromoInterval());
    }

    @Test
    public void testSessionInterval_FirstPromo() {
        when(mCounter.getPromoCount()).thenReturn(0);
        Assert.assertEquals(3, mCounter.getMinSessionCount());
    }

    @Test
    public void testSessionInterval_SecondPromo() {
        when(mCounter.getPromoCount()).thenReturn(1);
        // Code assumes 3 session for 1st promo shown + 2 session interval = 5.
        Assert.assertEquals(5, mCounter.getMinSessionCount());
    }

    @Test
    public void testSessionInterval_ThirdPromo() {
        when(mCounter.getPromoCount()).thenReturn(2);
        when(mCounter.getLastPromoSessionCount()).thenReturn(10);
        // 10 session count at last promo + 2 session interval = 12.
        Assert.assertEquals(12, mCounter.getMinSessionCount());
    }

    @Test
    public void testOnPromoShown() {
        int testSessionCount = 3;
        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, testSessionCount);

        mCounter.onPromoShown();
        Assert.assertEquals(1, mCounter.getPromoCount());
        Assert.assertEquals(0, mCounter.getLastPromoInterval());
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount);

        // Advance 3 days, last interval is 3 days = 4320 minutes
        mClockRule.advanceMillis(DateUtils.DAY_IN_MILLIS * 3);
        Assert.assertEquals(4320, mCounter.getLastPromoInterval());

        // Increase session count, getLastPromoSessionCount stays the same.
        DefaultBrowserPromoUtils.incrementSessionCount();
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount);

        // Show another promo
        mCounter.onPromoShown();
        Assert.assertEquals(2, mCounter.getPromoCount());
        Assert.assertEquals(0, mCounter.getLastPromoInterval());
        Assert.assertEquals(mCounter.getLastPromoSessionCount(), testSessionCount + 1);
    }

    @Test
    public void testShouldShowPromo() {
        // Initial state, should not show promo immediately.
        Assert.assertEquals(0, mCounter.getSessionCount());
        Assert.assertEquals(0, mCounter.getLastPromoSessionCount());
        Assert.assertEquals(Integer.MAX_VALUE, mCounter.getLastPromoInterval());
        Assert.assertEquals(0, mCounter.getMinPromoInterval());
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
