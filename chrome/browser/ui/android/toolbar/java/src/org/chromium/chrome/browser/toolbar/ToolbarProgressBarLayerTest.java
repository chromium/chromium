// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link ToolbarProgressBarLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarProgressBarLayerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ToolbarControlContainer mControlContainer;
    @Mock private ToolbarProgressBar mProgressBarView;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private CoordinatorLayout mContentView;
    @Mock private ToolbarLayout mToolbarLayout;

    private Activity mActivity;
    private View mProgressBarContainer;
    private View mToolbarHairline;

    private ToolbarProgressBarLayer mLayer;
    private @ControlsPosition int mTestControlPosition = ControlsPosition.BOTTOM;
    private SettableMonotonicObservableSupplier<Integer> mBookmarkBarIdSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mProgressBarContainer = spy(new View(mActivity));
        doReturn(mContentView).when(mProgressBarContainer).getParent();
        mToolbarHairline = new View(mActivity);

        mBookmarkBarIdSupplier = ObservableSuppliers.createMonotonic(0);

        mLayer =
                new ToolbarProgressBarLayer(
                        mControlContainer,
                        mProgressBarContainer,
                        mProgressBarView,
                        mToolbarHairline,
                        () -> mTestControlPosition,
                        mBookmarkBarIdSupplier,
                        mTopControlsStacker,
                        mBottomControlsStacker,
                        false,
                        mToolbarLayout);
    }

    @Test
    public void testTopControlVisibility() {
        when(mProgressBarView.isStarted()).thenReturn(true);
        mTestControlPosition = ControlsPosition.TOP;
        assertEquals(TopControlVisibility.VISIBLE, mLayer.getTopControlVisibility());

        mTestControlPosition = ControlsPosition.BOTTOM;
        assertEquals(TopControlVisibility.HIDDEN, mLayer.getTopControlVisibility());

        when(mProgressBarView.isStarted()).thenReturn(false);
        mTestControlPosition = ControlsPosition.TOP;
        assertEquals(TopControlVisibility.HIDDEN, mLayer.getTopControlVisibility());
    }

    @Test
    public void testUpdateTopAnchorView() {
        mTestControlPosition = ControlsPosition.TOP;
        View controlContainerView = new View(mActivity);
        controlContainerView.setId(123);
        when(mControlContainer.getView()).thenReturn(controlContainerView);
        mProgressBarContainer.setLayoutParams(
                new CoordinatorLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Bookmark bar is not visible.
        mBookmarkBarIdSupplier.set(0);
        mLayer.onTopControlLayerHeightChanged(0, 0);
        ShadowLooper.idleMainLooper();
        assertEquals(
                controlContainerView.getId(),
                ((CoordinatorLayout.LayoutParams) mProgressBarContainer.getLayoutParams())
                        .getAnchorId());

        // Bookmark bar is visible.
        mBookmarkBarIdSupplier.set(456);
        ShadowLooper.idleMainLooper();
        when(mTopControlsStacker.isLayerAtBottom(TopControlsStacker.TopControlType.BOOKMARK_BAR))
                .thenReturn(true);
        mLayer.onTopControlLayerHeightChanged(0, 0);
        assertEquals(
                456,
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

        View controlContainerView = new View(mActivity);
        when(mControlContainer.getView()).thenReturn(controlContainerView);

        mLayer.onProgressBarInfoUpdate(drawingInfo);

        assertEquals(240, drawingInfo.progressBarRect.left);
        assertEquals(340, drawingInfo.progressBarRect.right);
        assertEquals(340, drawingInfo.progressBarBackgroundRect.left);
        assertEquals(740, drawingInfo.progressBarBackgroundRect.right);
        assertEquals(240, drawingInfo.progressBarStaticBackgroundRect.left);
        assertEquals(740, drawingInfo.progressBarStaticBackgroundRect.right);
    }
}
