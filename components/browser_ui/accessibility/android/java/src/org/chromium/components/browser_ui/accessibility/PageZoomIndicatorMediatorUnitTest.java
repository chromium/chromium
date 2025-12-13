// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomBarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomIndicatorMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageZoomManager mManager;

    private PropertyModel mModel;
    private PageZoomIndicatorMediator mMediator;

    @Before
    public void setUp() {
        mMediator = new PageZoomIndicatorMediator(mManager);
        mModel = mMediator.getModelForTesting();
    }

    @Test
    public void testPushProperties() {
        // Set up the manager mock to return specific values.
        when(mManager.getZoomLevel()).thenReturn(0.0); // Corresponds to 100%
        when(mManager.getDefaultZoomLevel()).thenReturn(0.0);

        mMediator.pushProperties();

        // Verify that the model is updated correctly.
        assertEquals(0.0, mModel.get(PageZoomProperties.DEFAULT_ZOOM_FACTOR), 0.0);
        assertEquals("100%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
        assertTrue(mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        assertTrue(mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
    }

    @Test
    public void testHandleDecreaseClicked() {
        when(mManager.decrementZoomLevel()).thenReturn(4); // Corresponds to 90%

        mMediator.handleDecreaseClicked();

        verify(mManager).decrementZoomLevel();
        assertEquals("90%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testHandleIncreaseClicked() {
        when(mManager.incrementZoomLevel()).thenReturn(6); // Corresponds to 110%

        mMediator.handleIncreaseClicked();

        verify(mManager).incrementZoomLevel();
        assertEquals("110%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testHandleResetClicked() {
        when(mManager.getDefaultZoomLevel()).thenReturn(0.0);
        when(mManager.getZoomLevel()).thenReturn(0.52);

        mMediator.pushProperties();

        assertEquals("110%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));

        mMediator.handleResetClicked();

        verify(mManager).setZoomLevel(0.0);
        assertEquals("100%", mModel.get(PageZoomProperties.ZOOM_PERCENT_TEXT));
    }

    @Test
    public void testButtonStates_AtMinimum() {
        when(mManager.getZoomLevel()).thenReturn(HostZoomMap.AVAILABLE_ZOOM_FACTORS[0]);
        when(mManager.getDefaultZoomLevel()).thenReturn(0.0);

        mMediator.pushProperties();

        assertFalse(mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        assertTrue(mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
    }

    @Test
    public void testButtonStates_AtMaximum() {
        double maxZoom =
                HostZoomMap.AVAILABLE_ZOOM_FACTORS[HostZoomMap.AVAILABLE_ZOOM_FACTORS.length - 1];
        when(mManager.getZoomLevel()).thenReturn(maxZoom);
        when(mManager.getDefaultZoomLevel()).thenReturn(0.0);

        mMediator.pushProperties();

        assertTrue(mModel.get(PageZoomProperties.DECREASE_ZOOM_ENABLED));
        assertFalse(mModel.get(PageZoomProperties.INCREASE_ZOOM_ENABLED));
    }
}
