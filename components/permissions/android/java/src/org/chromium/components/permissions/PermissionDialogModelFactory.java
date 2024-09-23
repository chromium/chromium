// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** This class creates the model for the permission dialog. */
class PermissionDialogModelFactory {
    public static PropertyModel getModel(
            ModalDialogProperties.Controller controller,
            PermissionDialogDelegate delegate,
            View customView,
            Runnable touchFilteredCallback) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.FOCUS_DIALOG, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, messageText)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .with(ModalDialogProperties.TOUCH_FILTERED_CALLBACK, touchFilteredCallback)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .with(
                                ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE,
                                PermissionsAndroidFeatureMap.isEnabled(
                                        PermissionsAndroidFeatureList
                                                .ANDROID_CANCEL_PERMISSION_PROMPT_ON_TOUCH_OUTSIDE));
        if (delegate.canShowEphemeralOption()) {
            var positiveEphemeralButtonSpec =
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL,
                            delegate.getPositiveEphemeralButtonText());
            var positiveButtonSpec =
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            ModalDialogProperties.ButtonType.POSITIVE,
                            delegate.getPositiveButtonText());
            var negativeButtonSpec =
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            ModalDialogProperties.ButtonType.NEGATIVE,
                            delegate.getNegativeButtonText());
            var buttonSpecs =
                    delegate.shouldShowPositiveNonEphemeralAsFirstButton()
                            ? new ModalDialogProperties.ModalDialogButtonSpec[] {
                                positiveButtonSpec, positiveEphemeralButtonSpec, negativeButtonSpec
                            }
                            : new ModalDialogProperties.ModalDialogButtonSpec[] {
                                positiveEphemeralButtonSpec, positiveButtonSpec, negativeButtonSpec
                            };
            builder.with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                    .with(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST, buttonSpecs);
        } else {
            builder.with(
                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                            delegate.getPositiveButtonText())
                    .with(
                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                            delegate.getNegativeButtonText());
        }
        return builder.build();
    }
}
