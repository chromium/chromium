// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageView;
import android.widget.TextView;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Class responsible for binding the site permissions page properties to its view. */
@NullMarked
public class SitePermissionsPageViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == SitePermissionsPageProperties.BACK_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_back_button)
                    .setOnClickListener(
                            model.get(SitePermissionsPageProperties.BACK_CLICK_LISTENER));
        } else if (key == SitePermissionsPageProperties.CLOSE_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_site_permissions_close_button)
                    .setOnClickListener(
                            model.get(SitePermissionsPageProperties.CLOSE_CLICK_LISTENER));
        } else if (key == SitePermissionsPageProperties.EXTENSION_NAME) {
            TextView nameView = view.findViewById(R.id.extensions_menu_extension_name);
            nameView.setText(model.get(SitePermissionsPageProperties.EXTENSION_NAME));
        } else if (key == SitePermissionsPageProperties.EXTENSION_ICON) {
            ImageView iconView = view.findViewById(R.id.extensions_menu_extension_icon);
            iconView.setImageBitmap(model.get(SitePermissionsPageProperties.EXTENSION_ICON));
        } else if (key == SitePermissionsPageProperties.MANAGE_EXTENSION_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_manage_this_extension)
                    .setOnClickListener(
                            model.get(
                                    SitePermissionsPageProperties.MANAGE_EXTENSION_CLICK_LISTENER));
        } else if (key == SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CHECKED) {
            MaterialSwitch toggle = view.findViewById(R.id.extensions_menu_show_requests_toggle);
            toggle.setChecked(
                    model.get(SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CHECKED));
        } else if (key == SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CLICK_LISTENER) {
            View toggleContainer =
                    view.findViewById(R.id.extensions_menu_show_requests_toggle_container);
            MaterialSwitch toggle = view.findViewById(R.id.extensions_menu_show_requests_toggle);
            OnCheckedChangeListener listener =
                    model.get(SitePermissionsPageProperties.SHOW_REQUESTS_TOGGLE_CLICK_LISTENER);
            toggleContainer.setOnClickListener(
                    v -> {
                        toggle.toggle();
                        if (listener != null) {
                            listener.onCheckedChanged(toggle, toggle.isChecked());
                        }
                    });
        }
    }
}
