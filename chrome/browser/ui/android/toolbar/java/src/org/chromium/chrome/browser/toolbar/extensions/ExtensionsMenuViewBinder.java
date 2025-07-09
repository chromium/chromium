// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class ExtensionsMenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == ExtensionsMenuProperties.CLOSE_CLICK_LISTENER) {
            view.findViewById(R.id.extensions_menu_close_button)
                    .setOnClickListener(model.get(ExtensionsMenuProperties.CLOSE_CLICK_LISTENER));
        }
    }
}
