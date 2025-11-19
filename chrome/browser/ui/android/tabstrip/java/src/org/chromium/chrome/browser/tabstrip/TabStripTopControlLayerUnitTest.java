// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;

/** Unit tests for {@link TabStripTopControlLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStripTopControlLayerUnitTest {

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private TabStripSceneLayerHolder mTabStripSceneLayerHolder;

    private TabStripTopControlLayer mTabStripTopControlLayer;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mTabStripTopControlLayer = new TabStripTopControlLayer(0, mTopControlsStacker);
        mTabStripTopControlLayer.initializeWithNative(mTabStripSceneLayerHolder);
    }

    @Test
    public void testTransitionToVisible() {
        mTabStripTopControlLayer.onTransitionRequested(100, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        // First update to start the transition
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onHeightChanged(100, true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        clearInvocations(mTabStripSceneLayerHolder);

        // Update, not reaching resting position
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished();
        clearInvocations(mTabStripSceneLayerHolder);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(1)).onHeightTransitionFinished();
    }

    @Test
    public void testTransitionToHidden() {
        mTabStripTopControlLayer.set(100);
        mTabStripTopControlLayer.onTransitionRequested(0, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onHeightChanged(0, true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        clearInvocations(mTabStripSceneLayerHolder);

        // Another update to start the transition
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished();
        clearInvocations(mTabStripSceneLayerHolder);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished();
    }

    @Test
    public void testNoTransitionHeightIncrease() {
        mTabStripTopControlLayer.set(100);
        mTabStripTopControlLayer.onTransitionRequested(120, false);
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        verify(mTabStripSceneLayerHolder).onHeightChanged(120, false);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished();
    }

    @Test
    public void testDestroy() {
        mTabStripTopControlLayer.destroy();
        verify(mTopControlsStacker).removeControl(mTabStripTopControlLayer);
    }
}
