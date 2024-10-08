// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render test for {@link PromoCardView}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class PromoCardViewRenderTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .setRevision(1)
                    .build();

    public PromoCardViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    private PromoCardCoordinator mPromoCardCoordinator;
    private PropertyModel mModel;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        Activity activity = getActivity();

        mModel =
                new PropertyModel.Builder(PromoCardProperties.ALL_KEYS)
                        .with(
                                PromoCardProperties.IMAGE,
                                activity,
                                R.drawable.test_logo_avatar_anonymous)
                        .with(PromoCardProperties.TITLE, "Title for Promo Card.")
                        .with(PromoCardProperties.DESCRIPTION, "Description for Promo Card.")
                        .with(PromoCardProperties.PRIMARY_BUTTON_TEXT, "Primary button")
                        .with(PromoCardProperties.SECONDARY_BUTTON_TEXT, "Secondary button")
                        .build();
    }

    private void setPromoCard(@LayoutStyle int variance) {
        Activity activity = getActivity();

        mPromoCardCoordinator =
                PromoCardCoordinator.create(activity, mModel, "render-test", variance);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set the content and add the promo card into the window
                    LinearLayout content = new LinearLayout(activity);
                    activity.setContentView(content);
                    content.addView(
                            mPromoCardCoordinator.getView(),
                            new LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT));
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testLarge_Default() throws Exception {
        Drawable illustration =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_illustration);
        mModel.set(PromoCardProperties.IMAGE, illustration);
        setPromoCard(LayoutStyle.LARGE);
        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_default");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testLarge_ButtonsWidth() throws Exception {
        Drawable illustration =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_illustration);
        mModel.set(PromoCardProperties.IMAGE, illustration);
        mModel.set(PromoCardProperties.BUTTONS_WIDTH, LayoutParams.WRAP_CONTENT);
        setPromoCard(LayoutStyle.LARGE);

        CriteriaHelper.pollUiThread(
                () -> {
                    LayoutParams layoutParams =
                            mPromoCardCoordinator
                                    .getView()
                                    .findViewById(R.id.promo_primary_button)
                                    .getLayoutParams();
                    Criteria.checkThat(layoutParams.width, Matchers.is(LayoutParams.WRAP_CONTENT));
                });

        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_buttons_width");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testLarge_HideSecondaryButton() throws Exception {
        Drawable illustration =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_illustration);
        mModel.set(PromoCardProperties.IMAGE, illustration);
        mModel.set(PromoCardProperties.HAS_SECONDARY_BUTTON, false);
        setPromoCard(LayoutStyle.LARGE);

        CriteriaHelper.pollUiThread(
                () -> {
                    int visibility =
                            mPromoCardCoordinator
                                    .getView()
                                    .findViewById(R.id.promo_secondary_button)
                                    .getVisibility();
                    Criteria.checkThat(visibility, Matchers.is(View.GONE));
                });

        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_secondary_hidden");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testLarge_ShowCloseButton() throws Exception {
        Drawable illustration =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_illustration);
        mModel.set(PromoCardProperties.IMAGE, illustration);
        mModel.set(PromoCardProperties.HAS_CLOSE_BUTTON, true);
        setPromoCard(LayoutStyle.LARGE);

        CriteriaHelper.pollUiThread(
                () -> {
                    int visibility =
                            mPromoCardCoordinator
                                    .getView()
                                    .findViewById(R.id.promo_close_button)
                                    .getVisibility();
                    Criteria.checkThat(visibility, Matchers.is(View.VISIBLE));
                });

        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_close_shown");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testCompact() throws Exception {
        setPromoCard(LayoutStyle.COMPACT);
        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_compact");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testCompact_Stack() throws Exception {
        mModel.set(PromoCardProperties.PRIMARY_BUTTON_TEXT, "Long text for primary button");
        mModel.set(PromoCardProperties.SECONDARY_BUTTON_TEXT, "Long text for secondary button");
        setPromoCard(LayoutStyle.COMPACT);

        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_compact_stack");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testSlim() throws Exception {
        setPromoCard(LayoutStyle.SLIM);
        mRenderTestRule.render(mPromoCardCoordinator.getView(), "promo_card_slim");
    }
}
