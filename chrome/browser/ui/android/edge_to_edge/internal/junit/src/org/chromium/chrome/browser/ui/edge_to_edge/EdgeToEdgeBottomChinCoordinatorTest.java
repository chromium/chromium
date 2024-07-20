// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EdgeToEdgeBottomChinCoordinatorTest {
    @Mock private LayoutManager mLayoutManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private NavigationBarColorProvider mNavigationBarColorProvider;
    @Mock private EdgeToEdgeBottomChinSceneLayer mEdgeToEdgeBottomChinSceneLayer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testEdgeToEdgeBottomChinCoordinator() {
        EdgeToEdgeBottomChinCoordinator coordinator =
                new EdgeToEdgeBottomChinCoordinator(
                        mLayoutManager,
                        mEdgeToEdgeController,
                        mNavigationBarColorProvider,
                        mBottomControlsStacker,
                        mEdgeToEdgeBottomChinSceneLayer);
        verify(mLayoutManager).addSceneOverlay(any());

        coordinator.destroy();
        verify(mEdgeToEdgeBottomChinSceneLayer).destroy();
    }
}
