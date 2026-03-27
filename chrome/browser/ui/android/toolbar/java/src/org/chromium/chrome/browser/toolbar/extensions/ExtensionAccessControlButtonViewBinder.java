// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarProperties.IS_REQUEST_ACCESS_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.extensions.ExtensionsToolbarProperties.REQUEST_ACCESS_BUTTON_TEXT;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class ExtensionAccessControlButtonViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == IS_REQUEST_ACCESS_BUTTON_VISIBLE) {
            view.setVisibility(
                    model.get(IS_REQUEST_ACCESS_BUTTON_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (key == REQUEST_ACCESS_BUTTON_TEXT) {
            int count = model.get(REQUEST_ACCESS_BUTTON_TEXT);
            String text =
                    view.getContext()
                            .getString(R.string.extensions_request_access_button)
                            .replace("$1", String.valueOf(count));
            ((TextView) view).setText(text);
        } else if (key == REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(REQUEST_ACCESS_BUTTON_CONTENT_DESCRIPTION));
        }
    }
}
