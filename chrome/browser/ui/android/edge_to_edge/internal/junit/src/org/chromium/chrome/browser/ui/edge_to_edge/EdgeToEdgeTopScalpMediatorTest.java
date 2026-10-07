// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;

// EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() requires Android R+ (API 30).
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
public class EdgeToEdgeTopScalpMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private LayoutManager mLayoutManager;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private Tab mTab;
    @Mock private FullscreenOptions mFullscreenOptions;

    private PropertyModel mModel;
    private EdgeToEdgeTopScalpMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(EdgeToEdgeTopScalpProperties.ALL_KEYS).build();
        when(mTopUiThemeColorProvider.getThemeColor()).thenReturn(Color.RED);
    }

    private EdgeToEdgeTopScalpMediator createMediator() {
        return new EdgeToEdgeTopScalpMediator(
                mModel,
                mTopControlsStacker,
                mEdgeToEdgeController,
                mLayoutManager,
                mFullscreenManager,
                mTopUiThemeColorProvider);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void testTopScalp_Disabled() {
        mMediator = createMediator();

        assertEquals(TopControlType.TOP_SCALP, mMediator.getTopControlType());
        assertEquals(0, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.HIDDEN, mMediator.getTopControlVisibility());
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        assertTrue(mMediator.contributesToTotalHeight());
        assertEquals(ScrollBehavior.NEVER_SCROLLABLE, mMediator.getScrollBehavior());
        verify(mTopControlsStacker).addControl(mMediator);
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(false);
        // When the feature is disabled, EdgeToEdgeController state is short-circuited.
        verify(mEdgeToEdgeController, never()).isDrawingToTopEdge();
        verify(mEdgeToEdgeController, never()).getSystemTopInsetPx();

        // Destroying while hidden should not request a layer update.
        mMediator.destroy();
        verify(mTopControlsStacker).removeControl(mMediator);
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(false);
    }

    @Test
    public void testTopScalp_RefactorOnlyArm_Disabled() {
        ChromeFeatureList.sEdgeToEdgeTopInsetEnableTopEdgeToEdge.setForTesting(false);

        mMediator = createMediator();

        assertEquals(0, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.HIDDEN, mMediator.getTopControlVisibility());
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(false);
        // When only the refactor arm is enabled (enable_top_edge_to_edge=false),
        // EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() is false and short-circuits before querying
        // EdgeToEdgeController.
        verify(mEdgeToEdgeController, never()).isDrawingToTopEdge();
        verify(mEdgeToEdgeController, never()).getSystemTopInsetPx();
    }

    @Test
    public void testTopScalp_Enabled() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);

        mMediator = createMediator();

        assertEquals(TopControlType.TOP_SCALP, mMediator.getTopControlType());
        assertEquals(100, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        assertTrue(mMediator.contributesToTotalHeight());
        assertEquals(ScrollBehavior.NEVER_SCROLLABLE, mMediator.getScrollBehavior());
        verify(mTopControlsStacker).addControl(mMediator);
    }

    @Test
    public void testTopScalp_ThemeColor() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(75);

        mMediator = createMediator();
        assertEquals(Color.RED, mModel.get(EdgeToEdgeTopScalpProperties.COLOR));

        mMediator.onThemeColorChanged(Color.GREEN, /* shouldAnimate= */ false);
        assertEquals(Color.GREEN, mModel.get(EdgeToEdgeTopScalpProperties.COLOR));
    }

    @Test
    public void testTopScalp_Destroy() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(60);

        mMediator = createMediator();
        assertEquals(60, mMediator.getTopControlHeight());
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker).addControl(mMediator);
        verify(mEdgeToEdgeController).registerObserver(mMediator);
        verify(mLayoutManager).addObserver(mMediator);
        verify(mFullscreenManager).addObserver(mMediator);
        verify(mTopUiThemeColorProvider).addThemeColorObserver(mMediator);
        clearInvocations(mTopControlsStacker);

        mMediator.destroy();
        verify(mTopControlsStacker).removeControl(mMediator);
        verify(mEdgeToEdgeController).unregisterObserver(mMediator);
        verify(mLayoutManager).removeObserver(mMediator);
        verify(mFullscreenManager).removeObserver(mMediator);
        verify(mTopUiThemeColorProvider).removeThemeColorObserver(mMediator);
    }

    @Test
    public void testTopScalp_Position_BrowserControlsOffsetUpdate() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);

        mMediator = createMediator();

        mMediator.onBrowserControlsOffsetUpdate(
                /* layerYOffset= */ 0, /* reachRestingPosition= */ true);
        assertEquals(0, mModel.get(EdgeToEdgeTopScalpProperties.Y_OFFSET));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));

        mMediator.onBrowserControlsOffsetUpdate(
                /* layerYOffset= */ -50, /* reachRestingPosition= */ false);
        assertEquals(-50, mModel.get(EdgeToEdgeTopScalpProperties.Y_OFFSET));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());

        // When scrolled completely off screen (offset + height <= 0).
        mMediator.onBrowserControlsOffsetUpdate(
                /* layerYOffset= */ -100, /* reachRestingPosition= */ false);
        assertEquals(-100, mModel.get(EdgeToEdgeTopScalpProperties.Y_OFFSET));
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
    }

    @Test
    public void testTopScalp_HeightAndVisibility() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);

        mMediator = createMediator();
        assertEquals(100, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        // When system top inset changes to 0 (e.g. split-screen mode).
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(0);
        mMediator.onToEdgeChange(
                /* bottomInset= */ 0, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(0, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.HIDDEN, mMediator.getTopControlVisibility());
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    public void testTopScalp_FullscreenTransitions() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);
        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(false);
        mMediator = createMediator();
        assertEquals(100, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        // Enter fullscreen.
        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(true);
        mMediator.onEnterFullscreen(mTab, mFullscreenOptions);
        assertEquals(0, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.HIDDEN, mMediator.getTopControlVisibility());
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        // Exit fullscreen.
        when(mFullscreenManager.getPersistentFullscreenMode()).thenReturn(false);
        mMediator.onExitFullscreen(mTab);
        assertEquals(100, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    public void testTopScalp_OnStartedShowingLayout() {
        // Verifies onStartedShowing() triggers updateHeightAndVisibility(); layout-type visibility
        // filtering (e.g. Hub) will be added in crbug.com/540849067.
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);
        mMediator = createMediator();
        assertEquals(100, mMediator.getTopControlHeight());

        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(80);
        mMediator.onStartedShowing(LayoutType.BROWSING);
        assertEquals(80, mMediator.getTopControlHeight());
    }

    @Test
    public void testTopScalp_OnToEdgeChange() {
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);
        mMediator = createMediator();
        assertEquals(100, mMediator.getTopControlHeight());
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(90);
        mMediator.onToEdgeChange(
                /* bottomInset= */ 50, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(90, mMediator.getTopControlHeight());
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        // Unchanged height/visibility should not request another layer update.
        mMediator.onToEdgeChange(
                /* bottomInset= */ 50, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(false);
    }

    @Test
    public void testTopScalp_NotDrawingToTopEdge() {
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(false);

        mMediator = createMediator();

        assertEquals(0, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.HIDDEN, mMediator.getTopControlVisibility());
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertFalse(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
        verify(mTopControlsStacker, never()).requestLayerUpdatePost(false);

        // Transitioning to drawing to top edge should apply the 100px inset.
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        mMediator.onToEdgeChange(
                /* bottomInset= */ 0, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(100, mMediator.getTopControlHeight());
        assertEquals(TopControlVisibility.VISIBLE, mMediator.getTopControlVisibility());
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.CAN_SHOW));
        assertTrue(mModel.get(EdgeToEdgeTopScalpProperties.IS_VISIBLE));
    }
}
