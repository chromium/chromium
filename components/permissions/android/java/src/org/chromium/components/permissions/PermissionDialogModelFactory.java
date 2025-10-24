// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** This class creates the model for the permission dialog. */
@NullMarked
class PermissionDialogModelFactory {
    public static PropertyModel getModel(
            ModalDialogProperties.Controller controller,
            PermissionDialogDelegate delegate,
            View customView,
            Runnable touchFilteredCallback) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText) || delegate.isEmbeddedPromptVariant();

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
        if (shouldUseVerticalButtons(delegate)) {
            builder.with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                    .with(
                            ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                            getButtonSpecs(delegate));
        } else {
            builder.with(
                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                            delegate.getPositiveButtonText())
                    .with(
                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                            delegate.getNegativeButtonText());
        }
        if (delegate.getContentSettingsTypes()[0] == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
            // The geolocation prompt with the precise and approximate location options is big, so
            // let's allow additional height for it.
            builder.with(
                    ModalDialogProperties.MAX_HEIGHT,
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.geolocation_with_options_prompt_max_height));
        }
        if (delegate.isEmbeddedPromptVariant()) {
            // We always begin with BUTTON_STYLES. This means we have @style/TextButton and
            // @style/FilledButton applied to two buttons negative-positive, respectively. That's
            // fine for OS_SYSTEM_SETTING. Later, for the ADMINISTRATOR screen, when we only have
            // one button, we can still choose one of them (in this case switch the button to
            // negative).
            builder.with(
                            ModalDialogProperties.BUTTON_STYLES,
                            ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                    .with(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS, true);
        }
        return builder.build();
    }

    public static ModalDialogProperties.ModalDialogButtonSpec[] getButtonSpecs(
            PermissionDialogDelegate delegate) {
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
        if (!delegate.isEmbeddedPromptVariant()
                                        && !delegate.canShowEphemeralOption()) {
            return new ModalDialogProperties.ModalDialogButtonSpec[] {
                positiveButtonSpec, negativeButtonSpec
            };
        }
        return delegate.shouldShowPositiveNonEphemeralAsFirstButton()
                ? new ModalDialogProperties.ModalDialogButtonSpec[] {
                    positiveButtonSpec, positiveEphemeralButtonSpec, negativeButtonSpec
                }
                : new ModalDialogProperties.ModalDialogButtonSpec[] {
                    positiveEphemeralButtonSpec, positiveButtonSpec, negativeButtonSpec
                };
    }

    public static boolean shouldUseVerticalButtons(PermissionDialogDelegate delegate) {
        if (delegate.isEmbeddedPromptVariant()
                                        || delegate.canShowEphemeralOption()) {
            return true;
        }
        if (delegate.isTablet()) {
            return true;
        }
        switch (delegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.UNINITIALIZED:
                return delegate.canShowEphemeralOption();
            case EmbeddedPromptVariant.ASK:
            case EmbeddedPromptVariant.PREVIOUSLY_GRANTED:
            case EmbeddedPromptVariant.PREVIOUSLY_DENIED:
                return true;
            case EmbeddedPromptVariant.ADMINISTRATOR_DENIED:
            case EmbeddedPromptVariant.ADMINISTRATOR_GRANTED:
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS:
                return false;
            case EmbeddedPromptVariant.OS_PROMPT:
                // We should never build a OS prompt view.
                assert false;
        }

        return false;
    }
}
