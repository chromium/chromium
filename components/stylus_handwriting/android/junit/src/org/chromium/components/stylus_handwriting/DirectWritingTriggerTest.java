// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;
import android.support.annotation.RequiresApi;
import android.view.MotionEvent;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.stylus_handwriting.test_support.ShadowDirectWritingSettingsHelper;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link DirectWritingTrigger}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowDirectWritingSettingsHelper.class})
public class DirectWritingTriggerTest {
    @Mock
    private WebContents mWebContents;
    @Mock
    private DirectWritingServiceBinder mDwServiceBinder;
    @Mock
    private StylusWritingImeCallback mStylusWritingImeCallback;
    @Mock
    private ViewGroup mContainerView;
    @Mock
    private DirectWritingServiceCallback mDwServiceCallback;

    private Context mContext;
    private DirectWritingTrigger mDwTrigger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mDwTrigger = spy(new DirectWritingTrigger());
        mDwTrigger.setServiceBinderForTest(mDwServiceBinder);
        doReturn(mContainerView).when(mStylusWritingImeCallback).getContainerView();
        mContext = RuntimeEnvironment.application;
        doReturn(mContext).when(mContainerView).getContext();

        // DirectWritingTrigger class comes into action only when Setting is enabled.
        ShadowDirectWritingSettingsHelper.setEnabled(true);
    }

    @After
    public void tearDown() {
        // Reset shadow settings.
        ShadowDirectWritingSettingsHelper.setEnabled(false);
    }

    private MotionEvent getMockMotionEvent(int toolType, int action) {
        MotionEvent mockEvent = mock(MotionEvent.class);
        doReturn(toolType).when(mockEvent).getToolType(0);
        doReturn(action).when(mockEvent).getAction();
        return mockEvent;
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnWebContentsChanged() {
        // Test that settings are updated and callback is created if null, when WebContents is set.
        assertNull(mDwTrigger.getServiceCallback());
        mDwTrigger.onWebContentsChanged(mContext, mWebContents);
        verify(mDwTrigger).updateDWSettings(mContext);
        assertNotNull(mDwTrigger.getServiceCallback());
        verify(mWebContents).setStylusWritingHandler(mDwTrigger);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testServiceCallbackCreation() {
        // Test that callback is created when settings are updated.
        assertNull(mDwTrigger.getServiceCallback());
        mDwTrigger.updateDWSettings(mContext);
        assertNotNull(mDwTrigger.getServiceCallback());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testGetStylusWritingCursorHandler() {
        assertEquals(mDwTrigger, mDwTrigger.getStylusWritingCursorHandler());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testCanShowSoftKeyboard() {
        assertFalse(mDwTrigger.canShowSoftKeyboard());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnFocusChanged_lostFocus() {
        ShadowDirectWritingSettingsHelper.setEnabled(false);
        mDwTrigger.updateDWSettings(mContext);

        mDwTrigger.onFocusChanged(false);
        // hide toolbar is not called when feature is disabled.
        verify(mDwServiceBinder, never()).hideDWToolbar();
        // stop recognition is not called until StylusWritingImeCallback is set.
        verify(mDwServiceBinder, never()).onStopRecognition(any(), any(), any());

        ShadowDirectWritingSettingsHelper.setEnabled(true);
        mDwTrigger.updateDWSettings(mContext);
        mDwTrigger.onFocusChanged(false);
        verify(mDwServiceBinder).hideDWToolbar();
        // stop recognition is not called until StylusWritingImeCallback is set.
        verify(mDwServiceBinder, never()).onStopRecognition(any(), any(), any());

        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        // Set Ime callback via requestStartStylusWriting.
        assertTrue(mDwTrigger.requestStartStylusWriting(mStylusWritingImeCallback));
        mDwTrigger.onFocusChanged(false);
        verify(mDwServiceBinder).onStopRecognition(null, null, mContainerView);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnFocusChanged_gainFocus() {
        mDwTrigger.updateDWSettings(mContext);
        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        // No action when focus is gained.
        mDwTrigger.onFocusChanged(true);
        verify(mDwServiceBinder, never()).hideDWToolbar();
        verify(mDwServiceBinder, never()).onStopRecognition(any(), any(), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testRequestStartStylusWriting() {
        mDwTrigger.updateDWSettings(mContext);
        // requestStartStylusWriting returns false until service is connected.
        assertFalse(mDwTrigger.requestStartStylusWriting(mStylusWritingImeCallback));
        assertFalse(mDwTrigger.stylusWritingDetected());

        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        mDwTrigger.setServiceCallbackForTest(mDwServiceCallback);
        assertTrue(mDwTrigger.requestStartStylusWriting(mStylusWritingImeCallback));
        verify(mDwServiceCallback).setImeCallback(mStylusWritingImeCallback);
        assertTrue(mDwTrigger.stylusWritingDetected());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnDetachedFromWindow() {
        // Unbind service is not called until service is connected, and settings is updated.
        mDwTrigger.onDetachedFromWindow(mContext);
        verify(mDwServiceBinder, never()).unbindService(any());

        mDwTrigger.updateDWSettings(mContext);
        mDwTrigger.onDetachedFromWindow(mContext);
        verify(mDwServiceBinder, never()).unbindService(any());

        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        mDwTrigger.onDetachedFromWindow(mContext);
        verify(mDwServiceBinder).unbindService(mContext);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_bindServiceWithToolTypeStylus() {
        // Service is not bound until dw setting is updated as enabled, on Hover enter.
        MotionEvent mockHoverEnterEvent =
                getMockMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(mockHoverEnterEvent, mContainerView);
        verify(mockHoverEnterEvent, never()).getToolType(0);
        verify(mDwServiceBinder, never()).bindService(any(), any());

        mDwTrigger.updateDWSettings(mContext);
        // Service is bound only for Hover enter and not hover move.
        MotionEvent mockMoveEvent =
                getMockMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_MOVE);
        mDwTrigger.handleHoverEvent(mockMoveEvent, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());

        mDwTrigger.handleHoverEvent(mockHoverEnterEvent, mContainerView);
        verify(mDwServiceBinder).bindService(eq(mContext), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_serviceAlreadyConnected() {
        MotionEvent mockEvent =
                getMockMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_ENTER);
        // Service is not bound if it is already connected.
        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        mDwTrigger.handleHoverEvent(mockEvent, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_bindServiceWithToolTypeEraser() {
        mDwTrigger.updateDWSettings(mContext);
        MotionEvent mockEvent =
                getMockMotionEvent(MotionEvent.TOOL_TYPE_ERASER, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(mockEvent, mContainerView);
        verify(mDwServiceBinder).bindService(eq(mContext), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_serviceNotConnectedForNonStylusEvent() {
        mDwTrigger.updateDWSettings(mContext);
        MotionEvent mockMouseMoveEvent =
                getMockMotionEvent(MotionEvent.TOOL_TYPE_MOUSE, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(mockMouseMoveEvent, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnWindowFocusChanged_gainFocus() {
        // Test behaviour when window gains focus with DW setting disabled.
        ShadowDirectWritingSettingsHelper.setEnabled(false);
        mDwTrigger.onWindowFocusChanged(mContext, true);
        verify(mDwTrigger).updateDWSettings(mContext);
        verify(mDwServiceBinder, never()).hideDWToolbar();
        verify(mDwServiceBinder, never()).onWindowFocusChanged(any(), anyBoolean());

        ShadowDirectWritingSettingsHelper.setEnabled(true);
        mDwTrigger.onWindowFocusChanged(mContext, true);
        verify(mDwServiceBinder, never()).hideDWToolbar();
        verify(mDwServiceBinder).onWindowFocusChanged(mContext, true);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnWindowFocusChanged_lostFocus() {
        // Test behaviour when window loses focus with DW setting disabled.
        ShadowDirectWritingSettingsHelper.setEnabled(false);
        mDwTrigger.onWindowFocusChanged(mContext, false);
        verify(mDwTrigger, never()).updateDWSettings(any());
        verify(mDwServiceBinder, never()).hideDWToolbar();
        verify(mDwServiceBinder, never()).onWindowFocusChanged(any(), anyBoolean());

        // Test behaviour when window loses focus with DW setting already enabled.
        ShadowDirectWritingSettingsHelper.setEnabled(true);
        mDwTrigger.updateDWSettings(mContext);

        mDwTrigger.onWindowFocusChanged(mContext, false);
        // Verify that updateDWSettings is not called again.
        verify(mDwTrigger, times(1)).updateDWSettings(mContext);
        verify(mDwServiceBinder).hideDWToolbar();
        verify(mDwServiceBinder).onWindowFocusChanged(mContext, false);
    }
}
