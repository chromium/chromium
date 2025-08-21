// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the Navigation Attachments properties to the view and component. */
@NullMarked
class NavigationAttachmentsViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(
            PropertyModel model, NavigationAttachmentsViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == NavigationAttachmentsProperties.TOOLBAR_VISIBLE) {
            view.navigationToolbar.setVisibility(
                    model.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        }
    }
}
