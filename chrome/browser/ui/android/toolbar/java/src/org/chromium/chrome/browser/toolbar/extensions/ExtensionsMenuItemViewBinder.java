// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.extensions.ExtensionsMenuTypes.ControlState.Status;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding an extension item's (icon and title) properties to its view. This
 * binder is designed for the simplified R.layout.extensions_menu_item layout.
 */
@NullMarked
public class ExtensionsMenuItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == ExtensionsMenuItemProperties.TITLE) {
            TextView titleView = view.findViewById(R.id.extensions_menu_item_title);
            titleView.setText(model.get(ExtensionsMenuItemProperties.TITLE));
        } else if (key == ExtensionsMenuItemProperties.ICON) {
            ImageView iconView = view.findViewById(R.id.extensions_menu_item_icon);
            @Nullable Bitmap bitmap = model.get(ExtensionsMenuItemProperties.ICON);
            iconView.setImageBitmap(bitmap);
        } else if (key == ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ICON) {
            ImageView contextMenuView = view.findViewById(R.id.extensions_menu_item_context_menu);
            contextMenuView.setImageResource(
                    model.get(ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ICON));
        } else if (key == ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ON_CLICK) {
            view.findViewById(R.id.extensions_menu_item_context_menu)
                    .setOnClickListener(
                            model.get(ExtensionsMenuItemProperties.CONTEXT_MENU_BUTTON_ON_CLICK));
        } else if (key == ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_CHECKED) {
            MaterialSwitchWithText toggle = getMenuItemToggle(view);
            // Null out the listener before calling setChecked() to avoid triggering programmatic
            // changes during view binding (e.g. when views are recycled). The listener is restored
            // immediately after to ensure only user-initiated changes trigger the callback.
            toggle.setOnCheckedChangeListener(null);
            toggle.setChecked(model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_CHECKED));
            toggle.setOnCheckedChangeListener(
                    model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_ON_CLICK));
        } else if (key == ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_ON_CLICK) {
            getMenuItemToggle(view)
                    .setOnCheckedChangeListener(
                            model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_ON_CLICK));
        } else if (key == ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS) {
            @Status int status = model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_STATUS);
            MaterialSwitchWithText toggle = getMenuItemToggle(view);
            toggle.setVisibility(status == Status.HIDDEN ? View.GONE : View.VISIBLE);
            toggle.setEnabled(status == Status.ENABLED);
        } else if (key == ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_TOOLTIP) {
            getMenuItemToggle(view)
                    .setTooltipText(
                            model.get(ExtensionsMenuItemProperties.SITE_ACCESS_TOGGLE_TOOLTIP));
        } else if (key == ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME) {
            getMenuItemSitePermissionsButton(view)
                    .setContentDescription(
                            model.get(
                                    ExtensionsMenuItemProperties
                                            .SITE_PERMISSIONS_BUTTON_ACCESSIBLE_NAME));
        } else if (key == ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK) {
            // Set the listener to handle navigation to the site permissions page.
            getMenuItemSitePermissionsButton(view)
                    .setOnClickListener(
                            model.get(
                                    ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_ON_CLICK));
        } else if (key == ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS) {
            @Status
            int status = model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_STATUS);
            TextView button = getMenuItemSitePermissionsButton(view);
            button.setVisibility(status == Status.HIDDEN ? View.GONE : View.VISIBLE);
            button.setEnabled(status == Status.ENABLED);
        } else if (key == ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TEXT) {
            getMenuItemSitePermissionsButton(view)
                    .setText(model.get(ExtensionsMenuItemProperties.SITE_PERMISSIONS_BUTTON_TEXT));
        }
    }

    private static TextView getMenuItemSitePermissionsButton(View view) {
        return view.findViewById(R.id.extensions_menu_item_site_permissions_button);
    }

    private static MaterialSwitchWithText getMenuItemToggle(View view) {
        return view.findViewById(R.id.extensions_menu_item_toggle);
    }
}
