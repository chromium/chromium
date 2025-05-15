// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Tracks touch events on a {@link android.widget.ListView}. */
@NullMarked
public interface ListViewTouchTracker {

    /**
     * A plain old data class that holds information about a touch {@link MotionEvent}.
     *
     * <p>This class should be used when information about a {@link MotionEvent} needs to be kept as
     * a state. We shouldn't keep {@link MotionEvent}s as states since the Android framework will
     * recycle them.
     *
     * @see MotionEvent#recycle()
     */
    final class ListViewTouchInfo {

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

        /** Derives {@link ListViewTouchInfo} from a {@link MotionEvent}. */
        public static ListViewTouchInfo fromMotionEvent(MotionEvent motionEvent) {
            int[] toolType = new int[motionEvent.getPointerCount()];
            for (int i = 0; i < toolType.length; i++) {
                toolType[i] = motionEvent.getToolType(i);
            }

            return new ListViewTouchInfo(
                    motionEvent.getAction(), motionEvent.getSource(), toolType);
        }

        private ListViewTouchInfo(int action, int source, int[] toolType) {
            this.action = action;
            this.source = source;
            this.toolType = toolType;
        }
    }

    /**
     * Returns the last {@link ListViewTouchInfo} that was a single tap up, as detected by {@link
     * android.view.GestureDetector}.
     */
    @Nullable ListViewTouchInfo getLastSingleTapUp();
}
