// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class ExtensionsMenuButtonViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE
                || key == ExtensionsToolbarProperties.EXTENSIONS_MENU_BUTTON_DEFAULT_BACKGROUND) {
            boolean isVisible =
                    model.get(ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE);
            if (isVisible) {
                view.setBackgroundResource(R.drawable.extensions_menu_button_bg);
            } else {
                int defaultBg =
                        model.get(
                                ExtensionsToolbarProperties
                                        .EXTENSIONS_MENU_BUTTON_DEFAULT_BACKGROUND);
                if (defaultBg != 0) {
                    view.setBackgroundResource(defaultBg);
                } else {
                    view.setBackgroundResource(R.drawable.default_icon_background);
                }
            }
        }
    }
}
