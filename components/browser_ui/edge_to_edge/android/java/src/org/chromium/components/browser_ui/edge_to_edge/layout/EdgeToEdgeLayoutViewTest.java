// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge.layout;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.graphics.Color;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.edge_to_edge.R;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;

/** Java unit test for {@link EdgeToEdgeBaseLayout} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(PER_CLASS) // Tests changes the content view of the activity.
public class EdgeToEdgeLayoutViewTest extends BlankUiTestActivityTestCase {

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule.Builder()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_EDGE_TO_EDGE)
                    .setRevision(0)
                    .build();

    private static final int STATUS_BAR_SIZE = 100;
    private static final int NAV_BAR_SIZE = 150;
    private static final int STATUS_BAR_COLOR = Color.RED;
    private static final int NAV_BAR_COLOR = Color.GREEN;
    private static final int NAV_BAR_DIVIDER_COLOR = Color.BLUE;
    private static final int BG_COLOR = Color.GRAY;

    private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private FrameLayout mContentView;
    private View mEdgeToEdgeLayout;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator =
                            new EdgeToEdgeLayoutCoordinator(getActivity(), null);

                    mContentView = new FrameLayout(getActivity(), null);
                    getActivity()
                            .setContentView(
                                    mEdgeToEdgeLayoutCoordinator.wrapContentView(mContentView));

                    mEdgeToEdgeLayout = getActivity().findViewById(R.id.edge_to_edge_base_layout);

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
}
