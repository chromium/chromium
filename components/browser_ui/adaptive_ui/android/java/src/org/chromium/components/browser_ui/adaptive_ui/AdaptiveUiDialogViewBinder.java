// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Stateless binder that maps 1:1 {@link AdaptiveUiProperties} keys onto a {@link
 * ModalDialogProperties} {@link PropertyModel} via {@link
 * org.chromium.ui.modelutil.PropertyModelChangeProcessor}.
 *
 * <p>Creation-time structural keys ({@link AdaptiveUiProperties#DIALOG_CONTENT_DESCRIPTION} and
 * {@link AdaptiveUiProperties#CUSTOM_VIEW}) and event-time action keys ({@link
 * AdaptiveUiProperties#ON_PRIMARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#PRIMARY_BUTTON_KEEPS_OPEN}, {@link
 * AdaptiveUiProperties#SECONDARY_BUTTON_KEEPS_OPEN}, and {@link
 * AdaptiveUiProperties#BACK_PRESS_HANDLER}) are handled by {@link AdaptiveUiDialogCoordinator}.
 * {@link AdaptiveUiProperties#DESCRIPTION} is bound here, both at the initial bind and on live
 * updates.
 */
@NullMarked
final class AdaptiveUiDialogViewBinder {
    /**
     * Keys handled at creation time or click time by {@link AdaptiveUiDialogCoordinator} rather
     * than bound 1:1 by {@link #bind}.
     */
    static final Set<PropertyKey> COORDINATOR_HANDLED_KEYS =
            Set.of(
                    AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                    AdaptiveUiProperties.CUSTOM_VIEW,
                    AdaptiveUiProperties.BACK_PRESS_HANDLER,
                    AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                    AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN);

    static void bind(PropertyModel model, PropertyModel dialogModel, PropertyKey key) {
        if (key == AdaptiveUiProperties.HEADER_ICON) {
            dialogModel.set(
                    ModalDialogProperties.TITLE_ICON, model.get(AdaptiveUiProperties.HEADER_ICON));
        } else if (key == AdaptiveUiProperties.TITLE) {
            dialogModel.set(ModalDialogProperties.TITLE, model.get(AdaptiveUiProperties.TITLE));
        } else if (key == AdaptiveUiProperties.DESCRIPTION) {
            CharSequence description = model.get(AdaptiveUiProperties.DESCRIPTION);
            View dialogCustomView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
            if (dialogCustomView == null) {
                if (!TextUtils.isEmpty(description)
                        || dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS) != null) {
                    dialogModel.set(
                            ModalDialogProperties.MESSAGE_PARAGRAPHS,
                            TextUtils.isEmpty(description)
                                    ? null
                                    : new ArrayList<>(List.of(description)));
                }
            } else {
                TextView descriptionView =
                        dialogCustomView.findViewById(R.id.adaptive_ui_dialog_description);
                if (descriptionView != null) {
                    descriptionView.setText(description);
                    descriptionView.setVisibility(
                            TextUtils.isEmpty(description) ? View.GONE : View.VISIBLE);
                    UiUtils.maybeSetLinkMovementMethod(descriptionView);
                }
            }
        } else if (key == AdaptiveUiProperties.SHOW_CLOSE_BUTTON) {
            dialogModel.set(
                    ModalDialogProperties.TITLE_CLOSE_BUTTON_VISIBLE,
                    model.get(AdaptiveUiProperties.SHOW_CLOSE_BUTTON));
        } else if (key == AdaptiveUiProperties.FOOTER_MESSAGE) {
            dialogModel.set(
                    ModalDialogProperties.FOOTER_MESSAGE,
                    model.get(AdaptiveUiProperties.FOOTER_MESSAGE));
        } else if (key == AdaptiveUiProperties.PRIMARY_BUTTON_TEXT) {
            String primaryText = model.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT);
            if (!TextUtils.isEmpty(primaryText)
                    || dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT) != null) {
                dialogModel.set(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        TextUtils.isEmpty(primaryText) ? null : primaryText);
            }
        } else if (key == AdaptiveUiProperties.SECONDARY_BUTTON_TEXT) {
            String secondaryText = model.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT);
            if (!TextUtils.isEmpty(secondaryText)
                    || dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT) != null) {
                dialogModel.set(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        TextUtils.isEmpty(secondaryText) ? null : secondaryText);
            }
        } else if (key == AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED) {
            dialogModel.set(
                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                    model.get(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED));
        } else if (key == AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED) {
            dialogModel.set(
                    ModalDialogProperties.NEGATIVE_BUTTON_DISABLED,
                    model.get(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED));
        } else {
            assert COORDINATOR_HANDLED_KEYS.contains(key)
                    : "Unhandled AdaptiveUiProperties key in AdaptiveUiDialogViewBinder: " + key;
        }
    }

    private AdaptiveUiDialogViewBinder() {}
}
