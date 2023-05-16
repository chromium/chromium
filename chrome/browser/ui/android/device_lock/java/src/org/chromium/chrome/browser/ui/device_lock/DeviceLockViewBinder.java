// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless Device Lock page view binder.
 */
public class DeviceLockViewBinder {
    public static void bind(PropertyModel model, DeviceLockView view, PropertyKey propertyKey) {
        if (propertyKey == DeviceLockProperties.PREEXISTING_DEVICE_LOCK) {
            DeviceLockViewBinder.setTitle(model, view);
            DeviceLockViewBinder.setDescription(model, view);
            DeviceLockViewBinder.setContinueButton(model, view);
            view.getDismissButton().setVisibility(
                    model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK) ? View.INVISIBLE
                                                                            : View.VISIBLE);
        } else if (propertyKey == DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT) {
            DeviceLockViewBinder.setDescription(model, view);
            DeviceLockViewBinder.setContinueButton(model, view);
        } else if (propertyKey == DeviceLockProperties.IN_SIGN_IN_FLOW) {
            if (model.get(DeviceLockProperties.IN_SIGN_IN_FLOW)) {
                view.getDismissButton().setText(R.string.signin_fre_dismiss_button);
            } else {
                view.getDismissButton().setText(R.string.no_thanks);
            }
        } else if (propertyKey == DeviceLockProperties.ON_DISMISS_CLICKED) {
            view.getDismissButton().setOnClickListener(
                    model.get(DeviceLockProperties.ON_DISMISS_CLICKED));
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
            view.getDescription().setText(R.string.device_lock_existing_lock_description);
            return;
        }
        if (model.get(DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT)) {
            view.getDescription().setText(R.string.device_lock_description);
            return;
        }
        view.getDescription().setText(R.string.device_lock_through_settings_description);
    }

    private static void setContinueButton(PropertyModel model, DeviceLockView view) {
        if (model.get(DeviceLockProperties.PREEXISTING_DEVICE_LOCK)) {
            view.getContinueButton().setText(R.string.got_it);
            view.getContinueButton().setOnClickListener(
                    model.get(DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED));
            return;
        }
        if (model.get(DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT)) {
            view.getContinueButton().setText(R.string.device_lock_create_lock_button);
            view.getContinueButton().setOnClickListener(
                    model.get(DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED));
            return;
        }
        view.getContinueButton().setText(R.string.device_lock_create_lock_button);
        view.getContinueButton().setOnClickListener(
                model.get(DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED));
    }
}
