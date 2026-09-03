// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.account_menu;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.account_menu.AccountMenuProperties.MenuItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Account Menu popup items. */
@NullMarked
public class AccountMenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = (TextView) view;
        if (propertyKey == MenuItemProperties.TITLE_ID) {
            textView.setText(model.get(MenuItemProperties.TITLE_ID));
        } else if (propertyKey == MenuItemProperties.START_ICON_ID) {
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    model.get(MenuItemProperties.START_ICON_ID), 0, 0, 0);
        } else if (propertyKey == MenuItemProperties.CLICK_LISTENER) {
            textView.setOnClickListener(model.get(MenuItemProperties.CLICK_LISTENER));
        } else {
            assert false : "Unhandled property key: " + propertyKey;
        }
    }
}
