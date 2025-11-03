// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RippleBackgroundHelper;

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
        } else if (propertyKey == NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE) {
            @StringRes
            int res =
                    switch (model.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE)) {
                        case AutocompleteRequestType.AI_MODE -> R.string.ai_mode_entrypoint_label;
                        default -> 0;
                    };

            if (res != 0) {
                view.requestType.setText(res);
                view.requestType.setContentDescription(
                        view.requestType
                                .getResources()
                                .getString(
                                        R.string.accessibility_omnibox_reset_mode,
                                        view.requestType.getResources().getString(res)));
            }
            updateModeSelectorVisibility(model, view);
        } else if (propertyKey
                == NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED) {
            view.requestType.setOnClickListener(
                    v ->
                            model.get(
                                            NavigationAttachmentsProperties
                                                    .AUTOCOMPLETE_REQUEST_TYPE_CLICKED)
                                    .run());
        } else if (propertyKey == NavigationAttachmentsProperties.POPUP_AI_MODE_CLICKED) {
            view.popup.mAiModeButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.POPUP_AI_MODE_CLICKED).run());
        } else if (propertyKey == NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE) {
            boolean visible = model.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE);
            view.attachmentsView.setVisibility(visible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE) {
            view.attachmentsToolbar.setVisibility(
                    model.get(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == NavigationAttachmentsProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey
                == NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE) {
            int visibility =
                    model.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE)
                            ? View.VISIBLE
                            : View.GONE;
            view.popup.mAutocompleteRequestTypeGroup.setVisibility(visibility);
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
        } else if (propertyKey == NavigationAttachmentsProperties.SHOW_DEDICATED_MODE_BUTTON) {
            updateModeSelectorVisibility(model, view);
        }
    }

    static void updateModeSelectorVisibility(
            PropertyModel model, NavigationAttachmentsViewHolder views) {
        boolean showDedicatedModeButton =
                model.get(NavigationAttachmentsProperties.SHOW_DEDICATED_MODE_BUTTON);
        boolean isAiModeEnabled =
                model.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE)
                        == AutocompleteRequestType.AI_MODE;
        Resources res = views.requestType.getResources();

        views.requestType.setVisibility(
                isAiModeEnabled || showDedicatedModeButton ? View.VISIBLE : View.GONE);

        views.requestType.setButtonColor(
                isAiModeEnabled
                        ? res.getColorStateList(R.color.gm3_baseline_surface_container)
                        : res.getColorStateList(android.R.color.transparent));

        views.requestType.setBorderStyle(
                isAiModeEnabled
                        ? RippleBackgroundHelper.BorderType.SOLID
                        : RippleBackgroundHelper.BorderType.DASHED);

        views.requestType.setCompoundDrawablesRelativeWithIntrinsicBounds(
                res.getDrawable(R.drawable.search_spark_black_24dp),
                null,
                isAiModeEnabled ? res.getDrawable(R.drawable.btn_close) : null,
                null);

        views.popup.mAutocompleteRequestTypeGroup.setVisibility(
                showDedicatedModeButton ? View.GONE : View.VISIBLE);
    }
}
