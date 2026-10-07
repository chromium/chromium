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
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.Collection;

@RunWith(ParameterizedRobolectricTestRunner.class)
public class EdgeToEdgeBottomChinMediatorTest {
    @Parameters
    public static Collection testCases() {
        return Arrays.asList(false, true);
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private LayoutManager mLayoutManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private FullscreenManager mFullscreenManager;

    private PropertyModel mModel;
    private EdgeToEdgeBottomChinMediator mMediator;
    private final boolean mIsTablet;

    private static final int DEFAULT_HEIGHT = 60;
    private static final int BAR_COLOR = Color.LTGRAY;
    private static final int TAB_BACKGROUND_COLOR = Color.BLUE;
    private static final int BOTTOM_BAR_HEIGHT = 180;
    private static final int TOTAL_BOTTOM_CONTROLS_HEIGHT = BOTTOM_BAR_HEIGHT + DEFAULT_HEIGHT;

    public EdgeToEdgeBottomChinMediatorTest(boolean isTablet) {
        mIsTablet = isTablet;
    }

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(EdgeToEdgeBottomChinProperties.ALL_KEYS).build();
        // set a default height for testing.
        doReturn(42).when(mEdgeToEdgeController).getSystemBottomInsetPx();
        mMediator =
                new EdgeToEdgeBottomChinMediator(
                        mModel,
                        mKeyboardVisibilityDelegate,
                        mInsetObserver,
                        mLayoutManager,
                        mEdgeToEdgeController,
                        mBottomControlsStacker,
                        mFullscreenManager,
                        !mIsTablet);
    }

    @Test
    public void testInitialization() {
        if (mIsTablet) {
            assertEquals(
                    "Should be initialized to the default height.",
                    mModel.get(HEIGHT),
                    mModel.get(Y_OFFSET));
        } else {
            assertEquals(0, mModel.get(Y_OFFSET));
        }

        verify(mKeyboardVisibilityDelegate).addKeyboardVisibilityListener(eq(mMediator));
        verify(mLayoutManager).addObserver(eq(mMediator));
        verify(mEdgeToEdgeController).registerObserver(eq(mMediator));
        verify(mBottomControlsStacker).addLayer(eq(mMediator));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mKeyboardVisibilityDelegate).removeKeyboardVisibilityListener(eq(mMediator));
        verify(mLayoutManager).removeObserver(eq(mMediator));
        verify(mEdgeToEdgeController).unregisterObserver(eq(mMediator));
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
    public void testUpdateColor_bciv_enabled() {
        OffsetTag offsetTag = OffsetTag.createRandom();
        mModel.set(OFFSET_TAG, offsetTag);
        mModel.set(HEIGHT, DEFAULT_HEIGHT);
        assertEquals("The color should default to 0.", 0, mModel.get(COLOR));

        // make view visible
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        doReturn(0).when(mBrowserControlsStateProvider).getBottomControlOffset();

        mMediator.changeBottomChinColor(Color.BLUE);
        assertEquals("The color should have been updated to blue.", Color.BLUE, mModel.get(COLOR));

        // scroll view but keep it visible
        doReturn(DEFAULT_HEIGHT / 2).when(mBrowserControlsStateProvider).getBottomControlOffset();
        mMediator.changeBottomChinColor(Color.RED);
        assertEquals("The color should have been updated to red.", Color.RED, mModel.get(COLOR));

        // scroll view offscreen
        doReturn(DEFAULT_HEIGHT).when(mBrowserControlsStateProvider).getBottomControlOffset();

        // color shouldn't be applied, but should be cached
        mMediator.changeBottomChinColor(Color.WHITE);
        assertEquals("The color should have not been updated.", Color.RED, mModel.get(COLOR));

        // scroll view back on screen, should apply cached color
        doReturn(0).when(mBrowserControlsStateProvider).getBottomControlOffset();
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertEquals("The cached color should be applied.", Color.WHITE, mModel.get(COLOR));

        // scroll view offscreen
        doReturn(DEFAULT_HEIGHT).when(mBrowserControlsStateProvider).getBottomControlOffset();

        // null out offset tag, browser offset should take over and color should be updated
        mModel.set(OFFSET_TAG, null);
        mMediator.changeBottomChinColor(Color.BLUE);
        assertEquals("The color should have updated to white.", Color.BLUE, mModel.get(COLOR));
    }

