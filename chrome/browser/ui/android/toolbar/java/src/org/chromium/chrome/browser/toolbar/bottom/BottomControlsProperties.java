// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

class BottomControlsProperties {
    /** The height of the Android View in px. */
    static final WritableIntPropertyKey ANDROID_VIEW_HEIGHT = new WritableIntPropertyKey();

    /** The Y offset of the view in px. */
    static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /**
     * Y translation of Android view, needed if the controls should be drawn above the bottom of the
     * screen.
     */
    static final WritableIntPropertyKey ANDROID_VIEW_TRANSLATE_Y = new WritableIntPropertyKey();

    /** Whether the Android view version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey ANDROID_VIEW_VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the composited version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the view is obscured. */
    static final PropertyModel.WritableBooleanPropertyKey IS_OBSCURED =
            new PropertyModel.WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ANDROID_VIEW_HEIGHT,
                Y_OFFSET,
                ANDROID_VIEW_TRANSLATE_Y,
                ANDROID_VIEW_VISIBLE,
                COMPOSITED_VIEW_VISIBLE,
                IS_OBSCURED
            };
}
