// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.motion;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;

import org.chromium.build.annotations.NullMarked;

/** Utils for creating {@link MotionEvent} and {@link MotionEventInfo} for tests. */
@NullMarked
public final class MotionEventTestUtils {

    private MotionEventTestUtils() {}

    /**
     * Creates {@link MotionEventInfo} that matches a touch screen motion.
     *
     * @see #createTouchMotionEvent(long, long, int)
     */
    public static MotionEventInfo createTouchMotionInfo(long downTime, long eventTime, int action) {
        return MotionEventInfo.fromMotionEvent(createTouchMotionEvent(downTime, eventTime, action));
    }

    /**
     * Creates {@link MotionEventInfo} that matches a mouse motion.
     *
     * @see #createMouseMotionEvent(long, long, int)
     */
    public static MotionEventInfo createMouseMotionInfo(long downTime, long eventTime, int action) {
        return MotionEventInfo.fromMotionEvent(createMouseMotionEvent(downTime, eventTime, action));
    }

    /**
     * Creates a {@link MotionEvent} that matches a touch screen motion.
     *
     * @see #createMotionEvent(long, long, int, float, float, int, int)
     */
    public static MotionEvent createTouchMotionEvent(long downTime, long eventTime, int action) {
        return createMotionEvent(
                downTime,
                eventTime,
                action,
                /* x= */ 0,
                /* y= */ 0,
                InputDevice.SOURCE_TOUCHSCREEN,
                MotionEvent.TOOL_TYPE_FINGER);
    }

    /**
     * Creates a {@link MotionEvent} that matches a mouse motion.
     *
     * @see #createMouseMotionEvent(long, long, int, float, float)
     */
    public static MotionEvent createMouseMotionEvent(long downTime, long eventTime, int action) {
        return createMouseMotionEvent(downTime, eventTime, action, /* x= */ 0, /* y= */ 0);
    }

    /**
     * Creates a {@link MotionEvent} that matches a mouse motion.
     *
     * @see #createMotionEvent(long, long, int, float, float, int, int)
     */
    public static MotionEvent createMouseMotionEvent(
            long downTime, long eventTime, int action, float x, float y) {
        return createMotionEvent(
                downTime,
                eventTime,
                action,
                x,
                y,
                InputDevice.SOURCE_MOUSE,
                MotionEvent.TOOL_TYPE_MOUSE);
    }

    /**
     * Creates a {@link MotionEvent}.
     *
     * <p>All parameters are for {@link MotionEvent#obtain}.
     */
    public static MotionEvent createMotionEvent(
            long downTime, long eventTime, int action, float x, float y, int source, int toolType) {
        PointerProperties pointerProperties = new MotionEvent.PointerProperties();
        pointerProperties.id = 0;
        pointerProperties.toolType = toolType;

        PointerCoords pointerCoords = new PointerCoords();
        pointerCoords.x = x;
        pointerCoords.y = y;

        return MotionEvent.obtain(
                downTime,
                eventTime,
                action,
                /* pointerCount= */ 1,
                new PointerProperties[] {pointerProperties},
                new PointerCoords[] {pointerCoords},
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1.0f,
                /* yPrecision= */ 1.0f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                source,
                /* flags= */ 0);
    }
}
