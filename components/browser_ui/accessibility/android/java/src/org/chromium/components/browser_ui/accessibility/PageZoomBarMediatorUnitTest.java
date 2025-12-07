// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.ContentFeatureMapJni;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING})
public class PageZoomBarMediatorUnitTest {
    // Error messages
    private static final String CURRENT_ZOOM_FAILURE =
            "Failure in fetching the current zoom factor for the model or web contents.";
    private static final String DECREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in decrease zoom method. Expected 1 JNI call but none occurred.";
    private static final String INCREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in increase zoom method. Expected 1 JNI call but none occurred.";
    private static final String RESET_ZOOM_FAILURE_NO_JNI =
            "Failure in reset zoom method. Expected 1 JNI call but none occurred.";
    private static final String DECREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in decrease zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String INCREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in increase zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String DECREASE_ENABLED_FAILURE =
            "Failure to enable decrease button when bar value increased from minimum.";
    private static final String INCREASE_ENABLED_FAILURE =
            "Failure to enable increase button when bar value decreased from maximum.";
    private static final String DECREASE_DISABLED_FAILURE =
            "Failure to disable decrease button when bar value reached minimum.";
    private static final String INCREASE_DISABLED_FAILURE =
            "Failure to disable increase button when bar value reached maximum.";
    private static final String BAR_VALUE_FAILURE =
            "Failure to update zoom factor or bar level when value bar value changed.";
    private static final String BAR_VALUE_FAILURE_NO_JNI =
            "Failure in bar value method. Expected 1 JNI call but none occurred.";
    private static final String RESET_ZOOM_FAILURE = "Failure to reset to the default zoom level.";

    @Mock private HostZoomMapImpl.Natives mHostZoomMapMock;
    @Mock private ContentFeatureMap.Natives mContentFeatureListMapMock;
    @Mock private PageZoomManagerDelegate mPageZoomManagerDelegateMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private BrowserContextHandle mBrowserContextHandleMock;

    private PropertyModel mModel;
    private PageZoomManager mManager;
    private PageZoomBarMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapMock);
        ContentFeatureMapJni.setInstanceForTesting(mContentFeatureListMapMock);

        when(mPageZoomManagerDelegateMock.getWebContents()).thenReturn(mWebContentsMock);
        when(mPageZoomManagerDelegateMock.getBrowserContextHandle())
                .thenReturn(mBrowserContextHandleMock);

        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS).build();
        mModel.set(PageZoomProperties.DEFAULT_ZOOM_FACTOR, 0.0);
        mManager = new PageZoomManager(mPageZoomManagerDelegateMock);
        mMediator = new PageZoomBarMediator(mModel, mManager, (Void unused) -> {});
    }

    @Test
    public void testDecreaseZoom() {
        // Verify that calling decrease zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleDecreaseClicked();
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.56, 1.56);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 83, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testDecreaseZoom_SmallConfiguration() {
        // Verify that calling decrease zoom method sends expected value to native code.
        HostZoomMap.setSystemFontScaleForTesting(0.85f);
        HostZoomMap.setShouldAdjustForOSLevelForTesting(true);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 126, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleDecreaseClicked();
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 2.18);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 125, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testIncreaseZoom() {
        // Verify that calling increase zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleIncreaseClicked();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 3.07);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 125, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration() {
        // Verify that calling increase zoom method sends expected value to native code.
        HostZoomMap.setSystemFontScaleForTesting(1.3f);
        HostZoomMap.setShouldAdjustForOSLevelForTesting(true);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 65, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleIncreaseClicked();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.22, 2.66);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 75, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration_RenderOverMaxDisplayValue() {
        // Verify that calling increase zoom method sends expected value to native code when the OS
        // font scale in use could cause the rendered zoom factor value to overflow beyond the
        // maximum displayed zoom factor value.
        HostZoomMap.setSystemFontScaleForTesting(2.2f);
        HostZoomMap.setShouldAdjustForOSLevelForTesting(true);

        // Assume that the currently rendered zoom value is maximum, that is 300% or a zoom factor
        // of 6.03. For the zoom scales defined above, this will be equivalent to a display value of
        // 300/2.2 ~ 137%.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(6.03);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 87, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));

        // A single increase at this point should display the next closest zoom level of 150% (zoom
        // factor ~ 2.22) with respect to 137%, that is equivalent to a rendered value of 150*2.2
        // ~ 330% (zoom factor ~ 6.54).
        mMediator.handleIncreaseClicked();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 2.22, 6.54);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testBarValueChanged() {
        // Verify the calling bar value change method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleBarValueChanged(108);
        verify(mHostZoomMapMock, times(1).description(BAR_VALUE_FAILURE_NO_JNI))
                .setZoomLevel(
                        eq(mWebContentsMock),
                        ArgumentMatchers.doubleThat(argument -> Math.abs(2.51 - argument) <= 0.001),
                        ArgumentMatchers.doubleThat(
                                argument -> Math.abs(2.51 - argument) <= 0.001));
        Assert.assertEquals(
                BAR_VALUE_FAILURE, 108, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }

    @Test
    public void testDecreaseZoomEnabledChange() {
        // Verify that when already at the minimum zoom, the decrease button is disabled.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(-3.8);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 0, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        Assert.assertFalse(
                DECREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from minimum, the decrease button is reenabled.
        mMediator.handleIncreaseClicked();
        Assert.assertTrue(
                DECREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
    }

    @Test
    public void testIncreaseZoomEnabledChange() {
        // Verify that when already at the maximum zoom, the increase button is disabled
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(6.03);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 250, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        Assert.assertFalse(
                INCREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from maximum, the increase button is reenabled.
        mMediator.handleDecreaseClicked();
        Assert.assertTrue(
                INCREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
    }

    @Test
    public void testResetZoom() {
        // The decimal value in the when() statements refer to the zoom factor, while the integer
        // value in the assert() statements refers to the bar value listed in the table in
        // PageZoomUtils.java
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(0.0);
        mMediator.pushProperties();
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 50, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
        mMediator.handleResetClicked();
        // Ensure that setZoomLevel method is called with the correct params (0.0, 0.0)
        verify(mHostZoomMapMock, times(1).description(RESET_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 0.0, 0.0);
        // Ensure that zoom stays the same if already at default
        Assert.assertEquals(
                RESET_ZOOM_FAILURE, 50, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));

        // Increase the zoom and ensure that zoom returns to default
        mMediator.handleIncreaseClicked();
        mMediator.handleResetClicked();
        // Check that number of times setZoomLevel(0,0) is called has increased by 1
        verify(mHostZoomMapMock, times(2).description(RESET_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 0.0, 0.0);
        Assert.assertEquals(
                RESET_ZOOM_FAILURE, 50, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));

        // Decrease the zoom and ensure that zoom returns to default
        mMediator.handleDecreaseClicked();
        mMediator.handleResetClicked();
        verify(mHostZoomMapMock, times(3).description(RESET_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 0.0, 0.0);
        Assert.assertEquals(
                RESET_ZOOM_FAILURE, 50, mModel.get(PageZoomProperties.CURRENT_BAR_VALUE));
    }
}
