// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge.layout;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.edge_to_edge.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.WindowInsetsTestUtils.SpyWindowInsetsBuilder;

import java.io.IOException;

/** Java unit test for {@link EdgeToEdgeBaseLayout} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(PER_CLASS) // Tests changes the content view of the activity.
public class EdgeToEdgeLayoutViewTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule.Builder()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_EDGE_TO_EDGE)
                    .setRevision(0)
                    .build();

    private static final int STATUS_BAR_SIZE = 100;
    private static final int NAV_BAR_SIZE = 150;
    private static final int DISPLAY_CUTOUT_SIZE = 75;
    private static final int IME_SIZE = 300;

    private static final int STATUS_BAR_COLOR = Color.RED;
    private static final int NAV_BAR_COLOR = Color.GREEN;
    private static final int NAV_BAR_DIVIDER_COLOR = Color.BLUE;
    private static final int BG_COLOR = Color.GRAY;

    private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private FrameLayout mContentView;
    private View mEdgeToEdgeLayout;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator = new EdgeToEdgeLayoutCoordinator(sActivity, null);

                    mContentView = new FrameLayout(sActivity, null);
                    sActivity.setContentView(
                            mEdgeToEdgeLayoutCoordinator.wrapContentView(mContentView));

                    mEdgeToEdgeLayout = sActivity.findViewById(R.id.edge_to_edge_base_layout);

                    // Set colors to be used in render tests.
                    mContentView.setBackgroundColor(BG_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setStatusBarColor(STATUS_BAR_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(NAV_BAR_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(
                            NAV_BAR_DIVIDER_COLOR);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderBottomNavBar() throws IOException {
        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mContentView, topBottomInsets);
                });
        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "bottom_nav_bar");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderLeftNavBar() throws IOException {
        WindowInsetsCompat topLeftInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mContentView, topLeftInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "left_nav_Bar");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderDisplayCutoutOverlapSystemBars() throws IOException {
        WindowInsetsCompat topBottomSysBarsWithLeftCutoutInsets =
                new SpyWindowInsetsBuilder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                WindowInsetsCompat.Type.displayCutout(),
                                Insets.of(DISPLAY_CUTOUT_SIZE, 0, 0, 0))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topBottomSysBarsWithLeftCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "left_display_cutout_overlap");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderDisplayCutoutOverlapStatusBarOnly() throws IOException {
        WindowInsetsCompat topLeftSysBarsRightCutoutInsets =
                new SpyWindowInsetsBuilder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.displayCutout(),
                                Insets.of(0, 0, DISPLAY_CUTOUT_SIZE, 0))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topLeftSysBarsRightCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "top_left_sys_bars_right_cutout");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderImeInsets() throws IOException {
        WindowInsetsCompat topLeftSysBarsRightCutoutInsets =
                new SpyWindowInsetsBuilder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, IME_SIZE))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topLeftSysBarsRightCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "top_bottom_ime");
    }
}
