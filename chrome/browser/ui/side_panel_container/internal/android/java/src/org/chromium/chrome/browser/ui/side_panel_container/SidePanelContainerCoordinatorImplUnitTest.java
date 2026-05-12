// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator.MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL;
import static org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator.NARROW_SIDE_PANEL_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator.WIDE_SIDE_PANEL_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_panel.SidePanelCoordinatorAndroid;
import org.chromium.chrome.browser.ui.side_panel.SidePanelType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;

/** Unit tests for {@link SidePanelContainerCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SidePanelContainerCoordinatorImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiCoordinator mMockSideUiCoordinator;

    private Activity mTestActivity;

    @Before
    public void setUp() {
        mTestActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    @Test
    public void init_registerSelfAsSideUiContainer() {
        var sidePanelContainerCoordinator = createSidePanelContainerCoordinator();

        sidePanelContainerCoordinator.init(mock(SidePanelCoordinatorAndroid.class));

        verify(mMockSideUiCoordinator).registerSideUiContainer(sidePanelContainerCoordinator);
    }

    @Test
    public void destroy_unregisterSelfAsSideUiContainer() {
        var sidePanelContainerCoordinator = createSidePanelContainerCoordinator();

        sidePanelContainerCoordinator.destroy();

        verify(mMockSideUiCoordinator).unregisterSideUiContainer(sidePanelContainerCoordinator);
    }

    @Test
    public void determineContainerWidthDp_calculatePerWindowWidthAndAvailableWidth() {
        // 1. Wide side panel.
        int windowWidthDp = MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL;
        int availableWidthDp = WIDE_SIDE_PANEL_WIDTH_DP;
        assertEquals(
                WIDE_SIDE_PANEL_WIDTH_DP,
                SidePanelContainerCoordinatorImpl.determineContainerWidthDp(
                        availableWidthDp, windowWidthDp));

        // 2. Narrow side panel.
        windowWidthDp = MIN_WINDOW_WIDTH_DP_FOR_WIDE_SIDE_PANEL - 10;
        availableWidthDp = NARROW_SIDE_PANEL_WIDTH_DP;
        assertEquals(
                NARROW_SIDE_PANEL_WIDTH_DP,
                SidePanelContainerCoordinatorImpl.determineContainerWidthDp(
                        availableWidthDp, windowWidthDp));

        // 3. Fill available space.
        windowWidthDp = MIN_WEB_CONTENTS_WIDTH_DP + (NARROW_SIDE_PANEL_WIDTH_DP - 10);
        availableWidthDp = NARROW_SIDE_PANEL_WIDTH_DP - 10;
        assertEquals(
                availableWidthDp,
                SidePanelContainerCoordinatorImpl.determineContainerWidthDp(
                        availableWidthDp, windowWidthDp));
    }

    private SidePanelContainerCoordinatorImpl createSidePanelContainerCoordinator() {
        return new SidePanelContainerCoordinatorImpl(
                mTestActivity, mMockSideUiCoordinator, SidePanelType.TOOLBAR);
    }
}
