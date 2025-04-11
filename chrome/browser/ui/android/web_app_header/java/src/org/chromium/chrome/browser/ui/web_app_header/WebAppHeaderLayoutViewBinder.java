// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder that reflects model updates on the {@link WebAppHeaderLayout}. */
class WebAppHeaderLayoutViewBinder {

    private WebAppHeaderLayoutViewBinder() {}

    /**
     * Updates webapp header to reflect the state of a single model parameter.
     *
     * @param model the model that contains properties to reflect on the view.
     * @param view a view to be updated with a model state.
     * @param key a key that indicates what model property has changed.
     */
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey key) {
        if (key == WebAppHeaderLayoutProperties.PADDINGS) {
            final Rect paddings = model.get(WebAppHeaderLayoutProperties.PADDINGS);
            view.setPadding(paddings.left, paddings.top, paddings.right, paddings.bottom);
        } else if (key == WebAppHeaderLayoutProperties.IS_VISIBLE) {
            view.setVisibility(
                    model.get(WebAppHeaderLayoutProperties.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (key == WebAppHeaderLayoutProperties.MIN_HEIGHT) {
            view.setMinimumHeight(model.get(WebAppHeaderLayoutProperties.MIN_HEIGHT));
        } else {
            assert false : String.format("Unsupported property key %s", key.toString());
        }
    }
}
