// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static androidx.core.view.WindowInsetsCompat.Type.navigationBars;
import static androidx.core.view.WindowInsetsCompat.Type.statusBars;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowBuild;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.ui.insets.InsetObserver;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowBuild.class)
public class SimpleEdgeToEdgeControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TOP_STATUS_INSET = 100;
    private static final int BOTTOM_NAV_INSET = 100;
    private static final WindowInsetsCompat WINDOW_INSETS_WITH_NAVBAR =
            new WindowInsetsCompat.Builder()
                    .setInsets(statusBars(), Insets.of(0, TOP_STATUS_INSET, 0, 0))
                    .setInsets(navigationBars(), Insets.of(0, 0, 0, BOTTOM_NAV_INSET))
                    .build();
    private static final WindowInsetsCompat WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(statusBars(), Insets.of(0, TOP_STATUS_INSET, 0, 0))
                    .setInsets(navigationBars(), Insets.of(0, 0, 0, 0))
                    .build();

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private View mView;
    @Mock private InsetObserver mInsetObserver;
    private SimpleEdgeToEdgeController mSimpleEdgeToEdgeController;

    @Before
    public void setup() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(mDisplayMetrics).when(mResources).getDisplayMetrics();

        mSimpleEdgeToEdgeController = new SimpleEdgeToEdgeController(mContext, mInsetObserver);
        verify(mInsetObserver, times(1)).addInsetsConsumer(any(), eq(EDGE_TO_EDGE_CONTROLLER_IMPL));
        verify(mInsetObserver, times(1)).retriggerOnApplyWindowInsets();
    }

    @Test
    public void testSimpleEdgeToEdgeController_withNavBar() {
        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        assertEquals(
                "The controller should return the correct bottom inset.",
                BOTTOM_NAV_INSET,
                mSimpleEdgeToEdgeController.getBottomInsetPx());
        assertTrue(
                "The controller should be drawing edge-to-edge.",
                mSimpleEdgeToEdgeController.isDrawingToEdge());
        assertTrue(
                "The contorller should be opted into edge-to-edge.",
                mSimpleEdgeToEdgeController.isPageOptedIntoEdgeToEdge());

        // Verify that the inset is still correct after the same insets are seen again.
        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        assertEquals(
                "The controller should return the correct bottom inset.",
                BOTTOM_NAV_INSET,
                mSimpleEdgeToEdgeController.getBottomInsetPx());
    }

    @Test
    public void testSimpleEdgeToEdgeController_noNavBar() {
        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS);
        assertEquals(
                "The controller should indicate a bottom inset of 0.",
                0,
                mSimpleEdgeToEdgeController.getBottomInsetPx());
        assertFalse(
                "The controller should not be drawing edge-to-edge.",
                mSimpleEdgeToEdgeController.isDrawingToEdge());
        assertFalse(
                "The controller should not be opted into edge-to-edge.",
                mSimpleEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    @Test
    public void testSimpleEdgeToEdgeController_notifiesObservers() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mSimpleEdgeToEdgeController.registerObserver(changeObserver);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        changeObserver.checkState(
                BOTTOM_NAV_INSET, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS);
        changeObserver.checkState(0, /* isDrawingToEdge= */ false, /* isPageOptInToEdge= */ false);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        changeObserver.checkState(
                BOTTOM_NAV_INSET, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);
    }

    @Test
    public void testSimpleEdgeToEdgeController_notifiesAdjusters() {
        TestPadAdjuster padAdjuster = new TestPadAdjuster();
        mSimpleEdgeToEdgeController.registerAdjuster(padAdjuster);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        padAdjuster.checkInsets(BOTTOM_NAV_INSET);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS);
        padAdjuster.checkInsets(0);

        mSimpleEdgeToEdgeController.onApplyWindowInsets(mView, WINDOW_INSETS_WITH_NAVBAR);
        padAdjuster.checkInsets(BOTTOM_NAV_INSET);
    }

    private static class TestChangeObserver implements EdgeToEdgeSupplier.ChangeObserver {
        private int mInset;
        private boolean mIsDrawingToEdge;
        private boolean mIsPageOptInToEdge;

        TestChangeObserver() {}

        @Override
        public void onToEdgeChange(
                int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
            mInset = bottomInset;
            mIsDrawingToEdge = isDrawingToEdge;
            mIsPageOptInToEdge = isPageOptInToEdge;
        }

        void checkState(int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
            assertEquals(
                    "The change observer does not have the expected inset.", bottomInset, mInset);
            assertEquals(
                    "The change observer does not match the expected drawing toEdge state.",
                    isDrawingToEdge,
                    mIsDrawingToEdge);
            assertEquals(
                    "The change observer does not match the expected edge-to-edge opt-in state.",
                    isPageOptInToEdge,
                    mIsPageOptInToEdge);
        }
    }

    private static class TestPadAdjuster implements EdgeToEdgePadAdjuster {
        private int mInset;

        TestPadAdjuster() {}

        @Override
        public void overrideBottomInset(int inset) {
            mInset = inset;
        }

        @Override
        public void destroy() {}

        void checkInsets(int expected) {
            assertEquals("The pad adjuster does not have the expected inset.", expected, mInset);
        }
    }
}
