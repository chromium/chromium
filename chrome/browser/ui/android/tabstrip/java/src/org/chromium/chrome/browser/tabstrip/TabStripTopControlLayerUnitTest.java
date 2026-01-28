// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabstrip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.ui.util.TokenHolder;

/** Unit tests for {@link TabStripTopControlLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStripTopControlLayerUnitTest {

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private ControlContainer mControlContainer;
    @Mock private TabStripSceneLayerHolder mTabStripSceneLayerHolder;
    @Mock private BrowserControlsStateProvider mBrowserControls;
    @Mock private View mControlContainerView;

    private TabStripTopControlLayer mTabStripTopControlLayer;
    private final CallbackHelper mOnTransitionStartedCallback = new CallbackHelper();
    private final CallbackHelper mOnTokenUpdateCallback = new CallbackHelper();
    private TokenHolder mTokenHolder;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        doReturn(true).when(mTopControlsStacker).isLayerAtTop(TopControlType.TABSTRIP);
        doReturn(mControlContainerView).when(mControlContainer).getView();
        mTokenHolder = new TokenHolder(mOnTokenUpdateCallback::notifyCalled);
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(
                        0, mTopControlsStacker, mBrowserControls, mControlContainer, mTokenHolder);
        mTabStripTopControlLayer.initializeWithNative(mTabStripSceneLayerHolder);
    }

    @Test
    public void testTransitionToVisible() {
        requestTransition(100, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        // First update to start the transition. Visible height still at 0.
        doReturn(-100).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verifyHeightTransitionStarted(/* newHeight= */ 100, /* applyScrimOverlay= */ true);
        verifyLayerUpdateRequest(true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 0);
        clearInvocations(mTabStripSceneLayerHolder);

        // Update, move by 40, not reaching resting position.
        // Offset at -60, Visible height being 40.
        doReturn(-60).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 40);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished(anyBoolean());
        clearInvocations(mTabStripSceneLayerHolder);

        // Finishing, top control offset reach 0.
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 100);
        verify(mTabStripSceneLayerHolder, times(1)).onHeightTransitionFinished(true);
    }

    @Test
    public void testTransitionToHidden() {
        mTabStripTopControlLayer.set(100);
        requestTransition(0, true);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());

        // Start transition. Top controls offset change from 100 -> 0.
        doReturn(100).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verifyHeightTransitionStarted(/* newHeight= */ 0, /* applyScrimOverlay= */ true);
        verifyLayerUpdateRequest(true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 100);
        clearInvocations(mTabStripSceneLayerHolder);

        // Another update to mid the transition. Controls moved by 60.
        // Top offset = 100 - 60 = 40; visibleHeight being 40.
        doReturn(40).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(
                0, /* reachRestingPosition= */ false);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 40);
        verify(mTabStripSceneLayerHolder, times(0)).onHeightTransitionFinished(anyBoolean());
        clearInvocations(mTabStripSceneLayerHolder);

        // Transition finishing. Top control offset reach 0. Strip fully hidden.
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, /* reachRestingPosition= */ true);
        verify(mTabStripSceneLayerHolder).onLayerYOffsetChanged(0, 0);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished(true);
    }

    @Test
    public void testNoAnimationTransitionHeightIncrease() {
        mTabStripTopControlLayer.set(100);
        requestTransition(120, false);

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        verifyHeightTransitionStarted(/* newHeight= */ 120, /* applyScrimOverlay= */ false);
        verifyLayerUpdateRequest(false);
        verify(mTabStripSceneLayerHolder).onHeightTransitionFinished(true);
    }

    @Test
    public void testNotStartingTransitionBeforeTabStripInitialized() {
        BrowserControlsUtils.setsSyncMinHeightWithTotalHeightForTesting(false);
        // Recreate the stacker without setting the tab strip.
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(
                        0, mTopControlsStacker, mBrowserControls, mControlContainer, mTokenHolder);
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
    public void testTokenHolder() {
        requestTransition(100, true);
        assertEquals("Token should be acquired.", 1, mOnTokenUpdateCallback.getCallCount());
        assertTrue("Token should be held during transition.", mTokenHolder.hasTokens());

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        assertEquals(
                "Token should be released after transition finished.",
                2,
                mOnTokenUpdateCallback.getCallCount());
        assertFalse("Token should be released.", mTokenHolder.hasTokens());
    }

    @Test
    public void testTokenHolder_InterruptedTransition() {
        requestTransition(100, true);
        assertEquals("Token should be acquired.", 1, mOnTokenUpdateCallback.getCallCount());
        assertTrue("Token should be held during transition.", mTokenHolder.hasTokens());

        // Request another transition while the first one is ongoing.
        requestTransition(120, true);
        assertEquals(
                "Token update callback should not be triggered for nested transition.",
                1,
                mOnTokenUpdateCallback.getCallCount());
        assertTrue("Token should still be held.", mTokenHolder.hasTokens());

        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        assertEquals(
                "Token should be released after transition finished.",
                2,
                mOnTokenUpdateCallback.getCallCount());
        assertFalse("Token should be released.", mTokenHolder.hasTokens());
    }

    @Test
    public void testDestroy() {
        mTabStripTopControlLayer.destroy();
        verify(mTopControlsStacker).removeControl(mTabStripTopControlLayer);
    }

    @Test
    public void testTokenReleasedOnDestroy() {
        requestTransition(100, true);
        assertEquals("Token should be acquired.", 1, mOnTokenUpdateCallback.getCallCount());

        mTabStripTopControlLayer.destroy();
        assertEquals(
                "Token should be released after destroy.",
                2,
                mOnTokenUpdateCallback.getCallCount());
    }

    @Test
    public void testTransitionWithoutTabStrip() {
        BrowserControlsUtils.setsSyncMinHeightWithTotalHeightForTesting(true);

        // Recreate the layer without setting the tab strip (without calling initializeWithNative).
        mTabStripTopControlLayer =
                new TabStripTopControlLayer(
                        100,
                        mTopControlsStacker,
                        mBrowserControls,
                        mControlContainer,
                        mTokenHolder);
        requestTransition(120, true);

        // Assume another offset dispatched, which should result in transition started.
        mTabStripTopControlLayer.onBrowserControlsOffsetUpdate(0, true);
        assertEquals(
                "mOnTransitionStartedCallback is not called.",
                1,
                mOnTransitionStartedCallback.getCallCount());
        verify(mControlContainer).onHeightChanged(120, true);
        // The TabStripSceneLayerHolder should not be updated since it's not initialized.
        verify(mTabStripSceneLayerHolder, times(0)).onLayerYOffsetChanged(anyInt(), anyInt());
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

    private void verifyLayerUpdateRequest(boolean animate) {
        verify(mTopControlsStacker).requestLayerUpdateSync(animate);
    }

    private void verifyHeightTransitionNotStarted() {
        assertEquals(
                "mOnTransitionStartedCallback should not trigger yet.",
                0,
                mOnTransitionStartedCallback.getCallCount());
        verify(mControlContainer, times(0)).onHeightChanged(anyInt(), anyBoolean());
        verify(mTabStripSceneLayerHolder, times(0)).onHeightChanged(anyInt(), anyBoolean());
        verify(mTopControlsStacker, times(0)).requestLayerUpdateSync(anyBoolean());
    }
}
