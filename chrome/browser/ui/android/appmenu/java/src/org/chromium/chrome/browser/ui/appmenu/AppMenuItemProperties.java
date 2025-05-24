// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * {@link PropertyKey} list for app menu, most keys are set by {@link AppMenuPropertiesDelegate},
 * but HIGHLIGHTED and CLICK_HANDLER will be set by {@link AppMenuHandler}.
 */
@NullMarked
public class AppMenuItemProperties {
    /** The ID of the menu item. */
    public static final WritableIntPropertyKey MENU_ITEM_ID =
            new WritableIntPropertyKey("MENU_ITEM_ID");

    /** The title of the menu item. */
    public static final WritableObjectPropertyKey<CharSequence> TITLE =
            new WritableObjectPropertyKey<>("TITLE");

    /**
     * The condensed title of the menu item. This is used for View#setContentDescription() for the
     * menu item.
     */
    public static final WritableObjectPropertyKey<CharSequence> TITLE_CONDENSED =
            new WritableObjectPropertyKey<>("TITLE_CONDENSED");

    /** Whether the menu item is enabled. */
    public static final WritableBooleanPropertyKey ENABLED =
            new WritableBooleanPropertyKey("ENABLED");

    /** Whether the menu item is highlighted. */
    public static final WritableBooleanPropertyKey HIGHLIGHTED =
            new WritableBooleanPropertyKey("HIGHLIGHTED");

    /** Whether the menu item is checkable(have a checkbox in the menu item.). */
    public static final WritableBooleanPropertyKey CHECKABLE =
            new WritableBooleanPropertyKey("CHECKABLE");

    /** Whether the menu item is checked. */
    public static final WritableBooleanPropertyKey CHECKED =
            new WritableBooleanPropertyKey("CHECKED");

    /** Whether the menu item is managed by policy. */
    public static final WritableBooleanPropertyKey MANAGED =
            new WritableBooleanPropertyKey("MANAGED");

    /** The icon for the menu item. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>("ICON");

    /** The menu item icon's color resource value. */
    public static final WritableIntPropertyKey ICON_COLOR_RES =
            new WritableIntPropertyKey("ICON_COLOR_RES");

    /** Whether to show a badge on the menu item icon. */
    public static final WritableBooleanPropertyKey ICON_SHOW_BADGE =
            new WritableBooleanPropertyKey("ICON_SHOW_BADGE");

    /** The the menu item's position in the menu. */
    static final WritableIntPropertyKey POSITION = new WritableIntPropertyKey("POSITION");

    /** Whether the menu item support enter animation when menu just opened. */
    public static final WritableBooleanPropertyKey SUPPORT_ENTER_ANIMATION =
            new WritableBooleanPropertyKey("SUPPORT_ENTER_ANIMATION");

    /** The click handler for the menu item. */
    public static final WritableObjectPropertyKey<@Nullable AppMenuClickHandler> CLICK_HANDLER =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true, "CLICK_HANDLER");

    /**
     * Whether the menu is shown from a menu icon positioned at start. This is used to determine the
     * horizontal animation direction of the item.
     */
    public static final WritableBooleanPropertyKey MENU_ICON_AT_START =
            new WritableBooleanPropertyKey("MENU_ICON_AT_START");

    /**
     * Additional icons associated with a particular menu item. Only certain menu item types support
     * additional icons (e.g. icon rows). The number of supported icons also depends on the menu
     * item type.
     */
    public static final WritableObjectPropertyKey<ModelList> ADDITIONAL_ICONS =
            new WritableObjectPropertyKey<>("ADDITIONAL_ICONS");

    /**
     * A generic key for non-standard data types.
     *
     * <p>TODO(crbug.com/40145539): Remove this super hacky key.
     */
    public static final WritableObjectPropertyKey<Object> CUSTOM_ITEM_DATA =
            new WritableObjectPropertyKey<>("CUSTOM_ITEM_DATA");

    public static final PropertyKey[] ALL_ICON_KEYS =
            new PropertyKey[] {
                MENU_ITEM_ID,
                TITLE,
                TITLE_CONDENSED,
                CHECKABLE,
                CHECKED,
                ICON,
                ENABLED,
                HIGHLIGHTED,
                CLICK_HANDLER
            };

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MENU_ITEM_ID,
                TITLE,
                TITLE_CONDENSED,
                ENABLED,
                HIGHLIGHTED,
                CHECKABLE,
                CHECKED,
                MANAGED,
                ICON,
                ICON_COLOR_RES,
                ICON_SHOW_BADGE,
                POSITION,
                SUPPORT_ENTER_ANIMATION,
                CLICK_HANDLER,
                MENU_ICON_AT_START,
                ADDITIONAL_ICONS,
                CUSTOM_ITEM_DATA
            };
}
