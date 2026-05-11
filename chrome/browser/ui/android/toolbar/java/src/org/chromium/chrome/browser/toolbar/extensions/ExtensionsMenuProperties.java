// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View.OnClickListener;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes.HostAccessRequest;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

@NullMarked
class ExtensionsMenuProperties {
    /** Page types for the extensions menu. */
    @IntDef({Page.MAIN, Page.SITE_PERMISSIONS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Page {
        /** The main page displaying the list of all extensions. */
        int MAIN = 0;

        /** The site permissions page for a specific extension. */
        int SITE_PERMISSIONS = 1;
    }

    public static final WritableObjectPropertyKey<Callback<String>> ALLOW_EXTENSION_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<OnClickListener> CLOSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The current page being displayed in the extensions menu. */
    public static final WritableIntPropertyKey CURRENT_PAGE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            DISCOVER_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey DISCOVER_EXTENSIONS_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Callback<String>>
            DISMISS_EXTENSION_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<HostAccessRequest>> HOST_ACCESS_REQUESTS =
            new WritableObjectPropertyKey<>();

    /**
     * Whether the menu should display the 'zero state' when there are no actions to be shown in the
     * menu.
     */
    public static final WritableBooleanPropertyKey IS_ZERO_STATE =
            new WritableBooleanPropertyKey("IS_ZERO_STATE");

    public static final WritableObjectPropertyKey<OnClickListener>
            MANAGE_EXTENSIONS_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    /** Properties for the pinning extensions menu button in the toolbar (puzzle piece). */
    public static final WritableBooleanPropertyKey MENU_BUTTON_PINNED =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener>
            MENU_BUTTON_PINNING_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey OPTIONAL_SECTION_TYPE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<OnClickListener> RELOAD_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the entire site settings section is visible in the menu. */
    public static final WritableBooleanPropertyKey SITE_SETTINGS_CONTAINER_VISIBLE =
            new WritableBooleanPropertyKey();

    /**
     * Properties for the site settings toggle that allows users to block or allow all extensions
     * for the current site.
     */
    public static final WritableBooleanPropertyKey SITE_SETTINGS_INFO_ICON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<String> SITE_SETTINGS_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey SITE_SETTINGS_TOGGLE_CHECKED =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<OnCheckedChangeListener>
            SITE_SETTINGS_TOGGLE_CLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> SITE_SETTINGS_TOGGLE_TOOLTIP =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey SITE_SETTINGS_TOGGLE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALLOW_EXTENSION_CLICK_LISTENER,
                CLOSE_CLICK_LISTENER,
                CURRENT_PAGE,
                DISCOVER_EXTENSIONS_CLICK_LISTENER,
                DISCOVER_EXTENSIONS_VISIBLE,
                DISMISS_EXTENSION_CLICK_LISTENER,
                HOST_ACCESS_REQUESTS,
                IS_ZERO_STATE,
                MANAGE_EXTENSIONS_CLICK_LISTENER,
                MENU_BUTTON_PINNED,
                MENU_BUTTON_PINNING_CLICK_LISTENER,
                OPTIONAL_SECTION_TYPE,
                RELOAD_CLICK_LISTENER,
                SITE_SETTINGS_CONTAINER_VISIBLE,
                SITE_SETTINGS_INFO_ICON_VISIBLE,
                SITE_SETTINGS_LABEL,
                SITE_SETTINGS_TOGGLE_CHECKED,
                SITE_SETTINGS_TOGGLE_CLICK_LISTENER,
                SITE_SETTINGS_TOGGLE_TOOLTIP,
                SITE_SETTINGS_TOGGLE_VISIBLE
            };
}
