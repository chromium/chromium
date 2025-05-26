// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

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
    public static void bind(PropertyModel model, ListMenuButton button, PropertyKey key) {
        if (key == ExtensionActionButtonProperties.TITLE) {
            String title = model.get(ExtensionActionButtonProperties.TITLE);
            button.setTooltipText(title);
            button.setContentDescription(title);
        } else if (key == ExtensionActionButtonProperties.ICON) {
            Bitmap bitmap = model.get(ExtensionActionButtonProperties.ICON);
            // TODO: Investigate the correct resizing method.
            bitmap.setDensity(120);
            button.setImageBitmap(bitmap);
        } else if (key == ExtensionActionButtonProperties.ON_CLICK_LISTENER) {
            button.setOnClickListener(model.get(ExtensionActionButtonProperties.ON_CLICK_LISTENER));
        }
    }
}
