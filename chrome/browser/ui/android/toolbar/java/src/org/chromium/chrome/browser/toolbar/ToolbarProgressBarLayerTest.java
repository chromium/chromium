// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link ToolbarProgressBarLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarProgressBarLayerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private BottomControlsStacker mBottomControlsStacker;

    private Activity mActivity;
    private ToolbarControlContainer mControlContainer;
    private ToolbarProgressBar mProgressBarView;
    private View mProgressBarContainer;
    private View mToolbarHairline;

    private ToolbarProgressBarLayer mLayer;
    private @ControlsPosition int mTestControlPosition = ControlsPosition.BOTTOM;
    private SettableMonotonicObservableSupplier<Integer> mTopAnchorViewIdSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mControlContainer = new ToolbarControlContainer(mActivity, /* attrs= */ null);
        mProgressBarView = new ToolbarProgressBar(mActivity, /* attrs= */ null);
        mProgressBarContainer = new View(mActivity);
        CoordinatorLayout contentView = new CoordinatorLayout(mActivity);
        contentView.addView(mProgressBarContainer);
        mToolbarHairline = new View(mActivity);

        mTopAnchorViewIdSupplier = ObservableSuppliers.createMonotonic(Resources.ID_NULL);

        mLayer =
                new ToolbarProgressBarLayer(
                        mControlContainer,
                        mProgressBarContainer,
                        mProgressBarView,
                        mToolbarHairline,
                        () -> mTestControlPosition,
                        mTopAnchorViewIdSupplier,
                        mTopControlsStacker,
                        mBottomControlsStacker,
                        /* isToolbarPositionCustomizationEnabled= */ false,
                        // Only used for its Context in a path not exercised by these tests.
                        /* toolbarLayout= */ null);
    }

    @Test
    public void testTopControlVisibility() {
        mProgressBarView.start();
        mTestControlPosition = ControlsPosition.TOP;
        assertEquals(TopControlVisibility.VISIBLE, mLayer.getTopControlVisibility());

        mTestControlPosition = ControlsPosition.BOTTOM;
        assertEquals(TopControlVisibility.HIDDEN, mLayer.getTopControlVisibility());

        mProgressBarView.finish(/* fadeOut= */ false);
        mTestControlPosition = ControlsPosition.TOP;
        assertEquals(TopControlVisibility.HIDDEN, mLayer.getTopControlVisibility());
    }

    @Test
    public void testUpdateTopAnchorView() {
        mTestControlPosition = ControlsPosition.TOP;
        mControlContainer.setId(123);
        mProgressBarContainer.setLayoutParams(
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // The progress bar anchors to whatever the supplier resolves (the anchor priority ladder
        // now lives in ToolbarManager).
        mTopAnchorViewIdSupplier.set(mControlContainer.getId());
        mLayer.onTopControlLayerHeightChanged(0, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(
                mControlContainer.getId(),
                ((CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams())
                        .getAnchorId());

        // Bookmark bar Id resolved by the supplier.
        mTopAnchorViewIdSupplier.set(456);
        ShadowLooper.idleMainLooper();
        mLayer.onTopControlLayerHeightChanged(0, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(
                456,
                ((CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams())
                        .getAnchorId());

        // Tab sharing toolbar Id resolved by the supplier.
        mTopAnchorViewIdSupplier.set(R.id.tab_sharing_toolbar_container);
        ShadowLooper.idleMainLooper();
        mLayer.onTopControlLayerHeightChanged(0, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(
                R.id.tab_sharing_toolbar_container,
                ((CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams())
                        .getAnchorId());
    }

    @Test
    public void testUpdateTopAnchorView_customizationEnabled() {
        ToolbarProgressBarLayer layer =
                new ToolbarProgressBarLayer(
                        mControlContainer,
                        mProgressBarContainer,
                        mProgressBarView,
                        mToolbarHairline,
                        () -> mTestControlPosition,
                        mTopAnchorViewIdSupplier,
                        mTopControlsStacker,
                        mBottomControlsStacker,
                        /* isToolbarPositionCustomizationEnabled= */ true,
                        // Only used for its Context in a path not exercised by these tests.
                        /* toolbarLayout= */ null);

        mTestControlPosition = ControlsPosition.TOP;
        mControlContainer.setId(123);
        CoordinatorLayout.LayoutParams params =
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        params.setAnchorId(mControlContainer.getId());
        mProgressBarContainer.setLayoutParams(params);

        // When customization is enabled, updateTopAnchorView() bails out early, so the supplier's
        // value is never applied.
        mTopAnchorViewIdSupplier.set(456);
        ShadowLooper.idleMainLooper();
        layer.onTopControlLayerHeightChanged(0, 0);
        assertEquals(
                mControlContainer.getId(),
                ((CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams())
                        .getAnchorId());
    }

    @Test
    public void testOnProgressBarInfoUpdate_withXOffset() {
        org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo drawingInfo =
                new org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo();
        drawingInfo.progressBarRect.set(0, 0, 100, 10);
        drawingInfo.progressBarBackgroundRect.set(100, 0, 500, 10);
        drawingInfo.progressBarStaticBackgroundRect.set(0, 0, 500, 10);

        ViewGroup.MarginLayoutParams params =
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = 240;
        mProgressBarContainer.setLayoutParams(params);

        mLayer.onProgressBarInfoUpdate(drawingInfo);

        assertEquals(0, drawingInfo.progressBarRect.left);
        assertEquals(100, drawingInfo.progressBarRect.right);
        assertEquals(100, drawingInfo.progressBarBackgroundRect.left);
        assertEquals(500, drawingInfo.progressBarBackgroundRect.right);
        assertEquals(0, drawingInfo.progressBarStaticBackgroundRect.left);
        assertEquals(500, drawingInfo.progressBarStaticBackgroundRect.right);
    }
}
