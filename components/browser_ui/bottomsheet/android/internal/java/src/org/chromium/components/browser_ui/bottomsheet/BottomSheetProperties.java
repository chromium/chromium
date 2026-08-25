// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Model properties for the BottomSheet component. */
@NullMarked
public class BottomSheetProperties {
    public static final WritableIntPropertyKey SHEET_LAYOUT_MODE = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<GlowSpec> GLOW_SPEC =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableFloatPropertyKey CONTAINER_Z = new WritableFloatPropertyKey();
    public static final WritableBooleanPropertyKey CLOSE_BUTTON_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<@Nullable OnClickListener>
            CLOSE_BUTTON_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey CONTAINER_TOUCH_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey FALLBACK_SHADOW_VISIBILITY =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                SHEET_LAYOUT_MODE,
                GLOW_SPEC,
                BACKGROUND_COLOR,
                CONTAINER_Z,
                CLOSE_BUTTON_VISIBILITY,
                CLOSE_BUTTON_CLICK_LISTENER,
                CONTAINER_TOUCH_ENABLED,
                FALLBACK_SHADOW_VISIBILITY
            };

    private BottomSheetProperties() {}
}
