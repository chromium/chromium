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
 * <p>Creation-time keys ({@link AdaptiveUiProperties#DIALOG_CONTENT_DESCRIPTION}, {@link
 * AdaptiveUiProperties#CLOSE_BUTTON_VISIBILITY}, {@link AdaptiveUiProperties#CUSTOM_VIEW}, {@link
 * AdaptiveUiProperties#CUSTOM_VIEW_SCROLLS_ITSELF}, {@link
 * AdaptiveUiProperties#SECONDARY_BUTTON_BEHAVIOR}, and {@link
 * AdaptiveUiProperties#FILTER_TOUCH_FOR_SECURITY}) and event-time action keys ({@link
 * AdaptiveUiProperties#ON_PRIMARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED}, {@link
 * AdaptiveUiProperties#PRIMARY_BUTTON_KEEPS_OPEN}, and {@link
 * AdaptiveUiProperties#BACK_PRESS_HANDLER}) are handled by {@link AdaptiveUiDialogCoordinator}; the
 * lifecycle callbacks ({@link AdaptiveUiProperties#ON_SHOWN} and {@link
 * AdaptiveUiProperties#ON_DISMISSED}) are run by {@link AdaptiveUiControllerImpl}; sheet-only keys
 * ({@link AdaptiveUiProperties#SHEET_OPENED_ACCESSIBILITY_STRING_ID}, {@link
 * AdaptiveUiProperties#SHEET_CLOSED_ACCESSIBILITY_STRING_ID}, {@link
 * AdaptiveUiProperties#SHEET_INITIAL_STATE}, {@link AdaptiveUiProperties#SHEET_USER_INITIATED},
 * {@link AdaptiveUiProperties#SHEET_USER_CRITICAL}, {@link
 * AdaptiveUiProperties#SHEET_HALF_HEIGHT_RATIO}, {@link
 * AdaptiveUiProperties#SHEET_FULL_HEIGHT_RATIO}, and {@link
 * AdaptiveUiProperties#SHOW_SHEET_HANDLEBAR}) are ignored by dialogs. {@link
 * AdaptiveUiProperties#DESCRIPTION} is bound here, both at the initial bind and on live updates.
 */
@NullMarked
final class AdaptiveUiDialogViewBinder {
    /**
     * Keys handled at creation time or click time by {@link AdaptiveUiDialogCoordinator}, lifecycle
     * callbacks run by {@link AdaptiveUiControllerImpl}, or sheet-only keys ignored by dialogs,
     * rather than bound 1:1 by {@link #bind}.
     */
    static final Set<PropertyKey> COORDINATOR_HANDLED_KEYS =
            Set.of(
                    AdaptiveUiProperties.ON_SHOWN,
                    AdaptiveUiProperties.ON_DISMISSED,
                    AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                    AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                    AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                    AdaptiveUiProperties.SHEET_INITIAL_STATE,
                    AdaptiveUiProperties.SHEET_USER_INITIATED,
                    AdaptiveUiProperties.SHEET_USER_CRITICAL,
                    AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO,
                    AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO,
                    AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                    AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR,
                    AdaptiveUiProperties.CUSTOM_VIEW,
                    AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF,
                    AdaptiveUiProperties.BACK_PRESS_HANDLER,
                    AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                    AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                    AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                    AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY);

    static void bind(PropertyModel model, PropertyModel dialogModel, PropertyKey key) {
        if (key == AdaptiveUiProperties.HEADER_ICON) {
            dialogModel.set(
                    ModalDialogProperties.TITLE_ICON, model.get(AdaptiveUiProperties.HEADER_ICON));
        } else if (key == AdaptiveUiProperties.TITLE) {
            // Forward an empty title as null so TabModalPresenter falls back to
            // CONTENT_DESCRIPTION for the accessibility pane title.
            CharSequence title = model.get(AdaptiveUiProperties.TITLE);
            dialogModel.set(ModalDialogProperties.TITLE, TextUtils.isEmpty(title) ? null : title);
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
            if (AdaptiveUiDialogCoordinator.canDrawSecondaryButton(model)) {
                String secondaryText = model.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT);
                if (!TextUtils.isEmpty(secondaryText)
                        || dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT) != null) {
                    dialogModel.set(
                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                            TextUtils.isEmpty(secondaryText) ? null : secondaryText);
                }
            }
        } else if (key == AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED) {
            dialogModel.set(
                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                    model.get(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED));
        } else if (key == AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED) {
            if (AdaptiveUiDialogCoordinator.canDrawSecondaryButton(model)) {
                dialogModel.set(
                        ModalDialogProperties.NEGATIVE_BUTTON_DISABLED,
                        model.get(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED));
            }
        } else if (key == AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS) {
            dialogModel.set(
                    ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                    model.get(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
        } else if (key == AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED) {
            dialogModel.set(
                    ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE,
                    !model.get(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED));
        } else {
            assert COORDINATOR_HANDLED_KEYS.contains(key)
                    : "Unhandled AdaptiveUiProperties key in AdaptiveUiDialogViewBinder: " + key;
        }
    }

    private AdaptiveUiDialogViewBinder() {}
}
