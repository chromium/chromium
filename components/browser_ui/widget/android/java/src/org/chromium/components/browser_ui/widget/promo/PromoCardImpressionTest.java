// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/**
 * Tests targeting functionality to track impression on PromoCard component. TODO(wenyufu): Add the
 * test when the primary button is hidden initially.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PromoCardImpressionTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static FrameLayout sContent;

    private PropertyModel mModel;
    private PromoCardCoordinator mCoordinator;

    private final CallbackHelper mPromoSeenCallback = new CallbackHelper();

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sContent = new FrameLayout(sActivity);
                    sActivity.setContentView(sContent);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(() -> sContent.removeAllViews());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.destroy());
    }

    private void setUpPromoCard(boolean trackPrimary, boolean hidePromo) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel =
                            new PropertyModel.Builder(PromoCardProperties.ALL_KEYS)
                                    .with(
                                            PromoCardProperties.IMPRESSION_SEEN_CALLBACK,
                                            mPromoSeenCallback::notifyCalled)
                                    .with(
                                            PromoCardProperties.IS_IMPRESSION_ON_PRIMARY_BUTTON,
                                            trackPrimary)
                                    .with(PromoCardProperties.TITLE, "Title")
                                    .with(PromoCardProperties.DESCRIPTION, "Description")
                                    .with(PromoCardProperties.PRIMARY_BUTTON_TEXT, "Primary")
                                    .with(PromoCardProperties.HAS_SECONDARY_BUTTON, false)
                                    .build();

                    mCoordinator =
                            PromoCardCoordinator.create(sActivity, mModel, "impression-test");
                    View promoView = mCoordinator.getView();

                    if (hidePromo) promoView.setVisibility(View.INVISIBLE);
                    sContent.addView(
                            promoView,
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                });
    }

    @Test
    @SmallTest
    public void testImpression_Card_Seen() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(false, false);
        mPromoSeenCallback.waitForCallback("PromoCard is never seen.", initCount);
    }

    @Test
    @SmallTest
    public void testImpression_PrimaryButton_Seen() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(true, false);
        mPromoSeenCallback.waitForCallback("PromoCard's primary button is never seen.", initCount);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1181859")
    public void testImpression_Card_Hide() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(false, true);
        Assert.assertEquals(
                "Promo should not be seen yet.", initCount, mPromoSeenCallback.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.getView().setVisibility(View.VISIBLE);
                });
        mPromoSeenCallback.waitForCallback("PromoCard is never seen.", initCount);
    }
}
