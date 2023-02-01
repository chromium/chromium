// Copyright 2022 The Chromium Authors
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomMediatorUnitTest {
    // Error messages
    private static final String CURRENT_ZOOM_FAILURE =
            "Failure in fetching the current zoom factor for the model or web contents.";
    private static final String DECREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in decrease zoom method. Expected 1 JNI call but none occurred.";
    private static final String INCREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in increase zoom method. Expected 1 JNI call but none occurred.";
    private static final String DECREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in decrease zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String INCREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in increase zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String DECREASE_ENABLED_FAILURE =
            "Failure to enable decrease button when seekbar value increased from minimum.";
    private static final String INCREASE_ENABLED_FAILURE =
            "Failure to enable increase button when seekbar value decreased from maximum.";
    private static final String DECREASE_DISABLED_FAILURE =
            "Failure to disable decrease button when seekbar value reached minimum.";
    private static final String INCREASE_DISABLED_FAILURE =
            "Failure to disable increase button when seekbar value reached maximum.";
    private static final String SEEKBAR_VALUE_FAILURE =
            "Failure to update zoom factor or seekbar level when value seekbar value changed.";
    private static final String SEEKBAR_VALUE_FAILURE_NO_JNI =
            "Failure in seekbar value method. Expected 1 JNI call but none occurred.";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebContents mWebContentsMock;

    @Mock
    private HostZoomMapImpl.Natives mHostZoomMapMock;

    private PropertyModel mModel;
    private PageZoomMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(HostZoomMapImplJni.TEST_HOOKS, mHostZoomMapMock);

        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS).build();
        mMediator = new PageZoomMediator(mModel);

        HostZoomMap.SYSTEM_FONT_SCALE = 1.0f;
        when(mHostZoomMapMock.getDesktopSiteZoomScale(mWebContentsMock)).thenReturn(1.0);
    }

    @Test
    public void testDecreaseZoom() {
        // Verify that calling decrease zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleDecreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.56, 1.56);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 83, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testDecreaseZoom_SmallConfiguration() {
        // Verify that calling decrease zoom method sends expected value to native code.
        HostZoomMap.SYSTEM_FONT_SCALE = 0.85f;
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 126, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleDecreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 2.18);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 125, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testDecreaseZoom_SmallConfiguration_DesktopUserAgent() {
        // Verify that calling decrease zoom method sends expected value to native code.
        HostZoomMap.SYSTEM_FONT_SCALE = 0.85f;
        when(mHostZoomMapMock.getDesktopSiteZoomScale(mWebContentsMock)).thenReturn(1.1);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 110, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleDecreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 2.22, 1.85);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testIncreaseZoom() {
        // Verify that calling increase zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 3.07);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 125, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration() {
        // Verify that calling increase zoom method sends expected value to native code.
        HostZoomMap.SYSTEM_FONT_SCALE = 1.3f;
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 65, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.22, 2.66);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 75, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration_DesktopUserAgent() {
        // Verify that calling increase zoom method sends expected value to native code.
        HostZoomMap.SYSTEM_FONT_SCALE = 1.3f;
        when(mHostZoomMapMock.getDesktopSiteZoomScale(mWebContentsMock)).thenReturn(1.1);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 55, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 0.52, 2.48);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 60, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration_DesktopUserAgent_RenderOverMaxDisplayValue() {
        // Verify that calling increase zoom method sends expected value to native code when the OS
        // font scale and desktop site zoom scale in use could cause the rendered zoom factor value
        // to overflow beyond the maximum displayed zoom factor value.
        HostZoomMap.SYSTEM_FONT_SCALE = 2.0f;
        when(mHostZoomMapMock.getDesktopSiteZoomScale(mWebContentsMock)).thenReturn(1.1);
        // Assume that the currently rendered zoom value is maximum, that is 300% or a zoom factor
        // of 6.03. For the zoom scales defined above, this will be equivalent to a display value of
        // 300/2/1.1 ~ 137%.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(6.03);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 87, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));

        // A single increase at this point should display the next closest zoom level of 150% (zoom
        // factor ~ 2.22) with respect to 137%, that is equivalent to a rendered value of 150*2*1.1
        // ~ 330% (zoom factor ~ 6.54).
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 2.22, 6.54);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testSeekBarValueChanged() {
        // Verify the calling seekbar value change method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 100, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleSeekBarValueChanged(108);
        verify(mHostZoomMapMock, times(1).description(SEEKBAR_VALUE_FAILURE_NO_JNI))
                .setZoomLevel(eq(mWebContentsMock),
                        ArgumentMatchers.doubleThat(argument -> Math.abs(2.51 - argument) <= 0.001),
                        ArgumentMatchers.doubleThat(
                                argument -> Math.abs(2.51 - argument) <= 0.001));
        Assert.assertEquals(
                SEEKBAR_VALUE_FAILURE, 108, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testDecreaseZoomEnabledChange() {
        // Verify that when already at the minimum zoom, the decrease button is disabled.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(-3.8);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 0, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        Assert.assertFalse(
                DECREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from minimum, the decrease button is reenabled.
        mMediator.handleIncreaseClicked(null);
        Assert.assertTrue(
                DECREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
    }

    @Test
    public void testIncreaseZoomEnabledChange() {
        // Verify that when already at the maximum zoom, the increase button is disabled
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(6.03);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 250, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        Assert.assertFalse(
                INCREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from maximum, the increase button is reenabled.
        mMediator.handleDecreaseClicked(null);
        Assert.assertTrue(
                INCREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
    }
}