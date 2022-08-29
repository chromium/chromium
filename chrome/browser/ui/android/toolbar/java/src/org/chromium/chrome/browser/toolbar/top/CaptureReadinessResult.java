// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains whether the toolbar is ready for a capture, and a reason for that boolean choice. Often
 * implementors may find they have multiple reasons for the same boolean result, but they should
 * arbitrarily and consistently pick one reason. The reason is used to report metrics and should
 * still be a useful tool for understanding captures.
 */
class CaptureReadinessResult {
    /**
     * Reasons to allow toolbar captures. Treat this list as append only and keep it in sync with
     * TopToolbarAllowCaptureReason in enums.xml.
     **/
    @IntDef({TopToolbarAllowCaptureReason.UNKNOWN, TopToolbarAllowCaptureReason.FORCE_CAPTURE,
            TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE,
            TopToolbarAllowCaptureReason.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface TopToolbarAllowCaptureReason {
        int UNKNOWN = 0;
        int FORCE_CAPTURE = 1;
        int SNAPSHOT_DIFFERENCE = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Reasons to block toolbar captures. Treat this list as append only and keep it in sync with
     * TopToolbarBlockCaptureReason in enums.xml.
     **/
    @IntDef({TopToolbarBlockCaptureReason.UNKNOWN,
            TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL,
            TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY, TopToolbarBlockCaptureReason.SNAPSHOT_SAME,
            TopToolbarBlockCaptureReason.URL_BAR_HAS_FOCUS,
            TopToolbarBlockCaptureReason.URL_BAR_FOCUS_IN_PROGRESS,
            TopToolbarBlockCaptureReason.OPTIONAL_BUTTON_ANIMATION_IN_PROGRESS,
            TopToolbarBlockCaptureReason.STATUS_ICON_ANIMATION_IN_PROGRESS,
            TopToolbarBlockCaptureReason.SCROLL_ABLATION,
            TopToolbarBlockCaptureReason.BROWSER_CONTROLS_LOCKED,
            TopToolbarBlockCaptureReason.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface TopToolbarBlockCaptureReason {
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
        // TODO(https://crbug.com/1324678): SCROLL_IN_PROGRESS.
        // TODO(https://crbug.com/1324678): NATIVE_PAGE.
        int NUM_ENTRIES = 10;
    }

    public static CaptureReadinessResult readyForced() {
        return new CaptureReadinessResult(true, TopToolbarAllowCaptureReason.FORCE_CAPTURE,
                TopToolbarBlockCaptureReason.UNKNOWN, ToolbarSnapshotDifference.NONE);
    }

    public static CaptureReadinessResult readyWithSnapshotDifference(
            @ToolbarSnapshotDifference int difference) {
        return new CaptureReadinessResult(true, TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE,
                TopToolbarBlockCaptureReason.UNKNOWN, difference);
    }

    public static CaptureReadinessResult notReady(@TopToolbarBlockCaptureReason int blockReason) {
        return new CaptureReadinessResult(false, TopToolbarAllowCaptureReason.UNKNOWN, blockReason,
                ToolbarSnapshotDifference.NONE);
    }

    /* Used for legacy call sites where metrics are not filled out. */
    public static CaptureReadinessResult unknown(boolean isReady) {
        return new CaptureReadinessResult(isReady, TopToolbarAllowCaptureReason.UNKNOWN,
                TopToolbarBlockCaptureReason.UNKNOWN, ToolbarSnapshotDifference.NONE);
    }

    public static void logAllowCaptureReason(@TopToolbarAllowCaptureReason int reason) {
        RecordHistogram.recordEnumeratedHistogram("Android.TopToolbar.AllowCaptureReason", reason,
                TopToolbarAllowCaptureReason.NUM_ENTRIES);
    }

    public static void logBlockCaptureReason(@TopToolbarBlockCaptureReason int reason) {
        RecordHistogram.recordEnumeratedHistogram("Android.TopToolbar.BlockCaptureReason", reason,
                TopToolbarBlockCaptureReason.NUM_ENTRIES);
    }

    public static void logCaptureReasonFromResult(CaptureReadinessResult result) {
        if (result == null) {
            logBlockCaptureReason(TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL);
        } else if (result.isReady) {
            logAllowCaptureReason(result.allowReason);
            if (result.allowReason == TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE) {
                RecordHistogram.recordEnumeratedHistogram("Android.TopToolbar.SnapshotDifference",
                        result.snapshotDifference, ToolbarSnapshotDifference.NUM_ENTRIES);
            }
        } else {
            logBlockCaptureReason(result.blockReason);
        }
    }

    public final boolean isReady;
    public final @TopToolbarBlockCaptureReason int blockReason;
    public final @TopToolbarAllowCaptureReason int allowReason;
    public final @ToolbarSnapshotDifference int snapshotDifference;

    private CaptureReadinessResult(boolean isReady, @TopToolbarAllowCaptureReason int allowReason,
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
