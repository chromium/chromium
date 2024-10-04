// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.settings_promo_card;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

/** Test for {@link DefaultBrowserPromoCard}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
public class DefaultBrowserPromoCardTest {
    @Mock private Tracker mTestTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;
    @Mock private Runnable mOnDisplayChangedCallback;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Activity mActivity;

    private DefaultBrowserPromoCard mPromoCard;

    @Before
    public void setup() {
        mActivity = spy(Robolectric.buildActivity(TestActivity.class).setup().get());
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    private DefaultBrowserPromoCard initializePromoCard() {
        DefaultBrowserPromoCard card =
                new DefaultBrowserPromoCard(
                        mActivity,
                        mMockDefaultBrowserPromoUtils,
                        mTestTracker,
                        mOnDisplayChangedCallback);
        View view = LayoutInflater.from(mActivity).inflate(R.layout.promo_card_view_large, null);
        card.setUpPromoCardView(view);
        return card;
    }

    @Test
    public void testShowPromo() {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(true).thenReturn(false);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);
        DefaultBrowserPromoCard card = initializePromoCard();
        Assert.assertTrue(card.isPromoShowing());
        Assert.assertNotNull(card.getView());

        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(false);
        card.updatePromoCard();
        Assert.assertFalse(card.isPromoShowing());
    }

    @Test
    public void testPromoNotShown() {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(false);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);

        DefaultBrowserPromoCard card =
                new DefaultBrowserPromoCard(
                        mActivity, mMockDefaultBrowserPromoUtils, mTestTracker, () -> {});
        Assert.assertFalse(card.isPromoShowing());
    }

    @Test
    public void testClickingDismissButton() {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(true);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);

        DefaultBrowserPromoCard card = initializePromoCard();

        ((ImageButton) card.getView().findViewById(R.id.promo_close_button)).performClick();

        Assert.assertFalse(card.isPromoShowing());
        verify(mOnDisplayChangedCallback, times(1)).run();
        verify(mTestTracker, times(1)).notifyEvent("default_browser_promo_setting_card_dismissed");
    }

    @Test
    public void testClickingPrimaryButton() {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(true);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);

        DefaultBrowserPromoCard card = initializePromoCard();

        ((Button) card.getView().findViewById(R.id.promo_primary_button)).performClick();
        verify(mActivity, times(1)).startActivity(any(), any());
        verify(mTestTracker, times(1)).notifyEvent("default_browser_promo_setting_card_used");
    }
}
