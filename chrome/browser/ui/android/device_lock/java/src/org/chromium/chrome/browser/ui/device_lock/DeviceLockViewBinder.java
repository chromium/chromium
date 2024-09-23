// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.view.View;

import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockDialogMetrics;
import org.chromium.components.browser_ui.device_lock.DeviceLockDialogMetrics.DeviceLockDialogAction;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Stateless Device Lock page view binder. */
public class DeviceLockViewBinder {
    public static void bind(PropertyModel model, DeviceLockView view, PropertyKey propertyKey) {
        if (propertyKey == DeviceLockProperties.PREEXISTING_DEVICE_LOCK) {
            DeviceLockViewBinder.setTitle(model, view);
            DeviceLockViewBinder.setDescription(model, view);
            DeviceLockViewBinder.setNoticeText(model, view);
            DeviceLockViewBinder.setContinueButton(model, view);
            DeviceLockViewBinder.setDismissButton(model, view);
        } else if (propertyKey == DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT) {
            DeviceLockViewBinder.setContinueButton(model, view);
        } else if (propertyKey == DeviceLockProperties.SOURCE) {
            DeviceLockViewBinder.setDismissButton(model, view);
        } else if (propertyKey == DeviceLockProperties.UI_ENABLED) {
            if (model.get(DeviceLockProperties.UI_ENABLED)) {
                int dialogShownAction =
                        model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)
                                ? DeviceLockDialogAction.EXISTING_DEVICE_LOCK_DIALOG_SHOWN
                                : DeviceLockDialogAction.CREATE_DEVICE_LOCK_DIALOG_SHOWN;
                DeviceLockDialogMetrics.recordDeviceLockDialogAction(
                        dialogShownAction, model.get(DeviceLockProperties.SOURCE));
            }
            DeviceLockViewBinder.setUiStyle(model, view);
        } else if (propertyKey == DeviceLockProperties.ON_DISMISS_CLICKED) {
            view.getDismissButton()
                    .setOnClickListener(model.get(DeviceLockProperties.ON_DISMISS_CLICKED));
        }
    }

    private static void setTitle(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
            view.getTitle().setText(R.string.device_lock_existing_lock_title);
            return;
        }
        view.getTitle().setText(R.string.device_lock_title);
    }

    private static void setDescription(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
            @StringRes
            int stringId =
                    ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                            ? R.string.device_lock_existing_lock_description_for_signin
                            : R.string.device_lock_existing_lock_description;
            view.getDescription().setText(stringId);
            return;
        }
        view.getDescription().setText(R.string.device_lock_description);
    }

    private static void setNoticeText(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
            view.getNoticeText().setText(R.string.device_lock_notice);
            return;
        }
        view.getNoticeText().setText(R.string.device_lock_creation_notice);
    }

    private static void setContinueButton(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
            view.getContinueButton().setText(R.string.got_it);
            view.getContinueButton()
                    .setOnClickListener(
                            model.get(DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED));
            return;
        }
        view.getContinueButton().setText(R.string.device_lock_create_lock_button);
        if (model.get(DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT)) {
            view.getContinueButton()
                    .setOnClickListener(
                            model.get(DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED));
        } else {
            view.getContinueButton()
                    .setOnClickListener(
                            model.get(DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED));
        }
    }

    private static void setUiStyle(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.UI_ENABLED)) {
            view.getProgressBar().setVisibility(View.INVISIBLE);
            view.getTitle().setTextAppearance(R.style.TextAppearance_Headline_Primary);
            view.getDescription().setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
            view.getNoticeText().setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
            view.getNoticeText()
                    .setDrawableTintColor(
                            AppCompatResources.getColorStateList(
                                    view.getContext(),
                                    R.color.default_icon_color_accent1_tint_list));
            view.getContinueButton().setEnabled(true);
            view.getDismissButton().setEnabled(true);
        } else {
            view.getProgressBar().setVisibility(View.VISIBLE);
            view.getTitle().setTextAppearance(R.style.TextAppearance_Headline_Disabled);
            view.getDescription().setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
            view.getNoticeText().setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
            view.getNoticeText()
                    .setDrawableTintColor(
                            AppCompatResources.getColorStateList(
                                    view.getContext(), R.color.default_text_color_disabled_list));
            view.getContinueButton().setEnabled(false);
            view.getDismissButton().setEnabled(false);
        }
    }

    private static void setDismissButton(PropertyModel model, DeviceLockView view) {
        if (DeviceLockActivityLauncher.isSignInFlow(model.get(DeviceLockProperties.SOURCE))) {
            if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
                view.getDismissButton().setText(R.string.signin_fre_dismiss_button);
                view.getDismissButton()
                        .setOnClickListener(
                                model.get(DeviceLockProperties.ON_USE_WITHOUT_AN_ACCOUNT_CLICKED));
            } else {
                view.getDismissButton().setText(R.string.dialog_not_now);
                view.getDismissButton()
                        .setOnClickListener(model.get(DeviceLockProperties.ON_DISMISS_CLICKED));
            }
        } else {
            view.getDismissButton().setText(R.string.no_thanks);
            view.getDismissButton()
                    .setOnClickListener(model.get(DeviceLockProperties.ON_DISMISS_CLICKED));
        }
    }
}
