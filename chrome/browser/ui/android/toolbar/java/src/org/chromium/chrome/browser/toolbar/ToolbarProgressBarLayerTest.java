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
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link ToolbarProgressBarLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
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

    private Activity mActivity;
    private View mProgressBarContainer;
    private View mToolbarHairline;

    private ToolbarProgressBarLayer mLayer;
    private @ControlsPosition int mTestControlPosition = ControlsPosition.NONE;
    private ObservableSupplierImpl<Integer> mBookmarkBarIdSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mProgressBarContainer = spy(new View(mActivity));
        doReturn(mContentView).when(mProgressBarContainer).getParent();
        mToolbarHairline = new View(mActivity);

        mBookmarkBarIdSupplier = new ObservableSupplierImpl<>(0);

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
                        false);
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
}
