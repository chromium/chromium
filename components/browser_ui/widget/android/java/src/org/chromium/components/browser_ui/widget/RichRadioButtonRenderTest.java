// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render test for {@link RichRadioButton}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class RichRadioButtonRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private static final int REVISION = 1;
    private static final String REVISION_DESCRIPTION =
            "Initial render test for RichRadioButton covering various states and orientations.";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(REVISION)
                    .setDescription(REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .build();

    private LinearLayout mLayout;

    private RichRadioButton mRichRbHorizontalFullUnchecked;
    private RichRadioButton mRichRbHorizontalTitleChecked;
    private RichRadioButton mRichRbHorizontalMinimalUnchecked;

    private final int mFakeBgColor;

    public RichRadioButtonRenderTest(boolean nightModeEnabled) {
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @AfterClass
    public static void tearDownSuite() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Before
    public void setUp() throws Exception {
        Activity activity = sActivity;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View content =
                            LayoutInflater.from(activity)
                                    .inflate(R.layout.rich_radio_button_render_test, null, false);
                    activity.setContentView(content);

                    mLayout = content.findViewById(R.id.test_rich_radio_button_layout);
                    mLayout.setBackgroundColor(mFakeBgColor);

                    int displayWidth = activity.getResources().getDisplayMetrics().widthPixels;
                    mLayout.measure(
                            View.MeasureSpec.makeMeasureSpec(
                                    displayWidth, View.MeasureSpec.EXACTLY),
                            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
                    mLayout.layout(0, 0, mLayout.getMeasuredWidth(), mLayout.getMeasuredHeight());

                    mRichRbHorizontalFullUnchecked =
                            mLayout.findViewById(R.id.rich_rb_horizontal_full_unchecked);
                    mRichRbHorizontalTitleChecked =
                            mLayout.findViewById(R.id.rich_rb_horizontal_title_checked);
                    mRichRbHorizontalMinimalUnchecked =
                            mLayout.findViewById(R.id.rich_rb_horizontal_minimal_unchecked);
                });

        Assert.assertNotNull(mLayout);
        Assert.assertNotNull(mRichRbHorizontalFullUnchecked);
        Assert.assertNotNull(mRichRbHorizontalTitleChecked);
        Assert.assertNotNull(mRichRbHorizontalMinimalUnchecked);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButton"})
    public void testRichRbHorizontalFullUnchecked() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRbHorizontalFullUnchecked.setItemData(
                            R.drawable.test_location_precise,
                            "Precise Location",
                            "Provides exact location coordinates.",
                            false);
                    mRichRbHorizontalFullUnchecked.setChecked(false);
                });
        mRenderTestRule.render(mRichRbHorizontalFullUnchecked, "rich_rb_horizontal_full_unchecked");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButton"})
    public void testRichRbHorizontalTitleChecked() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRbHorizontalTitleChecked.setItemData(
                            R.drawable.test_location_precise, "Simple Option", null, false);
                    mRichRbHorizontalTitleChecked.setChecked(true);
                });
        mRenderTestRule.render(mRichRbHorizontalTitleChecked, "rich_rb_horizontal_title_checked");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButton"})
    public void testRichRbHorizontalMinimalUnchecked() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRbHorizontalMinimalUnchecked.setItemData(0, "Minimal Option", null, false);
                    mRichRbHorizontalMinimalUnchecked.setChecked(false);
                });
        mRenderTestRule.render(
                mRichRbHorizontalMinimalUnchecked, "rich_rb_horizontal_minimal_unchecked");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButton"})
    public void testRichRbHorizontalFullChecked() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRbHorizontalFullUnchecked.setItemData(
                            R.drawable.test_location_precise,
                            "Checked Item",
                            "This item is now checked.",
                            false);
                    mRichRbHorizontalFullUnchecked.setChecked(true);
                });
        mRenderTestRule.render(mRichRbHorizontalFullUnchecked, "rich_rb_horizontal_full_checked");
    }
}
