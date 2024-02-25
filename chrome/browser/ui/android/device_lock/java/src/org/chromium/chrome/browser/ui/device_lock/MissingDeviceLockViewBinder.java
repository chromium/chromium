// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CHECKBOX_TOGGLED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.REMOVE_ALL_LOCAL_DATA_CHECKED;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Stateless Missing Device Lock page view binder. */
public class MissingDeviceLockViewBinder {
    public static void bind(
            PropertyModel model, MissingDeviceLockView view, PropertyKey propertyKey) {
        if (propertyKey == ON_CREATE_DEVICE_LOCK_CLICKED) {
            view.getCreateDeviceLockButton()
                    .setOnClickListener(model.get(ON_CREATE_DEVICE_LOCK_CLICKED));
        } else if (propertyKey == ON_CONTINUE_CLICKED) {
            view.getContinueButton().setOnClickListener(model.get(ON_CONTINUE_CLICKED));
        } else if (propertyKey == ON_CHECKBOX_TOGGLED) {
            view.getCheckbox().setOnCheckedChangeListener(model.get(ON_CHECKBOX_TOGGLED));
        } else if (propertyKey == REMOVE_ALL_LOCAL_DATA_CHECKED) {
            ThreadUtils.runOnUiThread(
                    () -> {
                        view.getCheckbox().setChecked(model.get(REMOVE_ALL_LOCAL_DATA_CHECKED));
                    });
        }
    }
}
