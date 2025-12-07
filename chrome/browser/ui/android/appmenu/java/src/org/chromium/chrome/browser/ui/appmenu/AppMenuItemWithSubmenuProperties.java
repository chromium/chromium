// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ENABLED;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.HAS_HOVER_BACKGROUND;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.HIGHLIGHTED;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.HOVER_LISTENER;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON_COLOR_RES;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON_SHOW_BADGE;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.KEY_LISTENER;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MANAGED;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MENU_ICON_AT_START;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.POSITION;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/**
 * {@link PropertyKey} list for app menu, most keys are set by {@link AppMenuPropertiesDelegate},
 * but HIGHLIGHTED and CLICK_HANDLER will be set by {@link AppMenuHandler}.
 */
@NullMarked
public class AppMenuItemWithSubmenuProperties {
    public static final WritableObjectPropertyKey<List<ListItem>> SUBMENU_ITEMS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View.@Nullable OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MENU_ITEM_ID,
                TITLE,
                ENABLED,
                HIGHLIGHTED,
                MANAGED,
                ICON,
                ICON_COLOR_RES,
                ICON_SHOW_BADGE,
                POSITION,
                HOVER_LISTENER,
                KEY_LISTENER,
                HAS_HOVER_BACKGROUND,
                MENU_ICON_AT_START,
                CLICK_LISTENER,
                SUBMENU_ITEMS,
            };
}
