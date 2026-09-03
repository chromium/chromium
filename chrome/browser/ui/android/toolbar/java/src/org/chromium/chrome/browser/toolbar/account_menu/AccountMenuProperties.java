// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.view.View.OnClickListener;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties for the Account Menu popup. */
@NullMarked
public class AccountMenuProperties {
    private AccountMenuProperties() {}

    /** Item types supported by the Account Menu RecyclerView. */
    @IntDef({ItemType.MENU_ITEM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        int MENU_ITEM = 0;
    }

    /** Properties for menu items in the Account Menu. */
    public static class MenuItemProperties {
        /** String resource id for the item title. */
        public static final WritableIntPropertyKey TITLE_ID = new WritableIntPropertyKey();

        /** Drawable resource id for the item start icon. */
        public static final WritableIntPropertyKey START_ICON_ID = new WritableIntPropertyKey();

        /** Click listener for the item. */
        public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = {TITLE_ID, START_ICON_ID, CLICK_LISTENER};

        private MenuItemProperties() {}

        /** Factory helper to create a MenuItem PropertyModel. */
        public static PropertyModel createModel(
                @StringRes int titleId, @DrawableRes int iconId, OnClickListener clickListener) {
            return new PropertyModel.Builder(ALL_KEYS)
                    .with(TITLE_ID, titleId)
                    .with(START_ICON_ID, iconId)
                    .with(CLICK_LISTENER, clickListener)
                    .build();
        }
    }
}
