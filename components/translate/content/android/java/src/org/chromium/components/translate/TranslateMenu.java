// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import java.util.ArrayList;
import java.util.List;

/**
 * Translate menu config and its item entity definition.
 */
public final class TranslateMenu {
    /**
     * The menu item entity.
     */
    static final class MenuItem {
        public final int mType;
        public final int mId;
        public final String mCode;
        public final boolean mWithDivider;

        MenuItem(int itemType, int itemId, boolean withDivider) {
            this(itemType, itemId, EMPTY_STRING, withDivider);
        }

        MenuItem(int itemType, int itemId, String code) {
            this(itemType, itemId, code, false);
        }

        MenuItem(int itemType, int itemId, String code, boolean withDivider) {
            mType = itemType;
            mId = itemId;
            mCode = code;
            mWithDivider = withDivider;
        }
    }

    public static final String EMPTY_STRING = "";

    // Menu type config.
    public static final int MENU_OVERFLOW = 0;
    public static final int MENU_TARGET_LANGUAGE = 1;
    public static final int MENU_SOURCE_LANGUAGE = 2;

    // Menu item type config.
    public static final int ITEM_LANGUAGE = 0;
    public static final int ITEM_CHECKBOX_OPTION = 1;
    public static final int ITEM_CONTENT_LANGUAGE = 2;
    public static final int MENU_ITEM_TYPE_COUNT = 3;

    // Menu Item ID config for MENU_OVERFLOW.
    public static final int ID_OVERFLOW_MORE_LANGUAGE = 0;
    public static final int ID_OVERFLOW_ALWAYS_TRANSLATE = 1;
    public static final int ID_OVERFLOW_NEVER_SITE = 2;
    public static final int ID_OVERFLOW_NEVER_LANGUAGE = 3;
    public static final int ID_OVERFLOW_NOT_THIS_LANGUAGE = 4;

    /**
     * Build overflow menu item list.
     */
    static List<MenuItem> getOverflowMenu(boolean isIncognito, boolean isSourceLangUnknown) {
        List<MenuItem> menu = new ArrayList<MenuItem>();
        menu.add(new MenuItem(ITEM_CHECKBOX_OPTION, ID_OVERFLOW_MORE_LANGUAGE, true));
        // The "Always translate" and "Never translate" language options are hidden when the
        // source language is unknown.
        if (!isIncognito && !isSourceLangUnknown) {
            // "Always translate" does nothing in incognito mode, so just hide it.
            menu.add(new MenuItem(ITEM_CHECKBOX_OPTION, ID_OVERFLOW_ALWAYS_TRANSLATE, false));
        }
        if (!isSourceLangUnknown) {
            menu.add(new MenuItem(ITEM_CHECKBOX_OPTION, ID_OVERFLOW_NEVER_LANGUAGE, false));
        }
        menu.add(new MenuItem(ITEM_CHECKBOX_OPTION, ID_OVERFLOW_NEVER_SITE, false));
        menu.add(new MenuItem(ITEM_CHECKBOX_OPTION, ID_OVERFLOW_NOT_THIS_LANGUAGE, false));
        return menu;
    }

    private TranslateMenu() {}
}
