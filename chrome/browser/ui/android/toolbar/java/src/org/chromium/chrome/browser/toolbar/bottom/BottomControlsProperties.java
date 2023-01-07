// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

class BottomControlsProperties {
    /** The height of the bottom control container (view which includes the top shadow) in px. */
    static final WritableIntPropertyKey BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX =
            new WritableIntPropertyKey();

    /** The Y offset of the view in px. */
    static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /** Whether the Android view version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey ANDROID_VIEW_VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the composited version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the view is obscured. */
    static final PropertyModel.WritableBooleanPropertyKey IS_OBSCURED =
            new PropertyModel.WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX,
            Y_OFFSET, ANDROID_VIEW_VISIBLE, COMPOSITED_VIEW_VISIBLE, IS_OBSCURED};
}
