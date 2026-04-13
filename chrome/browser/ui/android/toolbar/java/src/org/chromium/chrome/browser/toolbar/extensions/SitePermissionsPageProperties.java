// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the site permissions page in the extensions menu. */
@NullMarked
public class SitePermissionsPageProperties {
    /** The ID of the extension whose permissions are being displayed. */
    public static final WritableObjectPropertyKey<String> EXTENSION_ID =
            new WritableObjectPropertyKey<>();

    /** The name of the extension whose permissions are being displayed. */
    public static final WritableObjectPropertyKey<String> EXTENSION_NAME =
            new WritableObjectPropertyKey<>();

    /** The icon of the extension whose permissions are being displayed. */
    public static final WritableObjectPropertyKey<Bitmap> EXTENSION_ICON =
            new WritableObjectPropertyKey<>();

    /** Click listener for the back button to return to the main menu page. */
    public static final WritableObjectPropertyKey<View.OnClickListener> BACK_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Click listener for the close button to dismiss the menu. */
    public static final WritableObjectPropertyKey<View.OnClickListener> CLOSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Click listener for the "Manage this extension" button. */
    public static final WritableObjectPropertyKey<View.OnClickListener>
            MANAGE_EXTENSION_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    /** Whether the "Show requests in toolbar" toggle is checked. */
    public static final WritableBooleanPropertyKey SHOW_REQUESTS_TOGGLE_CHECKED =
            new WritableBooleanPropertyKey();

    /** Click listener for the "Show requests in toolbar" toggle. */
    public static final WritableObjectPropertyKey<OnCheckedChangeListener>
            SHOW_REQUESTS_TOGGLE_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                EXTENSION_ID,
                EXTENSION_NAME,
                EXTENSION_ICON,
                BACK_CLICK_LISTENER,
                CLOSE_CLICK_LISTENER,
                MANAGE_EXTENSION_CLICK_LISTENER,
                SHOW_REQUESTS_TOGGLE_CHECKED,
                SHOW_REQUESTS_TOGGLE_CLICK_LISTENER
            };
}
