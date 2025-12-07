// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomUtils}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomUtilsUnitTest {
    // Error messages
    private static final String BAR_VALUE_TO_ZOOM_FACTOR_FAILURE =
            "Failure to correctly convert bar value to zoom factor.";
    private static final String ZOOM_FACTOR_TO_BAR_VALUE_FAILURE =
            "Failure to correctly convert zoom factor to bar value.";
    private static final String BAR_VALUE_TO_ZOOM_LEVEL_FAILURE =
            "Failure to correctly convert bar value to zoom level.";
    private static final String SHOULD_SNAP_BAR_VALUE_TO_DEFAULT_ZOOM_FAILURE =
            "Failure to correctly return whether to snap bar value to default zoom.";

    private static final String GET_NEXT_INDEX_DECREASE_FAILURE =
            "Failure to get next index in decreasing direction.";
    private static final String GET_NEXT_INDEX_INCREASE_FAILURE =
            "Failure to get next index in increasing direction.";

    private static final String SET_DEFAULT_FAILURE_NO_JNI =
            "Failure in set default zoom by bar value. Expected 1 JNI call but none occurred.";
    private static final String GET_DEFAULT_FAILURE_NO_JNI =
            "Failure in get default zoom as bar value. Expected 1 JNI call but none occurred.";
    private static final String GET_DEFAULT_FAILURE = "Failure to get default zoom as bar value.";

    private static final String SHOULD_SHOW_ZOOM_MENU_ITEM_FAILURE_EXPECTED_FALSE =
            "Failure in should show zoom menu item method. Expected false but returned true.";

    @Mock private HostZoomMapImpl.Natives mHostZoomMapMock;

    @Mock private BrowserContextHandle mContextMock;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapMock);
    }

    @Test
    public void testConvertBarValueToZoomFactor() {
        // Cached zoom factor
        Assert.assertEquals(
                BAR_VALUE_TO_ZOOM_FACTOR_FAILURE,
                2.22,
                PageZoomUtils.convertBarValueToZoomFactor(100),
                0.0001);

        // Non-cached zoom factor
        Assert.assertEquals(
                BAR_VALUE_TO_ZOOM_FACTOR_FAILURE,
                2.26,
                PageZoomUtils.convertBarValueToZoomFactor(101),
                0.0001);
    }

    @Test
    public void testConvertZoomFactorToBarValue() {
        // Cached zoom factor
        Assert.assertEquals(
                ZOOM_FACTOR_TO_BAR_VALUE_FAILURE,
                100,
                PageZoomUtils.convertZoomFactorToBarValue(2.22));

        // Non-cached zoom factor
        Assert.assertEquals(
                ZOOM_FACTOR_TO_BAR_VALUE_FAILURE,
                101,
                PageZoomUtils.convertZoomFactorToBarValue(2.26));
    }

    @Test
    public void testConvertBarValueToZoomLevel() {
        // Cached zoom level
        Assert.assertEquals(
                BAR_VALUE_TO_ZOOM_LEVEL_FAILURE,
                1.5,
                PageZoomUtils.convertBarValueToZoomLevel(100),
                0.0001);

        // Non-cached zoom level
        Assert.assertEquals(
                BAR_VALUE_TO_ZOOM_FACTOR_FAILURE,
                1.51,
                PageZoomUtils.convertBarValueToZoomLevel(101),
                0.0001);
    }

    @Test
    public void testShouldSnapBarValueToDefaultZoom() {
        Assert.assertTrue(
                SHOULD_SNAP_BAR_VALUE_TO_DEFAULT_ZOOM_FAILURE,
                PageZoomUtils.shouldSnapBarValueToDefaultZoom(47, 0.0));

        Assert.assertFalse(
                SHOULD_SNAP_BAR_VALUE_TO_DEFAULT_ZOOM_FAILURE,
                PageZoomUtils.shouldSnapBarValueToDefaultZoom(45, 0.0));
    }

    @Test
    public void testSetDefaultZoomByBarValue() {
        PageZoomUtils.setDefaultZoomByBarValue(mContextMock, 110);
        verify(mHostZoomMapMock, times(1).description(SET_DEFAULT_FAILURE_NO_JNI))
                .setDefaultZoomLevel(mContextMock, 2.58);
    }

    @Test
    public void testGetDefaultZoomByBarValue() {
        when(mHostZoomMapMock.getDefaultZoomLevel(mContextMock)).thenReturn(2.58);
        Assert.assertEquals(
                GET_DEFAULT_FAILURE,
                110,
                PageZoomUtils.getDefaultZoomAsBarValue(mContextMock),
                0.0001);
        verify(mHostZoomMapMock, times(1).description(GET_DEFAULT_FAILURE_NO_JNI))
                .getDefaultZoomLevel(mContextMock);
    }

    @Test
    public void testShouldAlwaysShowZoomMenuItem_defaultIsFalse() {
        Assert.assertEquals(
                SHOULD_SHOW_ZOOM_MENU_ITEM_FAILURE_EXPECTED_FALSE,
                false,
                PageZoomUtils.shouldAlwaysShowZoomMenuItem());
    }

    @Test
    public void testGetNextIndexIncrease() {
        Assert.assertEquals(
                GET_NEXT_INDEX_INCREASE_FAILURE,
                7,
                PageZoomUtils.getNextIndex(false, 1.00),
                0.0001);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetNextIndexIncreaseMax() {
        PageZoomUtils.getNextIndex(false, 6.03);
    }

    @Test
    public void testGetNextIndexDecrease() {
        Assert.assertEquals(
                GET_NEXT_INDEX_DECREASE_FAILURE, 6, PageZoomUtils.getNextIndex(true, 1.00), 0.0001);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetNextIndexDecreaseMin() {
        PageZoomUtils.getNextIndex(true, -3.80);
    }
}
