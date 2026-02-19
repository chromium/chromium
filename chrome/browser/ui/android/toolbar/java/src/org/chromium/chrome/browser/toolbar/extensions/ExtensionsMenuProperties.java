// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
class ExtensionsMenuProperties {
    /**
     * Whether the menu should display the 'zero state' when there are no actions to be shown in the
     * menu.
     */
    public static final WritableBooleanPropertyKey IS_ZERO_STATE =
            new WritableBooleanPropertyKey("IS_ZERO_STATE");

    public static final WritableObjectPropertyKey<View.OnClickListener> CLOSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            DISCOVER_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            MANAGE_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    /**
     * Properties for the site settings toggle that allows users to block or allow all extensions
     * for the current site.
     */
    public static final WritableBooleanPropertyKey SITE_SETTINGS_TOGGLE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SITE_SETTINGS_TOGGLE_CHECKED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<
                    android.widget.CompoundButton.OnCheckedChangeListener>
            SITE_SETTINGS_TOGGLE_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SITE_SETTINGS_LABEL =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CLOSE_CLICK_LISTENER,
                DISCOVER_EXTENSIONS_CLICK_LISTENER,
                IS_ZERO_STATE,
                MANAGE_EXTENSIONS_CLICK_LISTENER,
                SITE_SETTINGS_LABEL,
                SITE_SETTINGS_TOGGLE_VISIBLE,
                SITE_SETTINGS_TOGGLE_CHECKED,
                SITE_SETTINGS_TOGGLE_CLICK_LISTENER
            };
}
