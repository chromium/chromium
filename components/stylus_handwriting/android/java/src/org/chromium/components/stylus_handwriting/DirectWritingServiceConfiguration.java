// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_CONFIG_HIDE_DELAY;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_CONFIG_KEEP_WRITING_DELAY;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_CONFIG_TRIGGER_HORIZONTAL_SPACE_DEFAULT;
import static android.widget.directwriting.IDirectWritingService.KEY_BUNDLE_CONFIG_TRIGGER_VERTICAL_SPACE;

import android.os.Bundle;

/** This class stores the Configuration values received from Direct Writing service. */
class DirectWritingServiceConfiguration {
    private static final long DEFAULT_HIDE_DW_TOOLBAR_DELAY_MS = 1000L;
    private static final long DEFAULT_KEEP_WRITING_DELAY_MS = 1000L;
    private static final int DEFAULT_VERTICAL_SLOP_PX = 56;
    private static final int DEFAULT_HORIZONTAL_SLOP_PX = 56;

    private long mDefaultHideDwToolbarDelayMs = DEFAULT_HIDE_DW_TOOLBAR_DELAY_MS;
    private long mDefaultKeepWritingDelayMs = DEFAULT_KEEP_WRITING_DELAY_MS;
    private int mVerticalSlopPx = DEFAULT_VERTICAL_SLOP_PX;
    private int mHorizontalSlopPx = DEFAULT_HORIZONTAL_SLOP_PX;

    void update(Bundle bundle) {
        mDefaultHideDwToolbarDelayMs =
                bundle.getLong(KEY_BUNDLE_CONFIG_HIDE_DELAY, DEFAULT_HIDE_DW_TOOLBAR_DELAY_MS);
        mDefaultKeepWritingDelayMs =
                bundle.getLong(KEY_BUNDLE_CONFIG_KEEP_WRITING_DELAY, DEFAULT_KEEP_WRITING_DELAY_MS);
        mVerticalSlopPx =
                bundle.getInt(KEY_BUNDLE_CONFIG_TRIGGER_VERTICAL_SPACE, DEFAULT_VERTICAL_SLOP_PX);
        mHorizontalSlopPx =
                bundle.getInt(
                        KEY_BUNDLE_CONFIG_TRIGGER_HORIZONTAL_SPACE_DEFAULT,
                        DEFAULT_HORIZONTAL_SLOP_PX);
    }

    long getHideDwToolbarDelayMs() {
        return mDefaultHideDwToolbarDelayMs;
    }

    long getKeepWritingDelayMs() {
        return mDefaultKeepWritingDelayMs;
    }

    int getTriggerVerticalSpacePx() {
        return mVerticalSlopPx;
    }

    int getTriggerHorizontalSpacePx() {
        return mHorizontalSlopPx;
    }
}
