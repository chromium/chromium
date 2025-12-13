// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.ContentFeatureMapJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomMenuItemMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ContentFeatures.ANDROID_DESKTOP_ZOOM_SCALING})
public class PageZoomMenuItemMediatorUnitTest {
    // Error messages
    private static final String ZOOM_TEXT_FAILURE =
            "Failure in fetching the current zoom text for the model.";
    private static final String DECREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in decrease zoom method. Expected 1 JNI call but none occurred.";
    private static final String INCREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in increase zoom method. Expected 1 JNI call but none occurred.";
    private static final String DECREASE_ENABLED_FAILURE =
            "Failure to enable decrease button when zoom increased from minimum.";
    private static final String INCREASE_ENABLED_FAILURE =
            "Failure to enable increase button when zoom decreased from maximum.";
    private static final String DECREASE_DISABLED_FAILURE =
            "Failure to disable decrease button when zoom reached minimum.";
    private static final String INCREASE_DISABLED_FAILURE =
            "Failure to disable increase button when zoom reached maximum.";

    @Mock private HostZoomMapImpl.Natives mHostZoomMapMock;
    @Mock private ContentFeatureMap.Natives mContentFeatureListMapMock;
    @Mock private PageZoomManagerDelegate mPageZoomManagerDelegateMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private BrowserContextHandle mBrowserContextHandleMock;

    private PropertyModel mModel;
    private PageZoomManager mManager;
    private PageZoomMenuItemMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        HostZoomMapImplJni.setInstanceForTesting(mHostZoomMapMock);
        ContentFeatureMapJni.setInstanceForTesting(mContentFeatureListMapMock);

        when(mPageZoomManagerDelegateMock.getWebContents()).thenReturn(mWebContentsMock);
        when(mPageZoomManagerDelegateMock.getBrowserContextHandle())
                .thenReturn(mBrowserContextHandleMock);

        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS_FOR_MENU_ITEM).build();
        mManager = new PageZoomManager(mPageZoomManagerDelegateMock);
        mMediator = new PageZoomMenuItemMediator(mModel, mManager);
    }

    @Test
    public void testDecreaseZoom() {
        // Verify that calling decrease zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "150%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
        mMediator.handleDecreaseClicked();
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.56, 1.56);
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "133%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testIncreaseZoom() {
        // Verify that calling increase zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.pushProperties();
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "150%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
        mMediator.handleIncreaseClicked();
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07, 3.07);
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "175%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testDecreaseZoomEnabledChange() {
        // Verify that when already at the minimum zoom, the decrease button is disabled.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(-3.8);
        mMediator.pushProperties();
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "50%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
        Assert.assertFalse(
                DECREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from minimum, the decrease button is re-enabled.
        mMediator.handleIncreaseClicked();
        Assert.assertTrue(
                DECREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "67%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testIncreaseZoomEnabledChange() {
        // Verify that when already at the maximum zoom, the increase button is disabled
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(6.03);
        mMediator.pushProperties();
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "300%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
        Assert.assertFalse(
                INCREASE_DISABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));

        // Verify that when zoom level changes from maximum, the increase button is re-enabled.
        mMediator.handleDecreaseClicked();
        Assert.assertTrue(
                INCREASE_ENABLED_FAILURE, mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
        Assert.assertEquals(
                ZOOM_TEXT_FAILURE, "250%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }
}
