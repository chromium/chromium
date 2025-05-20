// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/** Tracks touch events on a {@link android.widget.ListView}. */
@NullMarked
public interface ListViewTouchTracker {

    /**
     * Returns the last {@link MotionEventInfo} that was a single tap up, as detected by {@link
     * android.view.GestureDetector}.
     */
    @Nullable MotionEventInfo getLastSingleTapUp();
}
