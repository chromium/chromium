// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;

import org.chromium.components.translate.TranslateMessage.MenuItem;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

final class TranslateMessageSecondaryMenuAdapter extends BaseAdapter {
    @IntDef({
        ViewType.DIVIDER,
        ViewType.MENU_ITEM,
        ViewType.MENU_ITEM_WITH_SUBTITLE,
        ViewType.MENU_ITEM_WITH_CHECKMARK,
        ViewType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ViewType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
        int MENU_ITEM_WITH_SUBTITLE = 2;
        int MENU_ITEM_WITH_CHECKMARK = 3;
        int NUM_ENTRIES = 4;
    }

    private final LayoutInflater mInflater;
    private MenuItem[] mMenuItems;

    public TranslateMessageSecondaryMenuAdapter(Context context, MenuItem[] menuItems) {
        mInflater = LayoutInflater.from(context);
        mMenuItems = menuItems;
    }

    public void setMenuItems(MenuItem[] menuItems) {
        mMenuItems = menuItems;
        notifyDataSetChanged();
    }

    @Override
    public boolean isEnabled(int position) {
        return getItemViewType(position) != ViewType.DIVIDER;
    }

    @Override
    public boolean areAllItemsEnabled() {
        for (int position = 0; position < getCount(); ++position) {
            if (!isEnabled(position)) return false;
        }
        return true;
    }

    @Override
    public int getItemViewType(int position) {
        MenuItem item = mMenuItems[position];
        if (item.title.equals("")) return ViewType.DIVIDER;
        if (!item.subtitle.equals("")) {
            // Currently, menu items with both a subtitle and a checkmark aren't supported.
            assert !item.hasCheckmark;
            return ViewType.MENU_ITEM_WITH_SUBTITLE;
        }
        if (item.hasCheckmark) return ViewType.MENU_ITEM_WITH_CHECKMARK;
        return ViewType.MENU_ITEM;
    }

    @Override
    public int getCount() {
        return mMenuItems == null ? 0 : mMenuItems.length;
    }

    @Override
    public int getViewTypeCount() {
        return ViewType.NUM_ENTRIES;
    }

    @Override
    public boolean hasStableIds() {
        return false;
    }

    @Override
    public boolean isEmpty() {
        return getCount() == 0;
    }

    @Override
    public Object getItem(int position) {
        return mMenuItems[position];
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        switch (getItemViewType(position)) {
            case ViewType.DIVIDER:
                convertView =
                        reuseOrCreateView(
                                convertView,
                                ViewType.DIVIDER,
                                R.layout.list_section_divider,
                                parent);
                break;

            case ViewType.MENU_ITEM:
                convertView =
                        reuseOrCreateView(
                                convertView,
                                ViewType.MENU_ITEM,
                                R.layout.translate_menu_item,
                                parent);
                ((TextView) convertView.findViewById(R.id.menu_item_text))
                        .setText(mMenuItems[position].title);
                break;

            case ViewType.MENU_ITEM_WITH_SUBTITLE:
                convertView =
                        reuseOrCreateView(
                                convertView,
                                ViewType.MENU_ITEM_WITH_SUBTITLE,
                                R.layout.translate_menu_extended_item,
                                parent);
                ((TextView) convertView.findViewById(R.id.menu_item_text))
                        .setText(mMenuItems[position].title);
                ((TextView) convertView.findViewById(R.id.menu_item_secondary_text))
                        .setText(mMenuItems[position].subtitle);
                break;

            case ViewType.MENU_ITEM_WITH_CHECKMARK:
                convertView =
                        reuseOrCreateView(
                                convertView,
                                ViewType.MENU_ITEM_WITH_CHECKMARK,
                                R.layout.translate_menu_item_checked,
                                parent);
                ((TextView) convertView.findViewById(R.id.menu_item_text))
                        .setText(mMenuItems[position].title);
                break;

            default:
                assert false;
                break;
        }
        return convertView;
    }

    private View reuseOrCreateView(
            View view,
            @ViewType int desiredViewType,
            @LayoutRes int layoutResourceId,
            ViewGroup parent) {
        if (canReuseView(view, desiredViewType)) return view;
        view = mInflater.inflate(layoutResourceId, parent, false);
        view.setTag(R.id.view_type, desiredViewType);
        return view;
    }

    private static boolean canReuseView(View view, @ViewType int desiredViewType) {
        return view != null
                && view.getTag(R.id.view_type) != null
                && (int) view.getTag(R.id.view_type) == desiredViewType;
    }
}
