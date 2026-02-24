// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;

/** Unit tests for {@link SidePanelContainerCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SidePanelContainerCoordinatorImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SideUiCoordinator mMockSideUiCoordinator;

    @Test
    public void init_registerSelfAsSideUiContainer() {
        var sidePanelContainerCoordinator =
                new SidePanelContainerCoordinatorImpl(mMockSideUiCoordinator);

        sidePanelContainerCoordinator.init();

        verify(mMockSideUiCoordinator).registerSideUiContainer(sidePanelContainerCoordinator);
    }

    @Test
    public void destroy_unregisterSelfAsSideUiContainer() {
        var sidePanelContainerCoordinator =
                new SidePanelContainerCoordinatorImpl(mMockSideUiCoordinator);

        sidePanelContainerCoordinator.destroy();

        verify(mMockSideUiCoordinator).unregisterSideUiContainer(sidePanelContainerCoordinator);
    }
}
