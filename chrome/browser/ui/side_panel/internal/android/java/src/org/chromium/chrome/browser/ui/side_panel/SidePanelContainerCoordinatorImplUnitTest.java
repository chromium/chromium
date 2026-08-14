// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.MIN_SIDE_PANEL_CONTENT_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL;
import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.NARROW_SIDE_PANEL_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;

/** Unit tests for {@link SidePanelContainerCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SidePanelContainerCoordinatorImplUnitTest {

    @Test
    public void determineShowableWidthDp_calculatePerWindowWidthAndAvailableWidth() {
        // 1. Wide side panel.
        int windowWidthDp = MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL;
        int availableWidthDp = WIDE_SIDE_PANEL_WIDTH_DP;
        int minSidePanelContainerWidthDp =
                MIN_SIDE_PANEL_CONTENT_WIDTH_DP + 12 /* horizontal padding */;

        assertEquals(
                WIDE_SIDE_PANEL_WIDTH_DP,
                SidePanelContainerCoordinatorImpl.determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp));

        // 2. Narrow side panel.
        windowWidthDp = MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL - 10;
        availableWidthDp = NARROW_SIDE_PANEL_WIDTH_DP;
        assertEquals(
                NARROW_SIDE_PANEL_WIDTH_DP,
                SidePanelContainerCoordinatorImpl.determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp));

        // 3. Fill available space.
        availableWidthDp = minSidePanelContainerWidthDp + 10;
        windowWidthDp = MIN_WEB_CONTENTS_WIDTH_DP + availableWidthDp;
        assertEquals(
                availableWidthDp,
                SidePanelContainerCoordinatorImpl.determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp));

        // 4. Not enough space to accommodate MIN_SIDE_PANEL_WIDTH_DP.
        availableWidthDp = minSidePanelContainerWidthDp - 10;
        windowWidthDp = MIN_WEB_CONTENTS_WIDTH_DP + availableWidthDp;
        assertEquals(
                0,
                SidePanelContainerCoordinatorImpl.determineShowableWidthDp(
                        availableWidthDp, windowWidthDp, minSidePanelContainerWidthDp));
    }

    @Test
    public void determineHeightType_calculatePerShowableWidthAndVerticalTabsState() {
        assertEquals(
                HeightType.NOT_APPLICABLE,
                SidePanelContainerCoordinatorImpl.determineHeightType(
                        /* showableWidthDp= */ 0, /* isVerticalTabsEnabled= */ false));
        assertEquals(
                HeightType.NOT_APPLICABLE,
                SidePanelContainerCoordinatorImpl.determineHeightType(
                        /* showableWidthDp= */ 0, /* isVerticalTabsEnabled= */ true));
        assertEquals(
                HeightType.TOOLBAR,
                SidePanelContainerCoordinatorImpl.determineHeightType(
                        /* showableWidthDp= */ WIDE_SIDE_PANEL_WIDTH_DP,
                        /* isVerticalTabsEnabled= */ false));
        assertEquals(
                HeightType.WEB_CONTENTS,
                SidePanelContainerCoordinatorImpl.determineHeightType(
                        /* showableWidthDp= */ WIDE_SIDE_PANEL_WIDTH_DP,
                        /* isVerticalTabsEnabled= */ true));
    }
}
