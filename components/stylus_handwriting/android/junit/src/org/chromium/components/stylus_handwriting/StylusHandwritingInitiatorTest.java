// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.openMocks;

import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for {@link StylusHandwritingInitiator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.TIRAMISU)
public class StylusHandwritingInitiatorTest {
    private @Mock ViewGroup mContainerView;
    private @Mock Context mContext;
    private @Mock InputMethodManager mInputMethodManager;
    private AndroidStylusWritingHandler mHandler;

    @Before
    public void setUp() {
        openMocks(this);
        when(mContext.getSystemService(InputMethodManager.class)).thenReturn(mInputMethodManager);
        mHandler = new AndroidStylusWritingHandler(mContext);
    }

    // Testing that if the movement of Stylus is larger than the slop, startStylusHandwriting should
    // be called.
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handlesLargerThanSlopMoveEventForStylus() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_POINTER_DOWN,
                        MotionEvent.TOOL_TYPE_STYLUS,
                        initX,
                        initY,
                        0,
                        1);
        int x = initX + 10;
        int y = initY + 10;
        MotionEvent moveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        MotionEvent upEvent =
                createMotionEvent(
                        MotionEvent.ACTION_POINTER_UP, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        mHandler.handleTouchEvent(downEvent, mContainerView);
        mHandler.handleTouchEvent(moveEvent, mContainerView);
        mHandler.handleTouchEvent(upEvent, mContainerView);
        verify(mInputMethodManager).startStylusHandwriting(mContainerView);
    }

    // Testing that if the movement of Stylus is even with the slop, startStylusHandwriting should
    // be called.
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handlesEqualToSlopMoveEventForStylus() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 0, 1);
        int x = initX + 2;
        int y = initY + 2;
        MotionEvent moveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        MotionEvent upEvent =
                createMotionEvent(MotionEvent.ACTION_UP, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        mHandler.handleTouchEvent(downEvent, mContainerView);
        mHandler.handleTouchEvent(moveEvent, mContainerView);
        mHandler.handleTouchEvent(upEvent, mContainerView);
        verify(mInputMethodManager).startStylusHandwriting(mContainerView);
    }

    // Testing that if the movement of Stylus is smaller than the slop, startStylusHandwriting
    // should not be called.
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handlesSmallerThanSlopMoveEventForStylus() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 0, 1);
        int x = initX + 1;
        int y = initY + 1;
        MotionEvent moveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        MotionEvent upEvent =
                createMotionEvent(MotionEvent.ACTION_UP, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);
        mHandler.handleTouchEvent(downEvent, mContainerView);
        mHandler.handleTouchEvent(moveEvent, mContainerView);
        mHandler.handleTouchEvent(upEvent, mContainerView);
        verify(mInputMethodManager, never()).startStylusHandwriting(mContainerView);
    }

    // Testing that the code handles multiple move events in one go
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handlesMultipleMoveEventsForStylus() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 0, 1);
        int firstMoveX = initX + 1;
        int firstMoveY = initY + 1;
        MotionEvent firstMoveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE,
                        MotionEvent.TOOL_TYPE_STYLUS,
                        firstMoveX,
                        firstMoveY,
                        1,
                        1);
        mHandler.handleTouchEvent(downEvent, mContainerView);
        mHandler.handleTouchEvent(firstMoveEvent, mContainerView);
        verify(mInputMethodManager, never()).startStylusHandwriting(mContainerView);
        int secondMoveX = initX + 10;
        int secondMoveY = initY + 10;
        MotionEvent secondMoveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE,
                        MotionEvent.TOOL_TYPE_STYLUS,
                        secondMoveX,
                        secondMoveY,
                        2,
                        1);
        MotionEvent upEvent =
                createMotionEvent(
                        MotionEvent.ACTION_UP,
                        MotionEvent.TOOL_TYPE_STYLUS,
                        secondMoveX,
                        secondMoveY,
                        2,
                        1);
        mHandler.handleTouchEvent(secondMoveEvent, mContainerView);
        mHandler.handleTouchEvent(upEvent, mContainerView);
        verify(mInputMethodManager).startStylusHandwriting(mContainerView);
    }

    // Testing that if two styluses are active and one of them is stopped being used, the other one
    // should still be active.
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handlesMoreThanOneStyluses() {
        int initX = 10;
        int initY = 10;
        int x = initX + 10;
        int y = initY + 10;
        MotionEvent downEventStylusOne =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 0, 1);
        MotionEvent downEventStylusTwo =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 1, 2);
        MotionEvent moveEventStylusTwo =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 2, 2);
        MotionEvent upEventStylusTwo =
                createMotionEvent(
                        MotionEvent.ACTION_POINTER_UP,
                        MotionEvent.TOOL_TYPE_STYLUS,
                        initX,
                        initY,
                        3,
                        2);
        MotionEvent moveEventStylusOne =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 4, 1);
        mHandler.handleTouchEvent(downEventStylusOne, mContainerView);
        mHandler.handleTouchEvent(downEventStylusTwo, mContainerView);
        mHandler.handleTouchEvent(moveEventStylusTwo, mContainerView);
        mHandler.handleTouchEvent(upEventStylusTwo, mContainerView);
        verify(mInputMethodManager, never()).startStylusHandwriting(mContainerView);
        mHandler.handleTouchEvent(moveEventStylusOne, mContainerView);
        verify(mInputMethodManager).startStylusHandwriting(mContainerView);
    }

    // Testing that if the tool type is something other than a Stylus, startStylusHandwriting
    // should not be called.
    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void touchEventsDoNotTriggerHandwriting() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_FINGER, initX, initY, 0, 1);
        int x = initX + 10;
        int y = initY + 10;
        MotionEvent moveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_FINGER, x, y, 1, 1);
        MotionEvent upEvent =
                createMotionEvent(MotionEvent.ACTION_UP, MotionEvent.TOOL_TYPE_FINGER, x, y, 1, 1);
        mHandler.handleTouchEvent(downEvent, mContainerView);
        mHandler.handleTouchEvent(moveEvent, mContainerView);
        mHandler.handleTouchEvent(upEvent, mContainerView);
        verify(mInputMethodManager, never()).startStylusHandwriting(mContainerView);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    @EnableFeatures("UseHandwritingInitiator")
    public void handwritingIsNotTriggeredIfViewIsNotWritable() {
        int initX = 10;
        int initY = 10;
        MotionEvent downEvent =
                createMotionEvent(
                        MotionEvent.ACTION_DOWN, MotionEvent.TOOL_TYPE_STYLUS, initX, initY, 0, 1);
        int x = initX + 10;
        int y = initY + 10;
        MotionEvent moveEvent =
                createMotionEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.TOOL_TYPE_STYLUS, x, y, 1, 1);

        StylusHandwritingInitiator stylusHandwritingInitiatorSpy =
                spy(new StylusHandwritingInitiator(mInputMethodManager));
        doReturn(false).when(stylusHandwritingInitiatorSpy).isViewWritable();
        AndroidStylusWritingHandler androidStylusWritingHandler =
                new AndroidStylusWritingHandler(mContext);
        androidStylusWritingHandler.setHandwritingInitiatorForTesting(
                stylusHandwritingInitiatorSpy);

        androidStylusWritingHandler.handleTouchEvent(downEvent, mContainerView);
        androidStylusWritingHandler.handleTouchEvent(moveEvent, mContainerView);
        verify(mInputMethodManager, never()).startStylusHandwriting(mContainerView);
    }

    public MotionEvent createMotionEvent(
            int actionType, int toolType, int x, int y, int eventTime, int deviceId) {
        MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
        properties.toolType = toolType;
        MotionEvent.PointerCoords pointerFirstCoords = new MotionEvent.PointerCoords();
        MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[1];
        pointerFirstCoords.x = x;
        pointerFirstCoords.y = y;
        pointerCoords[0] = pointerFirstCoords;

        properties.id = 0;

        MotionEvent stylusEvent =
                MotionEvent.obtain(
                        SystemClock.uptimeMillis(),
                        eventTime,
                        actionType,
                        1,
                        new MotionEvent.PointerProperties[] {properties},
                        pointerCoords,
                        0,
                        0,
                        0f,
                        0f,
                        deviceId,
                        0,
                        2,
                        0);

        return stylusEvent;
    }
}