    @Test
    public void testDividerColorChanges() {
        // make view visible
        mModel.set(HEIGHT, DEFAULT_HEIGHT);
        mMediator.onBrowserControlsOffsetUpdate(0);

        mMediator.changeBottomChinDividerColor(Color.WHITE);
        assertEquals(
                "The cached divider color should have been updated to WHITE.",
                Color.WHITE,
                mMediator.getDividerColorForTesting());
        assertEquals(
                "The divider color should have been updated to WHITE.",
                Color.WHITE,
                mModel.get(DIVIDER_COLOR));

        mMediator.changeBottomChinDividerColor(Color.TRANSPARENT);
        assertEquals(
                "The divider color should have been updated to TRANSPARENT.",
                Color.TRANSPARENT,
                mModel.get(DIVIDER_COLOR));
        assertEquals(
                "The cached divider color should have been updated to TRANSPARENT.",
                Color.TRANSPARENT,
                mMediator.getDividerColorForTesting());

        // scroll view offscreen
        mMediator.onBrowserControlsOffsetUpdate(mModel.get(HEIGHT));

        // color shouldn't be applied, but should be cached
        mMediator.changeBottomChinDividerColor(Color.WHITE);
        assertEquals(
                "The color should not have not been updated.",
                Color.TRANSPARENT,
                mModel.get(DIVIDER_COLOR));

        // scroll view back on screen, should apply cached color
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertEquals("The cached color should be applied.", Color.WHITE, mModel.get(DIVIDER_COLOR));
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
        assertLayerVisibility(mIsTablet);

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
        assertLayerVisibility(mIsTablet);

        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);

        assertTrue(
                "The chin can still show, even when the page is opted into edge-to-edge.",
                mModel.get(CAN_SHOW));
        assertEquals(LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE, mMediator.getLayerVisibility());
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
        assertLayerVisibility(mIsTablet);

        onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);

        assertTrue(
                "The chin can still show, conditionally, when the page is opted into"
                        + " edge-to-edge.",
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
        assertLayerVisibility(mIsTablet);

        doReturn(true).when(mFullscreenManager).getPersistentFullscreenMode();
        mMediator.onEnterFullscreen(null, null);
        assertFalse("The chin should not be visible when in fullscreen.", mModel.get(CAN_SHOW));
        assertEquals(LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE, mMediator.getLayerVisibility());

        doReturn(false).when(mFullscreenManager).getPersistentFullscreenMode();
        mMediator.onExitFullscreen(null);

        assertTrue("The chin should become visible when exit fullscreen.", mModel.get(CAN_SHOW));
        assertLayerVisibility(mIsTablet);
    }

    @Test
    public void testUpdateSafeAreaConstraint() {
        assertEquals(
                "The chin should be DEFAULT_SCROLL_OFF.",
                BottomControlsStacker.LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                mMediator.getScrollBehavior());

        mMediator.onSafeAreaConstraintChanged(true);
        assertEquals(
                "The chin should NEVER_SCROLL_OFF when safe area constraint presents.",
                BottomControlsStacker.LayerScrollBehavior.NEVER_SCROLL_OFF,
                mMediator.getScrollBehavior());

        mMediator.onSafeAreaConstraintChanged(false);
        assertEquals(
                "The chin should change back to DEFAULT_SCROLL_OFF once constraint removed.",
                BottomControlsStacker.LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                mMediator.getScrollBehavior());
    }

    @Test
    public void testUpdateSafeAreaConstraint_ScrollableWhenStacking_autoPage() {
        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        mMediator.onSafeAreaConstraintChanged(true);
        assertEquals(
                "The chin should NEVER_SCROLL_OFF when safe area constraint presents while"
                        + " on non-opt-in page.",
                BottomControlsStacker.LayerScrollBehavior.NEVER_SCROLL_OFF,
                mMediator.getScrollBehavior());
    }

    @Test
    public void testUpdateSafeAreaConstraint_ScrollableWhenStacking_optInPage() {
        mMediator.onToEdgeChange(60, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ true);
        mMediator.onSafeAreaConstraintChanged(true);
        assertEquals(
                "The chin should DEFAULT_SCROLL_OFF when safe area constraint presents"
                        + " while on opt-in page.",
                LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                mMediator.getScrollBehavior());
    }

    @Test
    public void testOnBrowserControlsOffsetUpdate() {
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
        assertLayerVisibility(mIsTablet);

        mMediator.keyboardVisibilityChanged(true);
        assertTrue(
                "The chin should still be visible as the keyboard has a zero inset.",
                mModel.get(CAN_SHOW));
        assertLayerVisibility(mIsTablet);

        mMediator.onKeyboardInsetChanged(180);
        assertFalse(
                "The chin should not be visible as the keyboard is showing.", mModel.get(CAN_SHOW));
        assertEquals(LayerVisibility.HIDDEN, mMediator.getLayerVisibility());

        mMediator.keyboardVisibilityChanged(false);
        assertTrue(
                "The chin should be visible as the keyboard is no longer showing.",
                mModel.get(CAN_SHOW));

        assertLayerVisibility(mIsTablet);
    }

    /**
     * Tests that the bottom chin remains composited when browser controls scroll back on screen
     * with BCIV enabled. Viz positions the chin and bottom bar together using the renderer's
     * offset, which the browser learns only after viz draws it. Dropping the chin from the browser
     * frame while scrolled off causes it to lag behind the bottom bar when scrolling back on,
     * exposing the tab background underneath. See crbug.com/568447704.
     */
    @Test
    public void testBciv_chinStillCompositedWhenRendererScrollsControlsBackOn() {
        // Bind the model to a real scene layer, as EdgeToEdgeBottomChinCoordinator does, so the
        // assertion is on what is actually submitted in the browser's compositor frame.
        EdgeToEdgeBottomChinSceneLayer sceneLayer =
                new EdgeToEdgeBottomChinSceneLayer(null) {
                    @Override
                    protected void initializeNative() {}
                };
        View androidView = new View(ContextUtils.getApplicationContext());
        androidView.setLayoutParams(
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));
        PropertyModelChangeProcessor.create(
                mModel,
                new EdgeToEdgeBottomChinViewBinder.ViewHolder(androidView, sceneLayer),
                EdgeToEdgeBottomChinViewBinder::bind);

        // Chin can show, BCIV is active and controls are fully shown with the bottom bar's color.
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(DEFAULT_HEIGHT, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        mModel.set(OFFSET_TAG, OffsetTag.createRandom());
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        doReturn(0).when(mBrowserControlsStateProvider).getBottomControlOffset();
        // With BCIV, BottomControlsStacker dispatches the resting offset (0 for the bottom-most
        // layer) and leaves the actual movement to viz.
        mMediator.onBrowserControlsOffsetUpdate(0);
        mMediator.changeBottomChinColor(BAR_COLOR);
        mMediator.changeBottomChinDividerColor(BAR_COLOR);
        assertTrue(mModel.get(IS_VISIBLE));
        assertTrue(sceneLayer.isSceneOverlayTreeShowing());
        assertEquals(BAR_COLOR, mModel.get(COLOR));

        // The renderer scrolls the controls fully off. The browser learns the new offset, and
        // TabbedNavigationBarColorController switches the nav bar color to the tab background
        // because BottomAttachedUiObserver no longer sees visible bottom controls.
        // isVisible() returns false here (since bottomControlOffset >= HEIGHT), so the color change
        // is cached and not applied immediately to the model.
        doReturn(TOTAL_BOTTOM_CONTROLS_HEIGHT)
                .when(mBrowserControlsStateProvider)
                .getBottomControlOffset();
        mMediator.onBrowserControlsOffsetUpdate(0);
        mMediator.changeBottomChinColor(TAB_BACKGROUND_COLOR);
        mMediator.changeBottomChinDividerColor(TAB_BACKGROUND_COLOR);

        // The renderer now scrolls / animates the controls back on (e.g. to an offset smaller than
        // the chin height). Viz applies that offset to the bottom bar and chin OffsetTags in the
        // next frame, before the browser is told about it. The browser frame at this point must
        // therefore still contain the chin, painted to match the bottom bar it is attached to.
        // isVisibleBasedOnOffset() remains true because mYOffset is 0, keeping the scene layer
        // composited with the cached bottom controls color.
        assertTrue(
                "The chin must remain in the browser's compositor frame while BCIV controls its"
                        + " position; otherwise viz moves the bottom bar back on screen before the"
                        + " browser re-adds the chin, exposing the tab background under the bar.",
                sceneLayer.isSceneOverlayTreeShowing());
        assertTrue(mModel.get(IS_VISIBLE));
        assertEquals(
                "The composited chin must use the bottom controls color, not the tab background"
                        + " color that was applied while the controls were scrolled off.",
                BAR_COLOR,
                mModel.get(COLOR));
    }

    @Test
    public void testBciv_isVisibleFalseWhenStackerHidesChin() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(DEFAULT_HEIGHT, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        mModel.set(OFFSET_TAG, OffsetTag.createRandom());
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        doReturn(0).when(mBrowserControlsStateProvider).getBottomControlOffset();

        // When the stacker dispatches an offset equal to the chin height, the chin is hidden
        // (isVisibleBasedOnOffset is false).
        mMediator.onBrowserControlsOffsetUpdate(DEFAULT_HEIGHT);
        assertFalse(mModel.get(IS_VISIBLE));

        // When the stacker dispatches the resting offset (0), the chin is visible again.
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertTrue(mModel.get(IS_VISIBLE));
    }

    @Test
    public void testNonBciv_isVisibleTracksDispatchedOffset() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        onToEdgeChange(DEFAULT_HEIGHT, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
        mModel.set(OFFSET_TAG, null);

        // When the dispatched offset is less than the chin height, the chin is visible
        // (isVisibleBasedOnOffset is true).
        mMediator.onBrowserControlsOffsetUpdate(DEFAULT_HEIGHT - 1);
        assertTrue(mModel.get(IS_VISIBLE));

        // When the stacker dispatches an offset equal to the chin height, the chin is hidden
        // (isVisibleBasedOnOffset is false).
        mMediator.onBrowserControlsOffsetUpdate(DEFAULT_HEIGHT);
        assertFalse(mModel.get(IS_VISIBLE));

        // When the stacker dispatches the resting offset (0), the chin is visible again.
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertTrue(mModel.get(IS_VISIBLE));
    }

    private void assertLayerVisibility(boolean isTablet) {
        if (isTablet) {
            assertEquals(LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE, mMediator.getLayerVisibility());
        } else {
            assertEquals(LayerVisibility.VISIBLE, mMediator.getLayerVisibility());
        }
    }

    private void onToEdgeChange(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
        doReturn(bottomInset).when(mEdgeToEdgeController).getSystemBottomInsetPx();
        mMediator.onToEdgeChange(bottomInset, isDrawingToEdge, isPageOptInToEdge);
    }
}
