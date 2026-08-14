// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.navigation_pane;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the generic desktop navigation pane. */
@NullMarked
public class NavigationPaneViewBinder {
    public static void bindNavigationItem(PropertyModel model, View view, PropertyKey propertyKey) {
        NavigationPaneItemView itemView = (NavigationPaneItemView) view;

        if (NavigationPaneProperties.TITLE == propertyKey) {
            itemView.setTitle(model.get(NavigationPaneProperties.TITLE));
        } else if (NavigationPaneProperties.ICON == propertyKey) {
            itemView.setIcon(model.get(NavigationPaneProperties.ICON));
        } else if (NavigationPaneProperties.IS_SELECTED == propertyKey) {
            itemView.setSelected(model.get(NavigationPaneProperties.IS_SELECTED));
        } else if (NavigationPaneProperties.ON_CLICK_HANDLER == propertyKey) {
            Runnable handler = model.get(NavigationPaneProperties.ON_CLICK_HANDLER);
            itemView.setOnClickListener(handler == null ? null : v -> handler.run());
            itemView.setClickable(handler != null);
            itemView.setFocusable(handler != null);
        } else if (NavigationPaneProperties.CONTENT_DESCRIPTION == propertyKey) {
            itemView.setContentDescription(model.get(NavigationPaneProperties.CONTENT_DESCRIPTION));
        }
    }

    public static void bindHeader(PropertyModel model, View view, PropertyKey propertyKey) {
        NavigationPaneHeaderView headerView = (NavigationPaneHeaderView) view;
        if (NavigationPaneProperties.HEADER_TITLE == propertyKey) {
            headerView.setTitle(model.get(NavigationPaneProperties.HEADER_TITLE));
        }
    }
}
