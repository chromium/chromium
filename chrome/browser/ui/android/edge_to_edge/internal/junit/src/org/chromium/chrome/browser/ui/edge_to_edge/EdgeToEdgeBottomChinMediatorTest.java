// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.CAN_SHOW;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.DIVIDER_COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;

import android.graphics.Color;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR)
@Config(manifest = Config.NONE)
public class EdgeToEdgeBottomChinMediatorTest {
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private LayoutManager mLayoutManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private NavigationBarColorProvider mNavigationBarColorProvider;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private FullscreenManager mFullscreenManager;

    private PropertyModel mModel;
    private EdgeToEdgeBottomChinMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS).build();
        mMediator =
                new EdgeToEdgeBottomChinMediator(
                        mModel,
                        mKeyboardVisibilityDelegate,
                        mLayoutManager,
                        mEdgeToEdgeController,
                        mNavigationBarColorProvider,
                        mBottomControlsStacker,
                        mFullscreenManager);
    }

    @Test
    public void testInitialization() {
        assertEquals(0, mModel.get(Y_OFFSET));

        verify(mKeyboardVisibilityDelegate).addKeyboardVisibilityListener(eq(mMediator));
        verify(mLayoutManager).addObserver(eq(mMediator));
        verify(mEdgeToEdgeController).registerObserver(eq(mMediator));
        verify(mNavigationBarColorProvider).addObserver(eq(mMediator));
        verify(mBottomControlsStacker).addLayer(eq(mMediator));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mKeyboardVisibilityDelegate).removeKeyboardVisibilityListener(eq(mMediator));
        verify(mLayoutManager).removeObserver(eq(mMediator));
        verify(mEdgeToEdgeController).unregisterObserver(eq(mMediator));
        verify(mNavigationBarColorProvider).removeObserver(eq(mMediator));
        verify(mBottomControlsStacker).removeLayer(eq(mMediator));
    }

    @Test
    public void testUpdateHeight() {
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(
                "The height should have adjusted to match the edge-to-edge bottom inset in pixels.",
                60,
                mModel.get(HEIGHT));

        onToEdgeChange(100, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(
                "The height should have been increased to match the edge-to-edge bottom inset.",
                100,
                mModel.get(HEIGHT));

        onToEdgeChange(0, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertEquals(
                "The height should have been cleared to 0 to match the edge-to-edge bottom inset.",
                0,
                mModel.get(HEIGHT));
    }

    @Test
    public void testUpdateColor() {
        mMediator.onNavigationBarColorChanged(Color.BLUE);
        assertEquals("The color should have been updated to blue.", Color.BLUE, mModel.get(COLOR));

        mMediator.onNavigationBarColorChanged(Color.RED);
        assertEquals("The color should have been updated to red.", Color.RED, mModel.get(COLOR));
    }

    @Test
    public void testDividerColorChanges() {
        mMediator.onNavigationBarDividerChanged(Color.WHITE);
        assertEquals(
                "The divider color should have been updated to WHITE.",
                Color.WHITE,
                mModel.get(DIVIDER_COLOR));

        mMediator.onNavigationBarDividerChanged(Color.TRANSPARENT);
        assertEquals(
                "The divider color should have been updated to TRANSPARENT.",
                Color.TRANSPARENT,
                mModel.get(DIVIDER_COLOR));
    }

    @Test
    public void testUpdateVisibility_updatesStacker() {
        clearInvocations(mBottomControlsStacker);

        assertFalse(
                "The chin should not be visible as it has just been initialized.",
                mModel.get(CAN_SHOW));

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mMediator.onStartedShowing(LayoutType.BROWSING);
        onToEdgeChange(0, /* isDrawingToEdge= */ false, /* isPageOptInToEdge= */ false);
        assertFalse(
                "The chin should not be visible as the edge-to-edge bottom inset is still 0.",
                mModel.get(CAN_SHOW));
        assertEquals(
                BottomControlsStacker.LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                mMediator.getLayerVisibility());
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());

        doReturn(LayoutType.NONE).when(mLayoutManager).getActiveLayoutType();
        mMediator.onStartedShowing(LayoutType.NONE);
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertFalse(
                "The chin should not be visible as the layout type does not support showing the"
                        + " chin.",
                mModel.get(CAN_SHOW));
        assertEquals(
                BottomControlsStacker.LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                mMediator.getLayerVisibility());
        // Height was updated.
        verify(mBottomControlsStacker, times(1)).requestLayerUpdate(anyBoolean());

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mMediator.onStartedShowing(LayoutType.BROWSING);
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertTrue("The chin should be visible as all conditions are met.", mModel.get(CAN_SHOW));
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mMediator.getLayerVisibility());
        // Visibility was updated.
        verify(mBottomControlsStacker, times(2)).requestLayerUpdate(anyBoolean());

        clearInvocations(mBottomControlsStacker);

        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        // Duplicate height notification, no updates.
        verify(mBottomControlsStacker, never()).requestLayerUpdate(anyBoolean());
    }

    @Test
    public void testUpdateVisibility_VisibleChin() {
        assertFalse(
                "The chin should not be visible as it has just been initialized.",
                mModel.get(CAN_SHOW));

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertTrue("The chin should be visible as all conditions are met.", mModel.get(CAN_SHOW));

        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);
        assertTrue(
                "The chin can still show, even when the page is opted into edge-to-edge.",
                mModel.get(CAN_SHOW));
    }

    @Test
    public void testUpdateVisibility_PageOptedIn() {
        clearInvocations(mBottomControlsStacker);

        assertFalse(
                "The chin should not be visible as it has just been initialized.",
                mModel.get(CAN_SHOW));

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mMediator.onStartedShowing(LayoutType.BROWSING);
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertTrue("The chin should be visible as all conditions are met.", mModel.get(CAN_SHOW));
        assertEquals(BottomControlsStacker.LayerVisibility.VISIBLE, mMediator.getLayerVisibility());

        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);
        assertTrue(
                "The chin can still show, conditionally, when the page is opted into edge-to-edge.",
                mModel.get(CAN_SHOW));
        assertEquals(
                BottomControlsStacker.LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                mMediator.getLayerVisibility());
    }

    @Test
    public void testUpdateVisibility_NoInsets() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(0, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertFalse(
                "The chin should not be visible as the edge-to-edge bottom inset is 0.",
                mModel.get(CAN_SHOW));
    }

    @Test
    public void testUpdateVisibility_NoneLayoutType() {
        doReturn(LayoutType.NONE).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertFalse(
                "The chin should not be visible as the layout type does not support showing the"
                        + " chin.",
                mModel.get(CAN_SHOW));
    }

    @Test
    public void testUpdateVisibility_NotToEdge() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(60, /* isDrawingToEdge= */ false, /* isPageOptInToEdge= */ false);
        assertFalse(
                "The chin should not be visible when not drawing to edge.", mModel.get(CAN_SHOW));
    }

    @Test
    public void testUpdateVisibility_Fullscreen() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertTrue("The chin should be visible.", mModel.get(CAN_SHOW));

        doReturn(true).when(mFullscreenManager).getPersistentFullscreenMode();
        mMediator.onEnterFullscreen(null, null);
        assertFalse("The chin should not be visible when in fullscreen.", mModel.get(CAN_SHOW));

        doReturn(false).when(mFullscreenManager).getPersistentFullscreenMode();
        mMediator.onExitFullscreen(null);
        assertTrue("The chin should become visible when exit fullscreen.", mModel.get(CAN_SHOW));
    }

    @Test
    public void testOnBrowserControlsOffsetUpdate() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.sDisableBottomControlsStackerYOffsetDispatching, "false");
        testValues.addFeatureFlagOverride(ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR, true);
        FeatureList.setTestValues(testValues);

        mMediator.onBrowserControlsOffsetUpdate(0);
        assertEquals("The y-offset should be 0.", 0, mModel.get(Y_OFFSET));

        mMediator.onBrowserControlsOffsetUpdate(10);
        assertEquals("The y-offset should be 10.", 10, mModel.get(Y_OFFSET));

        mMediator.onBrowserControlsOffsetUpdate(60);
        assertEquals("The y-offset should be 60.", 60, mModel.get(Y_OFFSET));
    }

    @Test
    public void testKeyboardVisibilityChanged() {
        assertFalse(
                "The chin should not be visible as it has just been initialized.",
                mModel.get(CAN_SHOW));

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        assertTrue("The chin should be visible as all conditions are met.", mModel.get(CAN_SHOW));

        mMediator.keyboardVisibilityChanged(true);
        assertFalse(
                "The chin should not be visible as the keyboard is showing.", mModel.get(CAN_SHOW));

        mMediator.keyboardVisibilityChanged(false);
        assertTrue(
                "The chin should be visible as the keyboard is no longer showing.",
                mModel.get(CAN_SHOW));
    }

    private void onToEdgeChange(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        doReturn(bottomInset).when(mEdgeToEdgeController).getSystemBottomInsetPx();
        mMediator.onToEdgeChange(bottomInset, isDrawingToEdge, isPageOptInToEdge);
    }
}
