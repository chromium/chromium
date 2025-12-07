// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
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
import org.chromium.base.test.util.DisabledTest;
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

    private static final int REVISION = 3;
    private static final String REVISION_DESCRIPTION =
            "Render test for RichRadioButton covering various states and orientations, with"
                    + " improved layout and ellipsized text in the vertical layout";

    private static final String sVeryLongTitle =
            "This is an extremely long title, which cannot possibly fit in one line.";
    private static final String sVeryLongDescription =
            "And this is a very long description, which cannot possibly fit in one line.";

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
    private RichRadioButton mRichRbVerticalFullUnchecked;

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
                    mRichRbVerticalFullUnchecked =
                            mLayout.findViewById(R.id.rich_rb_vertical_full_unchecked);
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
    @DisabledTest(message = "https://crbug.com/454385607")
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
    @DisabledTest(message = "https://crbug.com/454443245")
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

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButton"})
    public void testRichRbVerticalFullUnchecked() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRbVerticalFullUnchecked.setItemData(
                            R.drawable.test_location_precise,
                            sVeryLongTitle,
                            sVeryLongDescription,
                            true);
                    mRichRbVerticalFullUnchecked.setChecked(false);
                });
        mRenderTestRule.render(mRichRbVerticalFullUnchecked, "rich_rb_vertical_full_unchecked");
    }

    @Test
    @SmallTest
    @Feature({"RichRadioButton"})
    public void testLayoutParamsPreservedAfterMultipleSetItemDataCalls() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Get initial layout params for title and radio button.
                    ViewGroup.MarginLayoutParams initialTitleParams =
                            (ViewGroup.MarginLayoutParams)
                                    mRichRbHorizontalFullUnchecked
                                            .findViewById(R.id.rich_radio_button_title)
                                            .getLayoutParams();
                    ViewGroup.MarginLayoutParams initialRadioButtonParams =
                            (ViewGroup.MarginLayoutParams)
                                    mRichRbHorizontalFullUnchecked
                                            .findViewById(R.id.rich_radio_button_radio_button)
                                            .getLayoutParams();

                    // Get initial padding for the root layout.
                    int initialRootPaddingStart = mRichRbHorizontalFullUnchecked.getPaddingStart();
                    int initialRootPaddingTop = mRichRbHorizontalFullUnchecked.getPaddingTop();
                    int initialRootPaddingEnd = mRichRbHorizontalFullUnchecked.getPaddingEnd();
                    int initialRootPaddingBottom =
                            mRichRbHorizontalFullUnchecked.getPaddingBottom();

                    // Set with icon, then without, then with again.
                    mRichRbHorizontalFullUnchecked.setItemData(
                            R.drawable.test_location_precise, "Title A", "Description A", false);

                    mRichRbHorizontalFullUnchecked.setItemData(
                            0, // No icon
                            "Title B",
                            "Description B",
                            false);

                    mRichRbHorizontalFullUnchecked.setItemData(
                            R.drawable.test_location_precise, "Title C", "Description C", false);

                    // Assert that the layout params for the root layout , title and radio button
                    // are the same as their initial values.
                    ViewGroup.MarginLayoutParams finalTitleParams =
                            (ViewGroup.MarginLayoutParams)
                                    mRichRbHorizontalFullUnchecked
                                            .findViewById(R.id.rich_radio_button_title)
                                            .getLayoutParams();
                    ViewGroup.MarginLayoutParams finalRadioButtonParams =
                            (ViewGroup.MarginLayoutParams)
                                    mRichRbHorizontalFullUnchecked
                                            .findViewById(R.id.rich_radio_button_radio_button)
                                            .getLayoutParams();

                    Assert.assertNotSame(
                            "Initial and final title LayoutParams should NOT be the same instance",
                            initialTitleParams,
                            finalTitleParams);

                    Assert.assertNotSame(
                            "Initial and final radio button LayoutParams should NOT be the same"
                                    + " instance",
                            initialRadioButtonParams,
                            finalRadioButtonParams);

                    // Check title margins.
                    Assert.assertEquals(
                            "Title left margin changed unexpectedly",
                            initialTitleParams.leftMargin,
                            finalTitleParams.leftMargin);
                    Assert.assertEquals(
                            "Title top margin changed unexpectedly",
                            initialTitleParams.topMargin,
                            finalTitleParams.topMargin);
                    Assert.assertEquals(
                            "Title right margin changed unexpectedly",
                            initialTitleParams.rightMargin,
                            finalTitleParams.rightMargin);
                    Assert.assertEquals(
                            "Title bottom margin changed unexpectedly",
                            initialTitleParams.bottomMargin,
                            finalTitleParams.bottomMargin);
                    Assert.assertEquals(
                            "Title start margin changed unexpectedly",
                            initialTitleParams.getMarginStart(),
                            finalTitleParams.getMarginStart());
                    Assert.assertEquals(
                            "Title end margin changed unexpectedly",
                            initialTitleParams.getMarginEnd(),
                            finalTitleParams.getMarginEnd());

                    // Check radio button margins.
                    Assert.assertEquals(
                            "Radio button left margin changed unexpectedly",
                            initialRadioButtonParams.leftMargin,
                            finalRadioButtonParams.leftMargin);
                    Assert.assertEquals(
                            "Radio button top margin changed unexpectedly",
                            initialRadioButtonParams.topMargin,
                            finalRadioButtonParams.topMargin);
                    Assert.assertEquals(
                            "Radio button right margin changed unexpectedly",
                            initialRadioButtonParams.rightMargin,
                            finalRadioButtonParams.rightMargin);
                    Assert.assertEquals(
                            "Radio button bottom margin changed unexpectedly",
                            initialRadioButtonParams.bottomMargin,
                            finalRadioButtonParams.bottomMargin);
                    Assert.assertEquals(
                            "Radio button start margin changed unexpectedly",
                            initialRadioButtonParams.getMarginStart(),
                            finalRadioButtonParams.getMarginStart());

                    Assert.assertEquals(
                            "Radio button end margin changed unexpectedly",
                            initialRadioButtonParams.getMarginEnd(),
                            finalRadioButtonParams.getMarginEnd());

                    // Check root layout padding.
                    Assert.assertEquals(
                            "Root padding start changed unexpectedly",
                            initialRootPaddingStart,
                            mRichRbHorizontalFullUnchecked.getPaddingStart());
                    Assert.assertEquals(
                            "Root padding top changed unexpectedly",
                            initialRootPaddingTop,
                            mRichRbHorizontalFullUnchecked.getPaddingTop());
                    Assert.assertEquals(
                            "Root padding end changed unexpectedly",
                            initialRootPaddingEnd,
                            mRichRbHorizontalFullUnchecked.getPaddingEnd());
                    Assert.assertEquals(
                            "Root padding bottom changed unexpectedly",
                            initialRootPaddingBottom,
                            mRichRbHorizontalFullUnchecked.getPaddingBottom());
                });
    }
}
