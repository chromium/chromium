// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;

/** Unit tests for {@link SidePanelResizeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
// Use a density of 2, so that 1dp is 2px.
@Config(qualifiers = "xhdpi")
public class SidePanelResizeManagerUnitTest {
    private static final int HEIGHT = 1000;
    private static final int INITIAL_WIDTH = 400;
    private static final int MIN_WIDTH_DP = 200;

    private FrameLayout mParent;
    private View mContent;
    private View mContentViewToPin;
    private SidePanelResizeManager mResizeManager;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mParent = new FrameLayout(activity);
        mContent = new View(activity);
        mParent.addView(
                mContent,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        layoutParent(INITIAL_WIDTH);
        mResizeManager = new SidePanelResizeManager(activity, () -> mContentViewToPin);
    }

    @Test
    public void getUserResizedWidthPx_nullWithoutResize() {
        assertNull(mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void updateLiveResizeState_overridesCommittedWidthWithRawWidth() {
        commit(/* widthPx= */ 600);

        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ -50, AnchorSide.RIGHT);
        assertEquals(-50, (int) mResizeManager.getUserResizedWidthPx());

        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ 5000, AnchorSide.RIGHT);
        assertEquals(5000, (int) mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void commitFinalResizeState_clampsCommittedWidth() {
        commit(/* widthPx= */ 600);
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());

        // Below the minimum.
        commit(/* widthPx= */ 100);
        assertEquals(MIN_WIDTH_DP * 2, (int) mResizeManager.getUserResizedWidthPx());

        // Above the maximum.
        commit(/* widthPx= */ 5000);
        assertEquals(
                MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP * 2,
                (int) mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void commitFinalResizeState_noOpWithoutLiveFrame() {
        // E.g. a tap without a move.
        mResizeManager.commitFinalResizeState(/* finalWidthPx= */ 600, MIN_WIDTH_DP);
        assertNull(mResizeManager.getUserResizedWidthPx());

        commit(/* widthPx= */ 600);
        mResizeManager.commitFinalResizeState(/* finalWidthPx= */ 800, MIN_WIDTH_DP);
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void clearLiveResizeState_keepsCommittedWidth() {
        commit(/* widthPx= */ 600);
        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ 800, AnchorSide.RIGHT);

        mResizeManager.clearLiveResizeState();
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());

        // A commit after the drag ended has nothing to commit.
        mResizeManager.commitFinalResizeState(/* finalWidthPx= */ 800, MIN_WIDTH_DP);
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void updateLiveResizeState_pinsContentAlignedToLeftEdgeForRightAnchor() {
        mContentViewToPin = mContent;
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH, AnchorSide.RIGHT);

        // Growing the parent leaves a gap past the right edge.
        layoutParent(INITIAL_WIDTH + 100);
        assertEquals(INITIAL_WIDTH, mContent.getWidth());
        assertEquals(0, mContent.getLeft());

        // Shrinking the parent clips the content past the right edge.
        layoutParent(INITIAL_WIDTH - 100);
        assertEquals(INITIAL_WIDTH, mContent.getWidth());
        assertEquals(0, mContent.getLeft());
    }

    @Test
    public void updateLiveResizeState_pinsContentAlignedToRightEdgeForLeftAnchor() {
        mContentViewToPin = mContent;
        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ INITIAL_WIDTH, AnchorSide.LEFT);

        layoutParent(INITIAL_WIDTH + 100);
        assertEquals(INITIAL_WIDTH, mContent.getWidth());
        assertEquals(100, mContent.getLeft());

        layoutParent(INITIAL_WIDTH - 100);
        assertEquals(INITIAL_WIDTH, mContent.getWidth());
        assertEquals(-100, mContent.getLeft());
    }

    @Test
    public void updateLiveResizeState_pinsContentOnlyOnFirstFrame() {
        mContentViewToPin = mContent;
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH + 100, AnchorSide.RIGHT);
        layoutParent(INITIAL_WIDTH + 100);

        // Later frames keep the pre-drag width.
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH + 200, AnchorSide.RIGHT);
        layoutParent(INITIAL_WIDTH + 200);
        assertEquals(INITIAL_WIDTH, mContent.getWidth());
    }

    @Test
    public void updateLiveResizeState_doesNotPinWithoutContentToPin() {
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH, AnchorSide.RIGHT);

        layoutParent(INITIAL_WIDTH + 100);
        assertEquals(INITIAL_WIDTH + 100, mContent.getWidth());
    }

    @Test
    public void commitFinalResizeState_unpinsContent() {
        mContentViewToPin = mContent;
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH, AnchorSide.RIGHT);

        mResizeManager.commitFinalResizeState(
                /* finalWidthPx= */ INITIAL_WIDTH + 100, MIN_WIDTH_DP);
        assertContentFollowsParent();
    }

    @Test
    public void clearLiveResizeState_unpinsContent() {
        mContentViewToPin = mContent;
        mResizeManager.updateLiveResizeState(
                /* proposedWidthPx= */ INITIAL_WIDTH, AnchorSide.RIGHT);

        mResizeManager.clearLiveResizeState();
        assertContentFollowsParent();
    }

    /** Commits a drag that ends at {@code widthPx}. */
    private void commit(int widthPx) {
        mResizeManager.updateLiveResizeState(widthPx, AnchorSide.RIGHT);
        mResizeManager.commitFinalResizeState(widthPx, MIN_WIDTH_DP);
    }

    private void assertContentFollowsParent() {
        var layoutParams = (FrameLayout.LayoutParams) mContent.getLayoutParams();
        assertEquals(FrameLayout.LayoutParams.MATCH_PARENT, layoutParams.width);
        assertEquals(FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY, layoutParams.gravity);

        layoutParent(INITIAL_WIDTH + 100);
        assertEquals(INITIAL_WIDTH + 100, mContent.getWidth());
        assertEquals(0, mContent.getLeft());
    }

    private void layoutParent(int width) {
        mParent.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(HEIGHT, MeasureSpec.EXACTLY));
        mParent.layout(0, 0, width, HEIGHT);
    }
}
