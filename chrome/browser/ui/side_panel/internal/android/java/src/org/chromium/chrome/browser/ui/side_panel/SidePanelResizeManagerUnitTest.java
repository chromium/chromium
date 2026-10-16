// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.MAX_USER_RESIZED_SIDE_PANEL_WIDTH_DP;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SidePanelResizeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
// Use a density of 2, so that 1dp is 2px.
@Config(qualifiers = "xhdpi")
public class SidePanelResizeManagerUnitTest {
    private static final int MIN_WIDTH_DP = 200;

    private SidePanelResizeManager mResizeManager;

    @Before
    public void setUp() {
        mResizeManager = new SidePanelResizeManager(RuntimeEnvironment.getApplication());
    }

    @Test
    public void getUserResizedWidthPx_nullWithoutResize() {
        assertNull(mResizeManager.getUserResizedWidthPx());
    }

    @Test
    public void updateLiveResizeState_overridesCommittedWidthWithRawWidth() {
        commit(/* widthPx= */ 600);

        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ -50);
        assertEquals(-50, (int) mResizeManager.getUserResizedWidthPx());

        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ 5000);
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
        mResizeManager.updateLiveResizeState(/* proposedWidthPx= */ 800);

        mResizeManager.clearLiveResizeState();
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());

        // A commit after the drag ended has nothing to commit.
        mResizeManager.commitFinalResizeState(/* finalWidthPx= */ 800, MIN_WIDTH_DP);
        assertEquals(600, (int) mResizeManager.getUserResizedWidthPx());
    }

    /** Commits a drag that ends at {@code widthPx}. */
    private void commit(int widthPx) {
        mResizeManager.updateLiveResizeState(widthPx);
        mResizeManager.commitFinalResizeState(widthPx, MIN_WIDTH_DP);
    }
}
