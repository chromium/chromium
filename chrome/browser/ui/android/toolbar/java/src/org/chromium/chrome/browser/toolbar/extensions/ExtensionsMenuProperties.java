// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class ExtensionsMenuProperties {
    public static final WritableObjectPropertyKey<View.OnClickListener> CLOSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            DISCOVER_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            MANAGE_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLOSE_CLICK_LISTENER,
                DISCOVER_EXTENSIONS_CLICK_LISTENER,
                MANAGE_EXTENSIONS_CLICK_LISTENER
            };
}
