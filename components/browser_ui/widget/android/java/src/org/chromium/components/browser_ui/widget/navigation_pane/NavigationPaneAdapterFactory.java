// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.navigation_pane;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Factory for creating adapters for the navigation pane. */
@NullMarked
public class NavigationPaneAdapterFactory {
    /** Creates a SimpleRecyclerViewAdapter customized for the navigation pane items. */
    public static SimpleRecyclerViewAdapter createAdapter(Context context, ModelList modelList) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);

        adapter.registerType(
                NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM,
                parent ->
                        LayoutInflater.from(context)
                                .inflate(R.layout.navigation_pane_item, parent, false),
                NavigationPaneViewBinder::bindNavigationItem);

        adapter.registerType(
                NavigationPaneProperties.ITEM_TYPE_HEADER,
                parent ->
                        LayoutInflater.from(context)
                                .inflate(R.layout.navigation_pane_header, parent, false),
                NavigationPaneViewBinder::bindHeader);

        adapter.registerType(
                NavigationPaneProperties.ITEM_TYPE_DIVIDER,
                parent ->
                        LayoutInflater.from(context)
                                .inflate(R.layout.navigation_pane_divider, parent, false),
                (model, view, propertyKey) -> {});

        return adapter;
    }
}
