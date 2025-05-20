// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.list_view;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

/** Fake implementation of {@link ListViewTouchTracker} for testing. */
@NullMarked
public final class FakeListViewTouchTracker implements ListViewTouchTracker {

    private @Nullable MotionEventInfo mLastSingleTapUpInfo;

    @Override
    public @Nullable MotionEventInfo getLastSingleTapUp() {
        return mLastSingleTapUpInfo;
    }

    public void setLastSingleTapUpInfo(@Nullable MotionEventInfo listViewTouchInfo) {
        mLastSingleTapUpInfo = listViewTouchInfo;
    }
}
