// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;

/** Unit tests for LocationBarFocusScrimHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocationBarFocusScrimHandlerTest {
    @Mock private View mScrimTarget;
    @Mock private Runnable mClickDelegate;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Configuration mConfiguration;
    @Mock private ScrimCoordinator mScrimCoordinator;
    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private ObservableSupplier<Integer> mTabStripHeightSupplier;

    LocationBarFocusScrimHandler mScrimHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();
        doReturn(mConfiguration).when(mResources).getConfiguration();
        mScrimHandler =
                new LocationBarFocusScrimHandler(
                        mScrimCoordinator,
                        (visible) -> {},
                        mContext,
                        mLocationBarDataProvider,
                        mClickDelegate,
                        mScrimTarget,
                        mTabStripHeightSupplier);
    }

    @Test
    public void testScrimShown_thenHidden() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(false).when(mNewTabPageDelegate).isLocationBarShown();
        mScrimHandler.onUrlFocusChange(true);

        verify(mScrimCoordinator).showScrim(any());

        mScrimHandler.onUrlFocusChange(false);
        verify(mScrimCoordinator).hideScrim(true);

        // A second de-focus shouldn't trigger another hide.
        mScrimHandler.onUrlFocusChange(false);
        verify(mScrimCoordinator, times(1)).hideScrim(true);
    }

    @Test
    public void testScrimShown_afterAnimation() {
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isLocationBarShown();
        mScrimHandler.onUrlFocusChange(true);

        verify(mScrimCoordinator, never()).showScrim(any());

        mScrimHandler.onUrlAnimationFinished(true);
        verify(mScrimCoordinator).showScrim(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testTabStripHeightChangeCallback() {
        ArgumentCaptor<Callback<Integer>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mTabStripHeightSupplier).addObserver(captor.capture());
        Callback<Integer> tabStripHeightChangeCallback = captor.getValue();
        int newTabStripHeight =
                mContext.getResources()
                        .getDimensionPixelSize(org.chromium.chrome.R.dimen.tab_strip_height);
        tabStripHeightChangeCallback.onResult(newTabStripHeight);
        assertEquals(
                "Scrim top margin should be updated when tab strip height changes.",
                newTabStripHeight,
                mScrimHandler.getScrimModelForTesting().get(ScrimProperties.TOP_MARGIN));
    }
}
