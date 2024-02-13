// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import static android.view.MotionEvent.TOOL_TYPE_ERASER;
import static android.view.MotionEvent.TOOL_TYPE_UNKNOWN;

import android.view.MotionEvent;

import org.chromium.base.metrics.RecordHistogram;

/** Filters touch events which cannot be handled by the web contents due to invalid properties. */
public class TouchEventFilter {
    // crbug.com/1493531 we're receiving motion events with an unsupported tool type but we
    // don't know what the value is. This code logs any offending values so that we can decide
    // how best to deal with them. We're using a sparse histogram as we don't know what the
    // values may be.
    // Update: we want to find out what proportion of tool types are invalid so we now log all tool
    // type values (both valid and invalid).
    /**
     * @return {@code true} if the motion event has a tool type that Blink cannot handle.
     */
    public static boolean hasInvalidToolType(MotionEvent event) {
        boolean unrecognizedToolType = false;
        for (int pointerIdx = 0; pointerIdx < event.getPointerCount(); pointerIdx++) {
            RecordHistogram.recordSparseHistogram(
                    "Input.ToolType.Android", event.getToolType(pointerIdx));
            if (event.getToolType(pointerIdx) < TOOL_TYPE_UNKNOWN
                    || event.getToolType(pointerIdx) > TOOL_TYPE_ERASER) {
                unrecognizedToolType = true;
            }
        }
        return unrecognizedToolType;
    }

    private TouchEventFilter() {}
}
