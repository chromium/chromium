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
        } else if (propertyKey == NavigationAttachmentsProperties.AI_MODE_ENABLED) {
            view.navigationType.setChecked(
                    model.get(NavigationAttachmentsProperties.AI_MODE_ENABLED));
        } else if (propertyKey == NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE) {
            boolean visible = model.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE);
            view.attachmentsView.setVisibility(visible ? View.VISIBLE : View.GONE);
            if (visible) {
                view.navigationType.setChecked(true);
            }
        } else if (propertyKey == NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE) {
            view.attachmentsToolbar.setVisibility(
                    model.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == NavigationAttachmentsProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.NAVIGATION_TYPE_VISIBLE) {
            view.navigationTypeGroup.setVisibility(
                    model.get(NavigationAttachmentsProperties.NAVIGATION_TYPE_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == NavigationAttachmentsProperties.ON_USE_AI_MODE_CHANGED) {
            view.navigationType.setOnCheckedChangeListener(
                    (buttonView, isChecked) -> {
                        model.get(NavigationAttachmentsProperties.ON_USE_AI_MODE_CHANGED)
                                .onResult(isChecked);
                    });
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED) {
            view.popup.mCameraButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE) {
            view.popup.mClipboardButton.setVisibility(
                    model.get(NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_CLIPBOARD_CLICKED) {
            view.popup.mClipboardButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_CLIPBOARD_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_FILE_CLICKED) {
            view.popup.mFileButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_FILE_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED) {
            view.popup.mGalleryButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE) {
            view.popup.mRecentTabsHeader.setVisibility(
                    model.get(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        }
    }
}
