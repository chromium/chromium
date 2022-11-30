// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.chromium.base.metrics.RecordUserAction;

import java.util.HashMap;
import java.util.Map;

/** Helper class for recording paint preview user actions. */
class PlayerUserActionRecorder {
    private static final String ACTION_FLING = "PaintPreview.Player.Flung";
    private static final String ACTION_SCROLL = "PaintPreview.Player.Scrolled";
    private static final String ACTION_ZOOM = "PaintPreview.Player.Zoomed";
    private static final String ACTION_LINK_CLICK = "PaintPreview.Player.LinkClicked";
    private static final String ACTION_UNCONSUMED_TAP = "PaintPreview.Player.UnconsumedTap";
    private static final String ACTION_LONG_PRESS = "PaintPreview.Player.LongPress";

    private static final long NO_RECORD_WINDOW_MS = (long) (.5 * 1000);
    private static Map<String, Long> sLastRecordMap = new HashMap<>();

    private static boolean shouldNotRecord(String action) {
        long nowMs = System.currentTimeMillis();
        long lastRecordedMs = 0;
        if (sLastRecordMap.get(action) != null) lastRecordedMs = sLastRecordMap.get(action);
        return (nowMs - lastRecordedMs) < NO_RECORD_WINDOW_MS;
    }

    public static void recordScroll() {
        if (shouldNotRecord(ACTION_SCROLL)) return;

        RecordUserAction.record(ACTION_SCROLL);
        sLastRecordMap.put(ACTION_SCROLL, System.currentTimeMillis());
    }

    public static void recordFling() {
        if (shouldNotRecord(ACTION_FLING)) return;

        RecordUserAction.record(ACTION_FLING);
        sLastRecordMap.put(ACTION_FLING, System.currentTimeMillis());
    }

    public static void recordZoom() {
        if (shouldNotRecord(ACTION_ZOOM)) return;

        RecordUserAction.record(ACTION_ZOOM);
        sLastRecordMap.put(ACTION_ZOOM, System.currentTimeMillis());
    }

    public static void recordLinkClick() {
        RecordUserAction.record(ACTION_LINK_CLICK);
    }

    public static void recordUnconsumedTap() {
        RecordUserAction.record(ACTION_UNCONSUMED_TAP);
    }

    public static void recordLongPress() {
        RecordUserAction.record(ACTION_LONG_PRESS);
    }
}
