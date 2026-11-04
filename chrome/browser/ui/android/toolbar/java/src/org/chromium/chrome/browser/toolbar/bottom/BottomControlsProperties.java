// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class BottomControlsProperties {
    /** The height of the Android View in px, not including padding. */
    static final WritableIntPropertyKey ANDROID_VIEW_HEIGHT_NO_PADDING =
            new WritableIntPropertyKey();

    /** The bottom padding of the Android View in px, used for EdgeToEdge. */
    static final WritableIntPropertyKey BOTTOM_PADDING = new WritableIntPropertyKey();

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
    static final WritableBooleanPropertyKey IS_OBSCURED = new WritableBooleanPropertyKey();

    /** The tag indicating that this layer should be moved by viz. */
    static final WritableObjectPropertyKey<OffsetTag> OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    /** Whether the top shadow should be visible. */
    static final WritableBooleanPropertyKey SHOW_SHADOW = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                // go/keep-sorted start
                ANDROID_VIEW_HEIGHT_NO_PADDING,
                ANDROID_VIEW_TRANSLATE_Y,
                ANDROID_VIEW_VISIBLE,
                BOTTOM_PADDING,
                COMPOSITED_VIEW_VISIBLE,
                IS_OBSCURED,
                OFFSET_TAG,
                SHOW_SHADOW,
                Y_OFFSET
                // go/keep-sorted end
            };
}
