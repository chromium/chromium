// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.CompoundButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
public class ExtensionsMenuItemProperties {

    public static final WritableObjectPropertyKey<String> EXTENSION_ID =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<@Nullable Bitmap> ICON =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_PINNED = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View.OnClickListener>
            CONTEXT_MENU_BUTTON_ON_CLICK = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View.OnClickListener> PRIMARY_ACTION_ON_CLICK =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey SITE_ACCESS_TOGGLE_CHECKED =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<CompoundButton.OnCheckedChangeListener>
            SITE_ACCESS_TOGGLE_ON_CLICK = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey SITE_ACCESS_TOGGLE_STATUS =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> SITE_ACCESS_TOGGLE_TOOLTIP =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View.OnClickListener>
            SITE_PERMISSIONS_BUTTON_ON_CLICK = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey SITE_PERMISSIONS_BUTTON_STATUS =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> SITE_PERMISSIONS_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> SITE_PERMISSIONS_BUTTON_TOOLTIP =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_ENTERPRISE = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                EXTENSION_ID,
                ICON,
                IS_PINNED,
                TITLE,
                CONTEXT_MENU_BUTTON_ON_CLICK,
                PRIMARY_ACTION_ON_CLICK,
                SITE_ACCESS_TOGGLE_CHECKED,
                SITE_ACCESS_TOGGLE_ON_CLICK,
                SITE_ACCESS_TOGGLE_STATUS,
                SITE_ACCESS_TOGGLE_TOOLTIP,
                SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME,
                SITE_PERMISSIONS_BUTTON_ON_CLICK,
                SITE_PERMISSIONS_BUTTON_STATUS,
                SITE_PERMISSIONS_BUTTON_TEXT,
                SITE_PERMISSIONS_BUTTON_TOOLTIP,
                IS_ENTERPRISE
            };
}
