// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
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

/** Unit tests for {@link PageZoomManager}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING})
public class PageZoomManagerUnitTest {
    // Error messages
    private static final String CURRENT_ZOOM_FAILURE =
            "Failure in fetching the current zoom factor for the model or web contents.";
    private static final String DECREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in decrease zoom method. Expected 1 JNI call but none occurred.";
    private static final String INCREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in increase zoom method. Expected 1 JNI call but none occurred.";

    @Mock private HostZoomMapImpl.Natives mHostZoomMapMock;
    @Mock private ContentFeatureMap.Natives mContentFeatureListMapMock;
    @Mock private PageZoomManagerDelegate mPageZoomManagerDelegateMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private BrowserContextHandle mBrowserContextHandleMock;

    private PropertyModel mModel;
    private PageZoomManager mManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapMock);
        ContentFeatureMapJni.setInstanceForTesting(mContentFeatureListMapMock);

        when(mPageZoomManagerDelegateMock.getWebContents()).thenReturn(mWebContentsMock);
        when(mPageZoomManagerDelegateMock.getBrowserContextHandle())
                .thenReturn(mBrowserContextHandleMock);

        mManager = new PageZoomManager(mPageZoomManagerDelegateMock);
    }

    @Test
    public void testDecreaseZoom() {
        // Verify that calling decrease zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mManager.decrementZoomLevel();
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.56, 1.56);
    }

    @Test
    public void testDecreaseZoom_SmallConfiguration() {
        // Verify that calling decrease zoom method sends expected value to native code.
        HostZoomMap.setSystemFontScaleForTesting(0.85f);
        HostZoomMap.setShouldAdjustForOSLevelForTesting(true);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mManager.decrementZoomLevel();
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 2.18);
    }

    @Test
    public void testIncreaseZoom() {
        // Verify that calling increase zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mManager.incrementZoomLevel();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 3.07);
    }

    @Test
    public void testIncreaseZoom_LargeConfiguration() {
        // Verify that calling increase zoom method sends expected value to native code.
        HostZoomMap.setSystemFontScaleForTesting(1.3f);
        HostZoomMap.setShouldAdjustForOSLevelForTesting(true);
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mManager.incrementZoomLevel();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.22, 2.66);
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

        // A single increase at this point should display the next closest zoom level of 150% (zoom
        // factor ~ 2.22) with respect to 137%, that is equivalent to a rendered value of 150*2.2
        // ~ 330% (zoom factor ~ 6.54).
        mManager.incrementZoomLevel();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 2.22, 6.54);
    }
}
