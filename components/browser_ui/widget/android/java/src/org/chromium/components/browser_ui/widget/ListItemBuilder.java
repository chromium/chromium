// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType.MENU_ITEM;

import android.content.res.Resources;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Builder for creating a {@link ListItem}. */
@NullMarked
public class ListItemBuilder {
    private @StringRes int mTitleRes;
    private @IdRes int mMenuId;
    private @DrawableRes int mStartIconId;
    private @DrawableRes int mEndIconId;
    private boolean mEnabled;
    private @Nullable String mContentDescription;
    private boolean mIsTextEllipsizedAtEnd;
    private boolean mIsIncognito;
    private @ColorRes int mIconTintColorStateList;
    private @StyleRes int mTextAppearanceStyle;
    private @Nullable String mTitle;

    /** Constructs a new builder for a {@link ListItem}. By default, the item is enabled. */
    public ListItemBuilder() {
        mTitleRes = Resources.ID_NULL;
        mMenuId = Resources.ID_NULL;
        mStartIconId = Resources.ID_NULL;
        mEndIconId = Resources.ID_NULL;
        mIconTintColorStateList = Resources.ID_NULL;
        mTextAppearanceStyle = Resources.ID_NULL;

        mEnabled = true;
    }

    /**
     * @param title The text on the menu item. By default, this is set to null.
     */
    public ListItemBuilder setTitle(String title) {
        mTitle = title;
        return this;
    }

    /**
     * @param titleRes The string resource for the menu item. By default, this is set to {@link
     *     Resources#ID_NULL}.
     */
    public ListItemBuilder setTitleRes(@StringRes int titleRes) {
        mTitleRes = titleRes;
        return this;
    }

    /**
     * @param menuId The ID of the menu item. By default, this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder setMenuId(@IdRes int menuId) {
        mMenuId = menuId;
        return this;
    }

    /**
     * @param startIconId The icon on the start of the menu item. Pass 0 for no icon. By default,
     *     this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder setStartIconId(@DrawableRes int startIconId) {
        mStartIconId = startIconId;
        return this;
    }

    /**
     * @param endIconId The icon on the end of the menu item. Pass 0 for no icon. By default, this
     *     is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder setEndIconId(@DrawableRes int endIconId) {
        mEndIconId = endIconId;
        return this;
    }

    /**
     * @param enabled Whether this menu item should be enabled. By default, the item is enabled.
     */
    public ListItemBuilder setEnabled(boolean enabled) {
        mEnabled = enabled;
        return this;
    }

    /**
     * @param contentDescription The accessibility content description of the menu item. By default,
     *     this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder setContentDescription(String contentDescription) {
        mContentDescription = contentDescription;
        return this;
    }

    /**
     * @param isTextEllipsizedAtEnd Whether the text in this menu item is ellipsized at the end. By
     *     default, this is false.
     */
    public ListItemBuilder setIsTextEllipsizedAtEnd(boolean isTextEllipsizedAtEnd) {
        mIsTextEllipsizedAtEnd = isTextEllipsizedAtEnd;
        return this;
    }

    /**
     * @param isIncognito Whether the current menu item will be displayed in incognito mode. By
     *     default, this is false.
     */
    public ListItemBuilder setIsIncognito(boolean isIncognito) {
        mIsIncognito = isIncognito;
        return this;
    }

    /**
     * @param iconTintColorStateList The tint color for the icon. By default, this is set to {@link
     *     Resources#ID_NULL}.
     */
    public ListItemBuilder setIconTintColorStateList(@ColorRes int iconTintColorStateList) {
        mIconTintColorStateList = iconTintColorStateList;
        return this;
    }

    /**
     * @param textAppearanceStyle The appearance of the text in the menu item. By default, this is
     *     set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder setTextAppearanceStyle(@StyleRes int textAppearanceStyle) {
        mTextAppearanceStyle = textAppearanceStyle;
        return this;
    }

    /** Builds the {@link ListItem} with the specified properties. */
    public ListItem build() {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                BrowserUiListMenuUtils.getDefaultIconTintColorStateListId())
                        .with(
                                ListMenuItemProperties.TEXT_APPEARANCE_ID,
                                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle());

        if (mTitle != null) {
            builder.with(ListMenuItemProperties.TITLE, mTitle);
        } else {
            builder.with(ListMenuItemProperties.TITLE_ID, mTitleRes);
        }

        builder.with(ListMenuItemProperties.MENU_ITEM_ID, mMenuId)
                .with(ListMenuItemProperties.START_ICON_ID, mStartIconId)
                .with(ListMenuItemProperties.END_ICON_ID, mEndIconId)
                .with(ListMenuItemProperties.ENABLED, mEnabled);

        if (mContentDescription != null) {
            builder.with(ListMenuItemProperties.CONTENT_DESCRIPTION, mContentDescription);
        }

        if (mIsTextEllipsizedAtEnd) {
            builder.with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, true);
        }

        if (mIsIncognito) {
            builder.with(
                            ListMenuItemProperties.TEXT_APPEARANCE_ID,
                            mTextAppearanceStyle != Resources.ID_NULL
                                    ? mTextAppearanceStyle
                                    : R.style.TextAppearance_TextLarge_Primary_Baseline_Light)
                    .with(
                            ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                            mIconTintColorStateList != Resources.ID_NULL
                                    ? mIconTintColorStateList
                                    : R.color.default_icon_color_light_tint_list);
        }

        return new ListItem(MENU_ITEM, builder.build());
    }
}
