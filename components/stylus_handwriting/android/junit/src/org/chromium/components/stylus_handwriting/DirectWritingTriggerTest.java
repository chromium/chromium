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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.RequiresApi;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.stylus_handwriting.test_support.ShadowDirectWritingSettingsHelper;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link DirectWritingTrigger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowDirectWritingSettingsHelper.class})
public class DirectWritingTriggerTest {
    @Mock private WebContents mWebContents;
    @Mock private DirectWritingServiceBinder mDwServiceBinder;
    @Mock private StylusWritingImeCallback mStylusWritingImeCallback;
    @Mock private ViewGroup mContainerView;
    @Mock private DirectWritingServiceCallback mDwServiceCallback;

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

    private MotionEvent getMotionEvent(int toolType, int action) {
        MotionEvent.PointerProperties[] pointerProperties = new MotionEvent.PointerProperties[1];
        MotionEvent.PointerProperties pp1 = new MotionEvent.PointerProperties();
        pp1.id = 0;
        pp1.toolType = toolType;
        pointerProperties[0] = pp1;
        MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords pc = new MotionEvent.PointerCoords();
        pc.x = 0;
        pc.y = 0;
        pointerCoords[0] = pc;
        return MotionEvent.obtain(
                SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis() + 1,
                action,
                1,
                pointerProperties,
                pointerCoords,
                0,
                0,
                1.0f,
                1.0f,
                0,
                0,
                0,
                0);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnWebContentsChanged() {
        // Test that settings are updated and callback is created if null, when WebContents is set.
        mDwTrigger.setServiceCallbackForTest(mDwServiceCallback);
        doReturn(mStylusWritingImeCallback).when(mWebContents).getStylusWritingImeCallback();
        mDwTrigger.onWebContentsChanged(mContext, mWebContents);
        verify(mDwTrigger).updateDWSettings(mContext);
        verify(mWebContents).setStylusWritingHandler(mDwTrigger);
        verify(mWebContents).getStylusWritingImeCallback();
        verify(mDwServiceCallback).setImeCallback(mStylusWritingImeCallback);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnImeAdapterDestroyed() {
        // Set Ime callback via onWebContentsChanged.
        doReturn(mStylusWritingImeCallback).when(mWebContents).getStylusWritingImeCallback();
        mDwTrigger.onWebContentsChanged(mContext, mWebContents);
        assertNotNull(mDwTrigger.getStylusWritingImeCallbackForTest());

        mDwTrigger.setServiceCallbackForTest(mDwServiceCallback);
        mDwTrigger.onImeAdapterDestroyed();
        assertNull(mDwTrigger.getStylusWritingImeCallbackForTest());
        verify(mDwServiceCallback).setImeCallback(null);
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
        // Set Ime callback via onWebContentsChanged.
        doReturn(mStylusWritingImeCallback).when(mWebContents).getStylusWritingImeCallback();
        mDwTrigger.onWebContentsChanged(mContext, mWebContents);
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
    public void testShouldInitiateStylusWriting() {
        mDwTrigger.updateDWSettings(mContext);
        // requestStartStylusWriting returns false until service is connected.
        // Pass view = null as DW doesn't use the view.
        assertFalse(mDwTrigger.shouldInitiateStylusWriting());
        assertFalse(mDwTrigger.stylusWritingDetected());

        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        assertTrue(mDwTrigger.shouldInitiateStylusWriting());
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
        MotionEvent hoverEnterEvent =
                getMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(hoverEnterEvent, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());

        mDwTrigger.updateDWSettings(mContext);
        // Service is bound only for Hover enter and not hover move.
        MotionEvent hoverMoveEvent =
                getMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_MOVE);
        mDwTrigger.handleHoverEvent(hoverMoveEvent, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());

        mDwTrigger.handleHoverEvent(hoverEnterEvent, mContainerView);
        verify(mDwServiceBinder).bindService(eq(mContext), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_serviceAlreadyConnected() {
        MotionEvent me =
                getMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_HOVER_ENTER);
        // Service is not bound if it is already connected.
        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        mDwTrigger.handleHoverEvent(me, mContainerView);
        verify(mDwServiceBinder, never()).bindService(any(), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_bindServiceWithToolTypeEraser() {
        mDwTrigger.updateDWSettings(mContext);
        MotionEvent me =
                getMotionEvent(MotionEvent.TOOL_TYPE_ERASER, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(me, mContainerView);
        verify(mDwServiceBinder).bindService(eq(mContext), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @RequiresApi(api = Build.VERSION_CODES.P)
    public void testHandleHoverEvent_serviceNotConnectedForNonStylusEvent() {
        mDwTrigger.updateDWSettings(mContext);
        MotionEvent mouseMoveEvent =
                getMotionEvent(MotionEvent.TOOL_TYPE_MOUSE, MotionEvent.ACTION_HOVER_ENTER);
        mDwTrigger.handleHoverEvent(mouseMoveEvent, mContainerView);
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

    @Test
    @Feature({"Stylus Handwriting"})
    public void testFocusNodeChanged_isEditable() {
        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        ShadowDirectWritingSettingsHelper.setEnabled(true);
        mDwTrigger.updateDWSettings(mContext);
        mDwTrigger.setServiceCallbackForTest(mDwServiceCallback);
        // Simulate an ACTION_UP to check if stop recognition is called when editable is focused.
        MotionEvent me = getMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_UP);
        mDwTrigger.handleTouchEvent(me, mContainerView);

        Rect editableBounds = new Rect(0, 0, 20, 20);
        ArgumentCaptor<MotionEvent> eventReceived = ArgumentCaptor.forClass(MotionEvent.class);
        mDwTrigger.onFocusedNodeChanged(editableBounds, true, mContainerView, 2, 5);
        Rect scaledBounds =
                new Rect(
                        editableBounds.left * 2,
                        editableBounds.top * 2 + 5,
                        editableBounds.right * 2,
                        editableBounds.bottom * 2 + 5);
        verify(mDwServiceCallback).updateEditableBounds(eq(scaledBounds), any());
        verify(mDwServiceBinder).updateEditableBounds(scaledBounds, mContainerView, true);
        verify(mDwServiceBinder)
                .onStopRecognition(eventReceived.capture(), eq(scaledBounds), eq(mContainerView));
        assertEquals(eventReceived.getValue().getAction(), MotionEvent.ACTION_UP);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testFocusNodeChanged_isNotEditable() {
        doReturn(true).when(mDwServiceBinder).isServiceConnected();
        ShadowDirectWritingSettingsHelper.setEnabled(true);
        mDwTrigger.updateDWSettings(mContext);
        mDwTrigger.setServiceCallbackForTest(mDwServiceCallback);
        // Simulate an ACTION_UP to verify hide DW toolbar is called when node is not editable.
        MotionEvent me = getMotionEvent(MotionEvent.TOOL_TYPE_STYLUS, MotionEvent.ACTION_UP);
        mDwTrigger.handleTouchEvent(me, mContainerView);

        Rect editableBounds = new Rect(0, 0, 20, 20);
        mDwTrigger.onFocusedNodeChanged(editableBounds, false, mContainerView, 1, 20);
        editableBounds.offset(0, 20);
        verify(mDwServiceCallback).updateEditableBounds(eq(editableBounds), any());
        // Verify that hide DW toolbar is called and stop recognition is also called.
        verify(mDwServiceBinder).hideDWToolbar();
        verify(mDwServiceBinder).onStopRecognition(null, null, mContainerView);
        verify(mDwServiceBinder, never())
                .updateEditableBounds(editableBounds, mContainerView, true);
        verify(mDwServiceBinder, never())
                .onStopRecognition(any(), eq(editableBounds), eq(mContainerView));
    }
}
