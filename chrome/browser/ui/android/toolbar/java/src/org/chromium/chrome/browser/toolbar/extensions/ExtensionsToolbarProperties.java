// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class ExtensionsToolbarProperties {
    public static final WritableBooleanPropertyKey IS_REQUEST_ACCESS_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey("IS_REQUEST_ACCESS_BUTTON_VISIBLE");

    // -1 is used as a sentinel value to indicate the "Allowed" confirmation state.
    public static final WritableIntPropertyKey REQUEST_ACCESS_BUTTON_EXTENSION_COUNT =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<View.OnClickListener>
            REQUEST_ACCESS_BUTTON_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String>
            REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey EXTENSIONS_MENU_BUTTON_DEFAULT_BACKGROUND =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_REQUEST_ACCESS_BUTTON_VISIBLE,
                REQUEST_ACCESS_BUTTON_EXTENSION_COUNT,
                REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION,
                EXTENSIONS_MENU_BUTTON_DEFAULT_BACKGROUND,
                REQUEST_ACCESS_BUTTON_CLICK_LISTENER
            };
}
