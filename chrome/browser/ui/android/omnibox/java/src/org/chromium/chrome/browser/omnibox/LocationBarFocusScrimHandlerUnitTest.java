// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;

/** Unit tests for {@link LocationBarFocusScrimHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarFocusScrimHandlerUnitTest {
    private static final int HEIGHT_FROM_BOTTOM_TOOLBAR_TO_BOTTOM = 184;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Runnable mClickDelegate;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private ScrimManager mScrimManager;
    @Mock private BottomControlsStacker mBottomControlsStacker;

    private final View mScrimTarget = new View(ContextUtils.getApplicationContext());
    LocationBarFocusScrimHandler mScrimHandler;

    private final SettableNonNullObservableSupplier<Integer> mTabStripHeightSupplier =
            ObservableSuppliers.createNonNull(0);

    @Before
    public void setUp() {
        mScrimHandler =
                new LocationBarFocusScrimHandler(
                        mScrimManager,
                        (visible) -> {},
                        ContextUtils.getApplicationContext(),
                        mLocationBarDataProvider,
                        mClickDelegate,
                        mScrimTarget,
                        mTabStripHeightSupplier,
                        mBottomControlsStacker);
    }

    @Test
    public void testSetVisibility_shownThenHidden() {
        mScrimHandler.setVisibility(true);
        verify(mScrimManager).showScrim(any());

        mScrimHandler.setVisibility(false);
        verify(mScrimManager).hideScrim(any(), eq(true));

        // A second de-focus shouldn't trigger another hide.
        mScrimHandler.setVisibility(false);
        verify(mScrimManager).hideScrim(any(), eq(true));
    }

    @Test
    public void testTabStripHeightChangeCallback() {
        int newHeight = 10;
        mTabStripHeightSupplier.set(newHeight);
        assertEquals(
                "Scrim top margin should be updated when tab strip height changes.",
                newHeight,
                mScrimHandler.getScrimModelForTesting().get(ScrimProperties.TOP_MARGIN));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "sw800dp")
    public void testUpdateScrimVisualState_transparentScrim() {
        mScrimHandler.updateScrimVisualState();
        assertEquals(
                Color.TRANSPARENT,
                mScrimHandler
                        .getScrimModelForTesting()
                        .get(ScrimProperties.BACKGROUND_COLOR)
                        .intValue());
    }

    @Test
    public void testUpdateScrimVisualState_bottomToolbar_stopsAtToolbarTop() {
        when(mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_TOOLBAR)).thenReturn(true);
        when(mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_TOOLBAR))
                .thenReturn(HEIGHT_FROM_BOTTOM_TOOLBAR_TO_BOTTOM);

        mScrimHandler.updateScrimVisualState();
        assertEquals(
                HEIGHT_FROM_BOTTOM_TOOLBAR_TO_BOTTOM,
                mScrimHandler.getScrimModelForTesting().get(ScrimProperties.BOTTOM_MARGIN));
    }

    @Test
    public void testUpdateScrimVisualState_topToolbar_clearsBottomMargin() {
        when(mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_TOOLBAR))
                .thenReturn(true, false);
        when(mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_TOOLBAR))
                .thenReturn(HEIGHT_FROM_BOTTOM_TOOLBAR_TO_BOTTOM);
        mScrimHandler.updateScrimVisualState();

        // Toolbar moved to the top; the scrim should no longer be cropped.
        mScrimHandler.updateScrimVisualState();
        assertEquals(0, mScrimHandler.getScrimModelForTesting().get(ScrimProperties.BOTTOM_MARGIN));
    }

    @Test
    @Config(qualifiers = "sw800dp")
    public void testUpdateScrimVisualState_transparentScrim_Desktop() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        mScrimHandler.updateScrimVisualState();
        assertEquals(
                Color.TRANSPARENT,
                mScrimHandler
                        .getScrimModelForTesting()
                        .get(ScrimProperties.BACKGROUND_COLOR)
                        .intValue());
    }

    @Test
    public void testSetUseOpaqueScrim() {
        mScrimHandler.setUseOpaqueScrim(true);
        int color =
                mScrimHandler
                        .getScrimModelForTesting()
                        .get(ScrimProperties.BACKGROUND_COLOR)
                        .intValue();
        assertEquals(
                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                        ContextUtils.getApplicationContext(), BrandedColorScheme.APP_DEFAULT),
                color);
        assertEquals(255, Color.alpha(color));

        mScrimHandler.setUseOpaqueScrim(false);
        assertNotEquals(
                color,
                (Object)
                        mScrimHandler
                                .getScrimModelForTesting()
                                .get(ScrimProperties.BACKGROUND_COLOR));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "sw800dp")
    public void testSetUseOpaqueScrim_overridesTransparentScrim() {
        mScrimHandler.setUseOpaqueScrim(true);
        int color =
                mScrimHandler
                        .getScrimModelForTesting()
                        .get(ScrimProperties.BACKGROUND_COLOR)
                        .intValue();
        assertEquals(255, Color.alpha(color));
    }
}
