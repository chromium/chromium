// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import static org.junit.Assert.assertEquals;
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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.toolbar.ControlContainer;

/** Unit tests for {@link TabStripTopControlLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStripTopControlLayerUnitTest {

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private ControlContainer mControlContainer;
    @Mock private TabStripSceneLayerHolder mTabStripSceneLayerHolder;

    private TabStripTopControlLayer mTabStripTopControlLayer;
    private final CallbackHelper mOnTransitionStartedCallback = new CallbackHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(0, mTopControlsStacker, mControlContainer);
        mTabStripTopControlLayer.initializeWithNative(mTabStripSceneLayerHolder);
    }

    @Test
    public void testTransitionToVisible() {
        requestTransition(100, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        // First update to start the transition
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verifyHeightTransitionStarted(/* newHeight= */ 100, /* applyScrimOverlay= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        clearInvocations(mTabStripSceneLayerHolder);

        // Update, not reaching resting position
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished(anyBoolean());
        clearInvocations(mTabStripSceneLayerHolder);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(1)).onHeightTransitionFinished(true);
    }

    @Test
    public void testTransitionToHidden() {
        mTabStripTopControlLayer.set(100);
        requestTransition(0, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verifyHeightTransitionStarted(/* newHeight= */ 0, /* applyScrimOverlay= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        clearInvocations(mTabStripSceneLayerHolder);

        // Another update to start the transition
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished(anyBoolean());
        clearInvocations(mTabStripSceneLayerHolder);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished(true);
    }

    @Test
    public void testNoTransitionHeightIncrease() {
        mTabStripTopControlLayer.set(100);
        requestTransition(120, false);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        verifyHeightTransitionStarted(/* newHeight= */ 120, /* applyScrimOverlay= */ false);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished(true);
    }

    @Test
    public void testNotStartingTransitionBeforeTabStripInitialized() {
        // Recreate the stacker without setting the tab strip.
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(0, mTopControlsStacker, mControlContainer);
        requestTransition(100, false);

        verifyHeightTransitionNotStarted();
    }

    @Test
    public void testStartingTransitionBackToBack() {
        requestTransition(100, true);
        requestTransition(120, true);

        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished(false);
    }

    @Test
    public void testDestroy() {
        mTabStripTopControlLayer.destroy();
        verify(mTopControlsStacker).removeControl(mTabStripTopControlLayer);
    }

    private void requestTransition(int newHeight, boolean applyScrimOverlay) {
        mTabStripTopControlLayer.onTransitionRequested(
                newHeight, applyScrimOverlay, mOnTransitionStartedCallback::notifyCalled);
    }

    private void verifyHeightTransitionStarted(int newHeight, boolean applyScrimOverlay) {
        assertEquals(
                "mOnTransitionStartedCallback is not called.",
                1,
                mOnTransitionStartedCallback.getCallCount());
        verify(mControlContainer).onHeightChanged(newHeight, applyScrimOverlay);
        verify(mTabStripSceneLayerHolder).onHeightChanged(newHeight, applyScrimOverlay);
    }

    private void verifyHeightTransitionNotStarted() {
        assertEquals(
                "mOnTransitionStartedCallback should not trigger yet.",
                0,
                mOnTransitionStartedCallback.getCallCount());
        verify(mControlContainer, times(0)).onHeightChanged(anyInt(), anyBoolean());
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());
    }
}
