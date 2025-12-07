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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.ArrayList;
import java.util.List;

/** Render test for {@link RichRadioButtonList}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class RichRadioButtonListRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private static final int REVISION = 1;
    private static final String REVISION_DESCRIPTION =
            "Initial render test for RichRadioButtonList.";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(REVISION)
                    .setDescription(REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .build();

    private LinearLayout mRootLayout;
    private RichRadioButtonList mRichRadioButtonListView;

    private final int mFakeBgColor;

    public RichRadioButtonListRenderTest(boolean nightModeEnabled) {
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
                                    .inflate(
                                            R.layout.rich_radio_button_list_render_test,
                                            null,
                                            false);
                    activity.setContentView(content);

                    mRootLayout = content.findViewById(R.id.test_rich_radio_button_list_layout);
                    mRootLayout.setBackgroundColor(mFakeBgColor);

                    int displayWidth = activity.getResources().getDisplayMetrics().widthPixels;
                    mRootLayout.measure(
                            View.MeasureSpec.makeMeasureSpec(
                                    displayWidth, View.MeasureSpec.EXACTLY),
                            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
                    mRootLayout.layout(
                            0, 0, mRootLayout.getMeasuredWidth(), mRootLayout.getMeasuredHeight());

                    mRichRadioButtonListView =
                            mRootLayout.findViewById(R.id.rich_radio_button_list_view);
                });

        Assert.assertNotNull(mRootLayout);
        Assert.assertNotNull(mRichRadioButtonListView);
    }

    private List<RichRadioButtonData> createTestData(
            int count, boolean includeDescription, boolean includeIcon) {
        List<RichRadioButtonData> data = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            RichRadioButtonData.Builder builder =
                    new RichRadioButtonData.Builder("item_" + i, "Option " + (i + 1));

            if (includeIcon) {
                builder.setIconResId(
                        i % 2 == 0
                                ? R.drawable.test_ic_vintage_filter
                                : R.drawable.test_location_precise);
            } else {
                builder.setIconResId(0);
            }

            if (includeDescription) {
                builder.setDescription("Description for item " + (i + 1) + ".");
            } else {
                builder.setDescription(null);
            }

            data.add(builder.build());
        }
        return data;
    }

    /**
     * Waits for the view to be rendered.
     *
     * @param view The view to wait for.
     */
    private void waitForViewToBeRendered(View view) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return view.isShown() && view.getWidth() > 0 && view.getHeight() > 0;
                },
                "View not rendered: " + view.getClass().getSimpleName());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButtonList"})
    public void testVerticalSingleColumnLayout() throws Exception {
        List<RichRadioButtonData> testData = createTestData(3, true, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonListView.initialize(
                            testData,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            (selectedId) -> {});
                    mRichRadioButtonListView.setSelectedItem("item_0");
                });
        waitForViewToBeRendered(mRichRadioButtonListView);
        mRenderTestRule.render(mRichRadioButtonListView, "rich_rb_list_vertical_single_column");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButtonList"})
    public void testTwoColumnGridLayoutEven() throws Exception {
        List<RichRadioButtonData> testData = createTestData(4, true, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonListView.initialize(
                            testData,
                            RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID,
                            (selectedId) -> {});
                    mRichRadioButtonListView.setSelectedItem("item_1");
                });
        waitForViewToBeRendered(mRichRadioButtonListView);
        mRenderTestRule.render(mRichRadioButtonListView, "rich_rb_list_two_column_grid_even");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButtonList"})
    public void testTwoColumnGridLayoutOdd() throws Exception {
        List<RichRadioButtonData> testData = createTestData(3, false, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonListView.initialize(
                            testData,
                            RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID,
                            (selectedId) -> {});
                    mRichRadioButtonListView.setSelectedItem("item_2");
                });
        waitForViewToBeRendered(mRichRadioButtonListView);
        mRenderTestRule.render(mRichRadioButtonListView, "rich_rb_list_two_column_grid_odd");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButtonList"})
    public void testVerticalSingleColumnMinimal() throws Exception {
        List<RichRadioButtonData> testData = createTestData(2, false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonListView.initialize(
                            testData,
                            RichRadioButtonList.LayoutMode.VERTICAL_SINGLE_COLUMN,
                            (selectedId) -> {});
                    mRichRadioButtonListView.setSelectedItem("item_0");
                });
        waitForViewToBeRendered(mRichRadioButtonListView);
        mRenderTestRule.render(
                mRichRadioButtonListView, "rich_rb_list_vertical_single_column_minimal");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RichRadioButtonList"})
    public void testTwoColumnGridMinimal() throws Exception {
        List<RichRadioButtonData> testData = createTestData(2, false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRichRadioButtonListView.initialize(
                            testData,
                            RichRadioButtonList.LayoutMode.TWO_COLUMN_GRID,
                            (selectedId) -> {});
                    mRichRadioButtonListView.setSelectedItem("item_1");
                });
        waitForViewToBeRendered(mRichRadioButtonListView);
        mRenderTestRule.render(mRichRadioButtonListView, "rich_rb_list_two_column_grid_minimal");
    }
}
