// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Builder for creating a {@link ListItem}. */
@NullMarked
public class ListItemBuilder {
    private @StringRes int mTitleRes;
    private @IdRes int mMenuId;
    private @Nullable Bitmap mStartIconBitmap;
    private @Nullable Drawable mStartIconDrawable;
    private @DrawableRes int mStartIconRes;
    private @DrawableRes int mEndIconRes;
    private boolean mEnabled;
    private @Nullable OnClickListener mClickListener;
    private @Nullable String mContentDescription;
    private boolean mIsTextEllipsizedAtEnd;
    private boolean mIsIncognito;
    private boolean mShouldTintIcon;
    private @ColorRes int mIconTintColorStateList;
    private int mStartIconWidth;
    private int mEndIconWidth;
    private @Nullable List<ListItem> mSubmenuItems;
    private @StyleRes int mTextAppearanceStyle;
    private @Nullable String mTitle;

    /** Constructs a new builder for a {@link ListItem}. By default, the item is enabled. */
    public ListItemBuilder() {
        mTitleRes = Resources.ID_NULL;
        mMenuId = Resources.ID_NULL;
        mStartIconRes = Resources.ID_NULL;
        mEndIconRes = Resources.ID_NULL;
        mIconTintColorStateList = Resources.ID_NULL;
        mTextAppearanceStyle = Resources.ID_NULL;

        mEnabled = true;
        mShouldTintIcon = true;
    }

    /**
     * @param title The text on the menu item. By default, this is set to null.
     */
    public ListItemBuilder withTitle(String title) {
        mTitle = title;
        return this;
    }

    /**
     * @param titleRes The string resource for the menu item. By default, this is set to {@link
     *     Resources#ID_NULL}.
     */
    public ListItemBuilder withTitleRes(@StringRes int titleRes) {
        mTitleRes = titleRes;
        return this;
    }

    /**
     * @param menuId The ID of the menu item. By default, this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder withMenuId(@IdRes int menuId) {
        mMenuId = menuId;
        return this;
    }

    /**
     * @param startIconBitmap The icon on the start of the menu item. Pass null for no icon.
     */
    public ListItemBuilder withStartIconBitmap(@Nullable Bitmap startIconBitmap) {
        mStartIconBitmap = startIconBitmap;
        return this;
    }

    /**
     * @param startIconDrawable The icon on the start of the menu item. Pass null for no icon.
     */
    public ListItemBuilder withStartIconDrawable(@Nullable Drawable startIconDrawable) {
        mStartIconDrawable = startIconDrawable;
        return this;
    }

    /**
     * @param shouldTintIcon Whether the icon should be tinted. By default, icons are tinted.
     */
    public ListItemBuilder withShouldTintIcon(boolean shouldTintIcon) {
        mShouldTintIcon = shouldTintIcon;
        return this;
    }

    /**
     * @param startIconRes The icon on the start of the menu item. Pass 0 for no icon. By default,
     *     this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder withStartIconRes(@DrawableRes int startIconRes) {
        mStartIconRes = startIconRes;
        return this;
    }

    /**
     * @param endIconRes The icon on the end of the menu item. Pass 0 for no icon. By default, this
     *     is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder withEndIconRes(@DrawableRes int endIconRes) {
        mEndIconRes = endIconRes;
        return this;
    }

    /**
     * @param enabled Whether this menu item should be enabled. By default, the item is enabled.
     */
    public ListItemBuilder withEnabled(boolean enabled) {
        mEnabled = enabled;
        return this;
    }

    /**
     * @param clickListener The {@link OnClickListener} fired when the item is clicked.
     */
    public ListItemBuilder withClickListener(OnClickListener clickListener) {
        mClickListener = clickListener;
        return this;
    }

    /**
     * @param contentDescription The accessibility content description of the menu item. By default,
     *     this is set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder withContentDescription(String contentDescription) {
        mContentDescription = contentDescription;
        return this;
    }

    /**
     * @param isTextEllipsizedAtEnd Whether the text in this menu item is ellipsized at the end. By
     *     default, this is false.
     */
    public ListItemBuilder withIsTextEllipsizedAtEnd(boolean isTextEllipsizedAtEnd) {
        mIsTextEllipsizedAtEnd = isTextEllipsizedAtEnd;
        return this;
    }

