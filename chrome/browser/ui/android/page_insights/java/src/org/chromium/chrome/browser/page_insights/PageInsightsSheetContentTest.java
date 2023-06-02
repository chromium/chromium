// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static org.junit.Assert.assertEquals;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * This class tests the functionality of the {@link PageInsightsSheetContent}
 * without running the coordinator/mediator.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PageInsightsSheetContentTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private BottomSheetController mBottomSheetController;
    private ScrimCoordinator mScrimCoordinator;
    private BottomSheetContent mSheetContent;
    private BottomSheetTestSupport mTestSupport;

    @BeforeClass
    public static void setUpSuite() {
        sTestRule.launchActivity(null);
        BottomSheetTestSupport.setSmallScreen(false);
    }

    @Before
    public void setUp() throws Exception {
        ViewGroup rootView = sTestRule.getActivity().findViewById(android.R.id.content);
        TestThreadUtils.runOnUiThreadBlocking(() -> rootView.removeAllViews());

        mScrimCoordinator = new ScrimCoordinator(
                sTestRule.getActivity(), new ScrimCoordinator.SystemUiScrimDelegate() {
                    @Override
                    public void setStatusBarScrimFraction(float scrimFraction) {}

                    @Override
                    public void setNavigationBarScrimFraction(float scrimFraction) {}
                }, rootView, Color.WHITE);

        mBottomSheetController = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Supplier<ScrimCoordinator> scrimSupplier = () -> mScrimCoordinator;
            Callback<View> initializedCallback = (v) -> {};
            return BottomSheetControllerFactory.createFullWidthBottomSheetController(scrimSupplier,
                    initializedCallback, sTestRule.getActivity().getWindow(),
                    KeyboardVisibilityDelegate.getInstance(), () -> rootView);
        });

        mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent = new PageInsightsSheetContent(sTestRule.getActivity());
            mBottomSheetController.requestShowContent(mSheetContent, false);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mScrimCoordinator.destroy();
            mBottomSheetController = null;
        });
    }

    private void waitForAnimationToFinish() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    @Test
    @MediumTest
    public void testAvailableStates() throws Exception {
        // Starts with peeking state.
        assertEquals(BottomSheetController.SheetState.PEEK, mBottomSheetController.getSheetState());

        // Half state is disabled. Expanding sheets leads to full state.
        TestThreadUtils.runOnUiThreadBlocking(mBottomSheetController::expandSheet);
        waitForAnimationToFinish();
        assertEquals(BottomSheetController.SheetState.FULL, mBottomSheetController.getSheetState());

        // Collapsing from full state leads to peeking state.
        TestThreadUtils.runOnUiThreadBlocking(() -> mBottomSheetController.collapseSheet(false));
        assertEquals(BottomSheetController.SheetState.PEEK, mBottomSheetController.getSheetState());
    }
}
