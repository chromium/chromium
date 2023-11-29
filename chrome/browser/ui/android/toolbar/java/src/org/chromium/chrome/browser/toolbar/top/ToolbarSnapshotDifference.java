// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Reasons that two toolbar snapshots are different. Contains a superset of differences and each
 * toolbar instance will only be able to report a subset. Treat this list as append only and keep it
 * in sync with ToolbarSnapshotDifference in enums.xml, as well as the proto in
 * chrome_track_event.proto.
 */
@IntDef({
    ToolbarSnapshotDifference.NONE,
    ToolbarSnapshotDifference.NULL,
    ToolbarSnapshotDifference.TINT,
    ToolbarSnapshotDifference.TAB_COUNT,
    ToolbarSnapshotDifference.OPTIONAL_BUTTON,
    ToolbarSnapshotDifference.VISUAL_STATE,
    ToolbarSnapshotDifference.SECURITY_ICON,
    ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE,
    ToolbarSnapshotDifference.PAINT_PREVIEW,
    ToolbarSnapshotDifference.PROGRESS,
    ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
    ToolbarSnapshotDifference.URL_TEXT,
    ToolbarSnapshotDifference.HOME_BUTTON,
    ToolbarSnapshotDifference.TITLE_TEXT,
    ToolbarSnapshotDifference.CCT_ANIMATION,
    ToolbarSnapshotDifference.BOOKMARK_BUTTON,
    ToolbarSnapshotDifference.BACK_BUTTON,
    ToolbarSnapshotDifference.FORWARD_BUTTON,
    ToolbarSnapshotDifference.RELOAD_BUTTON,
    ToolbarSnapshotDifference.MINIMIZE_BUTTON,
    ToolbarSnapshotDifference.NUM_ENTRIES
})
@Retention(RetentionPolicy.SOURCE)
public @interface ToolbarSnapshotDifference {
    int NONE = 0;
    int NULL = 1;
    int TINT = 2;
    int TAB_COUNT = 3;
    int OPTIONAL_BUTTON = 4;
    int VISUAL_STATE = 5;
    int SECURITY_ICON = 6;
    int SHOWING_UPDATE_BADGE = 7;
    int PAINT_PREVIEW = 8;
    int PROGRESS = 9;
    int LOCATION_BAR_WIDTH = 10;
    int URL_TEXT = 11;
    int HOME_BUTTON = 12;
    int TITLE_TEXT = 13;
    int CCT_ANIMATION = 14;
    int BOOKMARK_BUTTON = 15;
    int BACK_BUTTON = 16;
    int FORWARD_BUTTON = 17;
    int RELOAD_BUTTON = 18;
    int MINIMIZE_BUTTON = 19;

    int NUM_ENTRIES = 20;
}