    /**
     * @param isIncognito Whether the current menu item will be displayed in incognito mode. By
     *     default, this is false.
     */
    public ListItemBuilder withIsIncognito(boolean isIncognito) {
        mIsIncognito = isIncognito;
        return this;
    }

    /**
     * @param iconTintColorStateList The tint color for the icon. By default, this is set to {@link
     *     Resources#ID_NULL}, which results in "default tinting" being applied.
     */
    public ListItemBuilder withIconTintColorStateList(@ColorRes int iconTintColorStateList) {
        mIconTintColorStateList = iconTintColorStateList;
        return this;
    }

    /**
     * @param startIconWidth The width for the start icon.
     */
    public ListItemBuilder withStartIconWidth(int startIconWidth) {
        mStartIconWidth = startIconWidth;
        return this;
    }

    /**
     * @param endIconWidth The width for the end icon.
     */
    public ListItemBuilder withEndIconWidth(int endIconWidth) {
        mEndIconWidth = endIconWidth;
        return this;
    }

    /**
     * @param submenuItems The submenu items that are children of this item.
     */
    public ListItemBuilder withSubmenuItems(List<ListItem> submenuItems) {
        mSubmenuItems = submenuItems;
        return this;
    }

    /**
     * @param textAppearanceStyle The appearance of the text in the menu item. By default, this is
     *     set to {@link Resources#ID_NULL}.
     */
    public ListItemBuilder withTextAppearanceStyle(@StyleRes int textAppearanceStyle) {
        mTextAppearanceStyle = textAppearanceStyle;
        return this;
    }

    /** Builds the {@link ListItem} with the specified properties. */
    public ListItem build() {
        boolean hasSubmenu = mSubmenuItems != null;
        PropertyModel.Builder builder =
                (hasSubmenu
                                ? new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                : new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS))
                        .with(
                                ListMenuItemProperties.TEXT_APPEARANCE_ID,
                                mTextAppearanceStyle != Resources.ID_NULL
                                        ? mTextAppearanceStyle
                                        : BrowserUiListMenuUtils.getDefaultTextAppearanceStyle());

        if (mTitle != null) {
            builder.with(ListMenuItemProperties.TITLE, mTitle);
        } else if (!hasSubmenu) {
            builder.with(ListMenuItemProperties.TITLE_ID, mTitleRes);
        }

        builder.with(ListMenuItemProperties.ENABLED, mEnabled);

        if (!hasSubmenu) {
            builder.with(ListMenuItemProperties.MENU_ITEM_ID, mMenuId)
                    .with(ListMenuItemProperties.START_ICON_ID, mStartIconRes)
                    .with(ListMenuItemProperties.END_ICON_ID, mEndIconRes);
        } else {
            builder.with(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER, () -> mSubmenuItems);
        }

        if (mShouldTintIcon) {
            builder.with(
                    ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                    mIconTintColorStateList != Resources.ID_NULL
                            ? mIconTintColorStateList
                            : BrowserUiListMenuUtils.getDefaultIconTintColorStateListId());
        }

        if (mStartIconBitmap != null) {
            builder.with(ListMenuItemProperties.START_ICON_BITMAP, mStartIconBitmap);
        }

        if (mStartIconDrawable != null) {
            builder.with(ListMenuItemProperties.START_ICON_DRAWABLE, mStartIconDrawable);
        }

        if (mStartIconWidth != 0) {
            builder.with(ListMenuItemProperties.START_ICON_WIDTH, mStartIconWidth);
        }

        if (mEndIconWidth != 0) {
            builder.with(ListMenuItemProperties.END_ICON_WIDTH, mEndIconWidth);
        }

        if (mClickListener != null) {
            builder.with(ListMenuItemProperties.CLICK_LISTENER, mClickListener);
        }

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
                            : R.style
                                    .TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light);
            if (mShouldTintIcon) {
                builder.with(
                        ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                        mIconTintColorStateList != Resources.ID_NULL
                                ? mIconTintColorStateList
                                : R.color.default_icon_color_light_tint_list);
            }
        }

        return hasSubmenu
                ? new ListItem(MENU_ITEM_WITH_SUBMENU, builder.build())
                : new ListItem(MENU_ITEM, builder.build());
    }

    /**
     * Builds a {@link ListItem} with the provided title.
     *
     * @param titleRes The string resource for the menu item.
     */
    public static ListItem buildSimpleMenuItem(@StringRes int titleRes) {
        return new ListItemBuilder().withTitleRes(titleRes).build();
    }
}
