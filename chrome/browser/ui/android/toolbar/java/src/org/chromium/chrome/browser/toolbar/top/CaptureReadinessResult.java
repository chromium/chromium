// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.IntDef;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains whether the toolbar is ready for a capture, and a reason for that boolean choice. Often
 * implementors may find they have multiple reasons for the same boolean result, but they should
 * arbitrarily and consistently pick one reason. The reason is used to report metrics and should
 * still be a useful tool for understanding captures.
 */
public class CaptureReadinessResult {
    /**
     * Reasons to allow toolbar captures. Treat this list as append only and keep it in sync with
     * TopToolbarAllowCaptureReason in enums.xml, as well as the proto in chrome_track_event.proto.
     **/
    @IntDef({
        TopToolbarAllowCaptureReason.UNKNOWN,
        TopToolbarAllowCaptureReason.FORCE_CAPTURE,
        TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE,
        TopToolbarAllowCaptureReason.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TopToolbarAllowCaptureReason {
        int UNKNOWN = 0;
        int FORCE_CAPTURE = 1;
        int SNAPSHOT_DIFFERENCE = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Reasons to block toolbar captures. Treat this list as append only and keep it in sync with
     * TopToolbarBlockCaptureReason in enums.xml, as well as the proto in chrome_track_event.proto.
     */
    @IntDef({
        TopToolbarBlockCaptureReason.UNKNOWN,
        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL,
        TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY,
        TopToolbarBlockCaptureReason.SNAPSHOT_SAME,
        TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS,
        TopToolbarBlockCaptureReason.URL_BAR_FOCUS_IN_PROGRESS,
        TopToolbarBlockCaptureReason.OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS,
        TopToolbarBlockCaptureReason.STATUS_ICON_ANIMATION_IN_PROGRESS,
        TopToolbarBlockCaptureReason.SCROLL_ABLATION,
        TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED,
        TopToolbarBlockCaptureReason.TAB_SWITCHER_MODE,
        TopToolbarBlockCaptureReason.COMPOSITOR_IN_MOTION,
        TopToolbarBlockCaptureReason.NTP_Y_TRANSLATION,
        TopToolbarBlockCaptureReason.FULLSCREEN,
        TopToolbarBlockCaptureReason.TABLET_BUTTON_ANIMATION_IN_PROGRESS,
        TopToolbarBlockCaptureReason.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TopToolbarBlockCaptureReason {
        int UNKNOWN = 0;
        int TOOLBAR_OR_RESULT_NULL = 1;
        int VIEW_NOT_DIRTY = 2;
        int SNAPSHOT_SAME = 3;
        int URL_BAR_HAS_FOCUS = 4;
        int URL_BAR_FOCUS_IN_PROGRESS = 5;
        int OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS = 6;
        int STATUS_ICON_ANIMATION_IN_PROGRESS = 7;
        int SCROLL_ABLATION = 8;
        int BROWSER_CONTROLS_LOCKED = 9;
        int TAB_SWITCHER_MODE = 10;
        int COMPOSITOR_IN_MOTION = 11;
        int NTP_Y_TRANSLATION = 12;
        int FULLSCREEN = 13;
        int TABLET_BUTTON_ANIMATION_IN_PROGRESS = 14;
        int NUM_ENTRIES = 15;
    }

    public static CaptureReadinessResult readyForced() {
        return new CaptureReadinessResult(
                true,
                TopToolbarAllowCaptureReason.FORCE_CAPTURE,
                TopToolbarBlockCaptureReason.UNKNOWN,
                ToolbarSnapshotDifference.NONE);
    }

    public static CaptureReadinessResult readyWithSnapshotDifference(
            @ToolbarSnapshotDifference int difference) {
        return new CaptureReadinessResult(
                true,
                TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE,
                TopToolbarBlockCaptureReason.UNKNOWN,
                difference);
    }

    public static CaptureReadinessResult notReady(@TopToolbarBlockCaptureReason int blockReason) {
        return new CaptureReadinessResult(
                false,
                TopToolbarAllowCaptureReason.UNKNOWN,
                blockReason,
                ToolbarSnapshotDifference.NONE);
    }

    /* Used for legacy call sites where metrics are not filled out. */
    public static CaptureReadinessResult unknown(boolean isReady) {
        return new CaptureReadinessResult(
                isReady,
                TopToolbarAllowCaptureReason.UNKNOWN,
                TopToolbarBlockCaptureReason.UNKNOWN,
                ToolbarSnapshotDifference.NONE);
    }

    private static void logAllowCaptureReason(@TopToolbarAllowCaptureReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TopToolbar.AllowCaptureReason",
                reason,
                TopToolbarAllowCaptureReason.NUM_ENTRIES);
    }

    private static void logBlockCaptureReason(@TopToolbarBlockCaptureReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TopToolbar.BlockCaptureReason",
                reason,
                TopToolbarBlockCaptureReason.NUM_ENTRIES);
    }

    public static void logCaptureReasonFromResult(CaptureReadinessResult result) {
        if (!ToolbarFeatures.shouldRecordSuppressionMetrics()) {
            return;
        }

        // The Java -> C++ layer makes passing enums tricky so we store the integer value and then
        // convert it to a proto enum on the C++ side. If we pass a -1 we will not set that
        // corresponding field.
        int blockReason = -1;
        int allowReason = -1;
        int snapshotDiff = -1;

        // Log the reason to UMA and update our trace event values.
        if (result == null) {
            blockReason = TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL;
            logBlockCaptureReason(blockReason);
        } else if (result.isReady) {
            allowReason = result.allowReason;
            logAllowCaptureReason(allowReason);
            if (result.allowReason == TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE) {
                snapshotDiff = result.snapshotDifference;
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.TopToolbar.SnapshotDifference",
                        snapshotDiff,
                        ToolbarSnapshotDifference.NUM_ENTRIES);
            }
        } else {
            blockReason = result.blockReason;
            logBlockCaptureReason(blockReason);
        }
        // Emit our trace event that will tell use why this capture occurred.
        TraceEvent.instantAndroidToolbar(blockReason, allowReason, snapshotDiff);
    }

    public final boolean isReady;
    public final @TopToolbarBlockCaptureReason int blockReason;
    public final @TopToolbarAllowCaptureReason int allowReason;
    public final @ToolbarSnapshotDifference int snapshotDifference;

    private CaptureReadinessResult(
            boolean isReady,
            @TopToolbarAllowCaptureReason int allowReason,
            @TopToolbarBlockCaptureReason int blockReason,
            @ToolbarSnapshotDifference int snapshotDifference) {
        this.isReady = isReady;

        assert blockReason == TopToolbarBlockCaptureReason.UNKNOWN
                || allowReason == TopToolbarAllowCaptureReason.UNKNOWN;
        this.blockReason = blockReason;
        this.allowReason = allowReason;

        assert snapshotDifference == ToolbarSnapshotDifference.NONE
                || allowReason == TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE;
        this.snapshotDifference = snapshotDifference;
    }
}
