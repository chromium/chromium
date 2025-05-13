// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Reload button properties set. */
@NullMarked
class ReloadButtonProperties {

    public static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    public static final WritableIntPropertyKey DRAWABLE_LEVEL = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<View.OnKeyListener> KEY_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> LONG_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<MotionEvent>> TOUCH_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<CharSequence> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> BACKGROUND_HIGHLIGHT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ColorStateList> TINT_LIST =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                DRAWABLE_LEVEL,
                KEY_LISTENER,
                CLICK_LISTENER,
                LONG_CLICK_LISTENER,
                TOUCH_LISTENER,
                IS_ENABLED,
                IS_VISIBLE,
                CONTENT_DESCRIPTION,
                TINT_LIST,
                BACKGROUND_HIGHLIGHT
            };

    private ReloadButtonProperties() {}
}
