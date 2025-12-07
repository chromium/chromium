// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.motion;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;

/**
 * A plain old data class that holds information about a {@link MotionEvent}.
 *
 * <p>It's safer to use this class when passing information about a {@link MotionEvent}, since using
 * raw {@link MotionEvent}s needs to ensure they are not recycled by the Android framework before
 * they are read.
 *
 * @see MotionEvent#recycle()
 */
@NullMarked
public final class MotionEventInfo {

    /**
     * @see MotionEvent#getAction()
     */
    public final int action;

    /**
     * @see MotionEvent#getSource()
     */
    public final int source;

    /**
     * @see MotionEvent#getToolType(int)
     */
    public final int[] toolType;

    /** Derives {@link MotionEventInfo} from a {@link MotionEvent}. */
    public static MotionEventInfo fromMotionEvent(MotionEvent motionEvent) {
        int[] toolType = new int[motionEvent.getPointerCount()];
        for (int i = 0; i < toolType.length; i++) {
            toolType[i] = motionEvent.getToolType(i);
        }

        return new MotionEventInfo(motionEvent.getAction(), motionEvent.getSource(), toolType);
    }

    private MotionEventInfo(int action, int source, int[] toolType) {
        this.action = action;
        this.source = source;
        this.toolType = toolType;
    }
}
