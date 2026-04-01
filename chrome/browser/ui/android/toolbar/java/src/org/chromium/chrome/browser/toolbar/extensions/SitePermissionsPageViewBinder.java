// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.view.View;

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
        } else if (key == SitePermissionsPageProperties.EXTENSION_ID) {
            // TODO(cburg.com/432392216): Implement data pull for site permissions page.
        }
    }
}
