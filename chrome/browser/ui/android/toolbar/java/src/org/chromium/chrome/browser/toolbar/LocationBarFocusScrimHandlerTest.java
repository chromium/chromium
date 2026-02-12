// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for LocationBarFocusScrimHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationBarFocusScrimHandlerTest {
    private static final int BOTTOM_CHIN_HEIGHT = 37;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View mScrimTarget;
    @Mock private Runnable mClickDelegate;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Configuration mConfiguration;
    @Mock private ScrimManager mScrimManager;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Captor private ArgumentCaptor<PropertyModel> mScrimModelCaptor;

    LocationBarFocusScrimHandler mScrimHandler;
    private final SettableNonNullObservableSupplier<Integer> mTabStripHeightSupplier =
            ObservableSuppliers.createNonNull(0);

    @Before
    public void setUp() {
        lenient()
                .doReturn(BOTTOM_CHIN_HEIGHT)
                .when(mBottomControlsStacker)
                .getHeightFromLayerToBottom(LayerType.BOTTOM_CHIN);

        doReturn(mResources).when(mContext).getResources();
        doReturn(mConfiguration).when(mResources).getConfiguration();
        mScrimHandler =
                new LocationBarFocusScrimHandler(
                        mScrimManager,
                        (visible) -> {},
                        mContext,
                        mLocationBarDataProvider,
                        mClickDelegate,
                        mScrimTarget,
                        mTabStripHeightSupplier,
                        mBottomControlsStacker);
    }

    @Test
    public void testScrimShown_thenHidden() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(false).when(mNewTabPageDelegate).isLocationBarShown();
        mScrimHandler.onUrlFocusChange(true);
        assertEquals(
                BOTTOM_CHIN_HEIGHT,
                mScrimHandler.getScrimModelForTesting().get(ScrimProperties.BOTTOM_MARGIN));

        verify(mScrimManager).showScrim(any());

        mScrimHandler.onUrlFocusChange(false);
        verify(mScrimManager).hideScrim(any(), eq(true));

        // A second de-focus shouldn't trigger another hide.
        mScrimHandler.onUrlFocusChange(false);
        verify(mScrimManager, times(1)).hideScrim(any(), eq(true));
    }

    @Test
    public void testScrimShown_afterAnimation() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isLocationBarShown();
        mScrimHandler.onUrlFocusChange(true);

        verify(mScrimManager, never()).showScrim(any());

        mScrimHandler.onUrlAnimationFinished(true);
        verify(mScrimManager).showScrim(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testScrimNotShown_omniboxAutofocusOnIncognitoNtp() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isIncognitoNewTabPageCurrentlyVisible();
        mScrimHandler.onUrlFocusChange(true);
        verify(mScrimManager, never()).showScrim(any());
    }

    @Test
    public void testTabStripHeightChangeCallback() {
        int newHeight = 10;
        doReturn(newHeight).when(mResources).getDimensionPixelSize(R.dimen.tab_strip_height);
        mTabStripHeightSupplier.set(newHeight);
        assertEquals(
                "Scrim top margin should be updated when tab strip height changes.",
                newHeight,
                mScrimHandler.getScrimModelForTesting().get(ScrimProperties.TOP_MARGIN));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_MULTIMODAL_INPUT)
    @Config(qualifiers = "sw800dp")
    public void testTransparentScrim() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(false).when(mNewTabPageDelegate).isLocationBarShown();
        mScrimHandler.onUrlFocusChange(true);
        verify(mScrimManager).showScrim(mScrimModelCaptor.capture());
        assertEquals(
                Color.TRANSPARENT,
                (int) mScrimModelCaptor.getValue().get(ScrimProperties.BACKGROUND_COLOR));
    }
}
