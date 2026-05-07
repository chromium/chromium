// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds changed {@link PropertyKey} in the {@link PropertyModel} to the actual view of the
 * extension action button.
 */
@NullMarked
public class ExtensionActionButtonViewBinder {
    @SuppressLint("ClickableViewAccessibility")
    public static void bind(PropertyModel model, ListMenuButton button, PropertyKey key) {
        if (key == ExtensionActionButtonProperties.ACCESSIBLE_NAME) {
            String accessibleName = model.get(ExtensionActionButtonProperties.ACCESSIBLE_NAME);
            button.setContentDescription(accessibleName);
        } else if (key == ExtensionActionButtonProperties.ICON) {
            Bitmap bitmap = model.get(ExtensionActionButtonProperties.ICON);
            button.setImageBitmap(bitmap);
        } else if (key == ExtensionActionButtonProperties.TOUCH_LISTENER) {
            button.setOnTouchListener(model.get(ExtensionActionButtonProperties.TOUCH_LISTENER));
        } else if (key == ExtensionActionButtonProperties.ON_CLICK_LISTENER) {
            button.setOnClickListener(model.get(ExtensionActionButtonProperties.ON_CLICK_LISTENER));
        } else if (key == ExtensionActionButtonProperties.ON_HOVER_LISTENER) {
            button.setOnHoverListener(model.get(ExtensionActionButtonProperties.ON_HOVER_LISTENER));
        } else if (key == ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER) {
            button.setOnLongClickListener(
                    model.get(ExtensionActionButtonProperties.ON_LONG_CLICK_LISTENER));
        }
    }
}
