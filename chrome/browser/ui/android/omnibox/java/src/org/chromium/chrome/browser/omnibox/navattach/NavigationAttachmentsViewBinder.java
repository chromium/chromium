// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

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
        if (propertyKey == NavigationAttachmentsProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(NavigationAttachmentsProperties.ADAPTER));
        } else if (propertyKey == NavigationAttachmentsProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED) {
            view.popup.mCameraButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED) {
            view.popup.mGalleryButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.TOOLBAR_VISIBLE) {
            view.navigationToolbar.setVisibility(
                    model.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        }
    }
}
