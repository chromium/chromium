// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.content.res.ColorStateList;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * A set of back button properties to reflect its state.
 *
 * @see PropertyKey
 */
class BackButtonProperties {
    public static final WritableObjectPropertyKey<Runnable> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ColorStateList> TINT_COLOR_LIST =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_HIGHLIGHT_RESOURCE =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Runnable> LONG_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_FOCUSABLE = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    public static final WritableObjectPropertyKey<View.OnKeyListener> KEY_LISTENER =
            new WritableObjectPropertyKey<>();

    public static PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLICK_LISTENER,
                TINT_COLOR_LIST,
                BACKGROUND_HIGHLIGHT_RESOURCE,
                LONG_CLICK_LISTENER,
                IS_ENABLED,
                IS_FOCUSABLE,
                IS_VISIBLE,
                ALPHA,
                KEY_LISTENER,
            };

    private BackButtonProperties() {}
}
