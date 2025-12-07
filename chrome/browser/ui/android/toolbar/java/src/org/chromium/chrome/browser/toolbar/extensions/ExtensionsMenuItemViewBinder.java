// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.R;
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
            Bitmap bitmap = model.get(ExtensionsMenuItemProperties.ICON);
            // TODO: Investigate the correct resizing method.
            bitmap.setDensity(120);
            iconView.setImageBitmap(bitmap);
        } else if (key == ExtensionsMenuItemProperties.CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_item_context_menu)
                    .setOnClickListener(model.get(ExtensionsMenuItemProperties.CLICK_LISTENER));
        }
    }
}
