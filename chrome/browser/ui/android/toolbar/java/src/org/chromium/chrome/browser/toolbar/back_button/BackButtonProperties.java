// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import android.content.res.ColorStateList;

import org.chromium.ui.modelutil.PropertyKey;
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
    public static WritableObjectPropertyKey<ColorStateList> TINT_COLOR_LIST =
            new WritableObjectPropertyKey<>();
    public static WritableIntPropertyKey BACKGROUND_HIGHLIGHT_RESOURCE =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Runnable> LONG_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLICK_LISTENER, TINT_COLOR_LIST, BACKGROUND_HIGHLIGHT_RESOURCE, LONG_CLICK_LISTENER
            };

    private BackButtonProperties() {}
}
