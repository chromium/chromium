// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.floating_pill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;

/** Unit tests for {@link FloatingPillLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FLOATING_PILL_LAYER)
public class FloatingPillLayerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControls;
    @Captor private ArgumentCaptor<BrowserControlsStateProvider.Observer> mObserverCaptor;

    private Activity mActivity;
    private Resources mResources;
    private FloatingPillContainer mContainer;
    private FloatingPillLayer mLayer;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mResources = mActivity.getResources();
        mContainer = createContainer();
        when(mBrowserControls.getTopControlsHeight()).thenReturn(100);
        when(mBrowserControls.getControlsPosition()).thenReturn(ControlsPosition.TOP);
        mLayer = new FloatingPillLayer(mTopControlsStacker, mBrowserControls);
    }

    @Test
    public void testLayerProperties() {
        assertEquals(TopControlType.FLOATING_PILL, mLayer.getTopControlType());
        assertEquals(0, mLayer.getTopControlHeight());
        assertFalse(mLayer.contributesToTotalHeight());
    }

    @Test
    public void testAddContainerPlacesIt() {
        mLayer.addContainer(mContainer);

        verify(mTopControlsStacker).addControl(mLayer);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        assertTopMargin(mContainer, 100, /* controlsAtBottom= */ false);
    }

    @Test
    public void testFollowsTopControlsHeight() {
        FloatingPillContainer other = createContainer();
        mLayer.addContainer(mContainer);
        mLayer.addContainer(other);
        // Joins the stacker and observes the controls once, however many containers it has.
        verify(mTopControlsStacker).addControl(mLayer);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());

        mLayer.onTopControlLayerHeightChanged(150, 0);

        assertTopMargin(mContainer, 150, /* controlsAtBottom= */ false);
        assertTopMargin(other, 150, /* controlsAtBottom= */ false);

        // In the stacker, the height only comes from onTopControlLayerHeightChanged().
        mObserverCaptor.getValue().onTopControlsHeightChanged(200, 0);
        assertTopMargin(mContainer, 150, /* controlsAtBottom= */ false);
        assertTopMargin(other, 150, /* controlsAtBottom= */ false);
    }

    @Test
    public void testFollowsControlsPosition() {
        mLayer.addContainer(mContainer);
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());

        when(mBrowserControls.getControlsPosition()).thenReturn(ControlsPosition.BOTTOM);
        mObserverCaptor.getValue().onControlsPositionChanged(ControlsPosition.BOTTOM);

        assertTopMargin(mContainer, 100, /* controlsAtBottom= */ true);
    }

    @Test
    public void testLeavesStackerWithLastContainer() {
        FloatingPillContainer other = createContainer();
        mLayer.addContainer(mContainer);
        mLayer.addContainer(other);

        mLayer.removeContainer(mContainer);
        verify(mTopControlsStacker, never()).removeControl(mLayer);
        verify(mBrowserControls, never()).removeObserver(any());

        mLayer.removeContainer(other);
        verify(mTopControlsStacker).removeControl(mLayer);
        verify(mBrowserControls).removeObserver(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FLOATING_PILL_LAYER)
    public void testWithoutFeatureStaysOutOfStacker() {
        mLayer = new FloatingPillLayer(mTopControlsStacker, mBrowserControls);
        mLayer.addContainer(mContainer);
        verify(mTopControlsStacker, never()).addControl(mLayer);

        // Follows the controls' height on its own.
        verify(mBrowserControls).addObserver(mObserverCaptor.capture());
        mObserverCaptor.getValue().onTopControlsHeightChanged(150, 0);
        assertTopMargin(mContainer, 150, /* controlsAtBottom= */ false);

        mLayer.removeContainer(mContainer);
        verify(mTopControlsStacker, never()).removeControl(mLayer);
        verify(mBrowserControls).removeObserver(mObserverCaptor.getValue());
    }

    @Test
    public void testDestroyLeavesStacker() {
        mLayer.addContainer(mContainer);

        mLayer.destroy();

        verify(mTopControlsStacker).removeControl(mLayer);
        verify(mBrowserControls).removeObserver(any());
    }

    private FloatingPillContainer createContainer() {
        FloatingPillContainer container = new FloatingPillContainer(mActivity);
        new FrameLayout(mActivity)
                .addView(
                        container,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT));
        return container;
    }

    private void assertTopMargin(
            FloatingPillContainer container, int topControlsHeight, boolean controlsAtBottom) {
        assertEquals(
                FloatingPillContainer.getTopMargin(mResources, topControlsHeight, controlsAtBottom),
                ((ViewGroup.MarginLayoutParams) container.getLayoutParams()).topMargin);
    }
}
