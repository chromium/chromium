// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.navigation_pane;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the generic desktop navigation pane. */
@NullMarked
public class NavigationPaneProperties {
    // View types
    public static final int ITEM_TYPE_NAVIGATION_ITEM = 0;
    public static final int ITEM_TYPE_HEADER = 1;
    public static final int ITEM_TYPE_DIVIDER = 2;

    // Navigation Item properties
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] NAVIGATION_ITEM_KEYS = {
        TITLE, ICON, IS_SELECTED, ON_CLICK_HANDLER, CONTENT_DESCRIPTION
    };

    // Header properties
    public static final WritableObjectPropertyKey<String> HEADER_TITLE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] HEADER_KEYS = {HEADER_TITLE};
}
