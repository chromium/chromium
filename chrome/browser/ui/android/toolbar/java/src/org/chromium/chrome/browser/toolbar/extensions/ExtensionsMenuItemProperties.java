// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
public class ExtensionsMenuItemProperties {

    public static final WritableObjectPropertyKey<@Nullable Bitmap> ICON =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey CONTEXT_MENU_BUTTON_ICON =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<View.OnClickListener>
            CONTEXT_MENU_BUTTON_ON_CLICK = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ICON, TITLE, CONTEXT_MENU_BUTTON_ICON, CONTEXT_MENU_BUTTON_ON_CLICK};
}
