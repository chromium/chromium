// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter;

/** Collection of utility methods related to the browser UI widgets. */
public class BrowserUiWidgetUtils {

    /**
     * Convenience method for constructing a {@link BasicListMenu} with the preferred content view.
     */
    @NonNull
    public static BasicListMenu getBasicListMenu(
            @NonNull Context context,
            @NonNull MVCListAdapter.ModelList data,
            @Nullable ListMenu.Delegate delegate) {
        return getBasicListMenu(context, data, delegate, 0);
    }

    /**
     * Convenience method for constructing a {@link BasicListMenu} with the preferred content view.
     */
    @NonNull
    public static BasicListMenu getBasicListMenu(
            @NonNull Context context,
            @NonNull MVCListAdapter.ModelList data,
            @Nullable ListMenu.Delegate delegate,
            @ColorRes int backgroundTintColor) {
        View contentView = LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        ListView listView = contentView.findViewById(R.id.app_menu_list);
        return new BasicListMenu(
                context, data, contentView, listView, delegate, backgroundTintColor);
    }
}
