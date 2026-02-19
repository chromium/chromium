// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class ExtensionsMenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == ExtensionsMenuProperties.CLOSE_CLICK_LISTENER) {
            View closeButton = view.findViewById(R.id.extensions_menu_close_button);
            closeButton.setOnClickListener(
                    model.get(ExtensionsMenuProperties.CLOSE_CLICK_LISTENER));
            ViewCompat.setTooltipText(closeButton, view.getContext().getString(R.string.close));
        } else if (key == ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_discover_extensions_button)
                    .setOnClickListener(
                            model.get(ExtensionsMenuProperties.DISCOVER_EXTENSIONS_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.IS_ZERO_STATE) {
            boolean isZeroState = model.get(ExtensionsMenuProperties.IS_ZERO_STATE);
            View listView = view.findViewById(R.id.extensions_menu_items);
            View zeroStateView = view.findViewById(R.id.extensions_menu_zero_state);

            if (isZeroState) {
                listView.setVisibility(View.GONE);
                zeroStateView.setVisibility(View.VISIBLE);
            } else {
                listView.setVisibility(View.VISIBLE);
                zeroStateView.setVisibility(View.GONE);
            }
        } else if (key == ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_manage_extensions_button)
                    .setOnClickListener(
                            model.get(ExtensionsMenuProperties.MANAGE_EXTENSIONS_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE) {
            view.findViewById(R.id.extensions_menu_site_settings_toggle)
                    .setVisibility(
                            model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_VISIBLE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setChecked(model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CHECKED));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setOnCheckedChangeListener(
                    model.get(ExtensionsMenuProperties.SITE_SETTINGS_TOGGLE_CLICK_LISTENER));
        } else if (key == ExtensionsMenuProperties.SITE_SETTINGS_LABEL) {
            MaterialSwitchWithText toggle =
                    view.findViewById(R.id.extensions_menu_site_settings_toggle);
            toggle.setText(model.get(ExtensionsMenuProperties.SITE_SETTINGS_LABEL));
        }
    }
}
