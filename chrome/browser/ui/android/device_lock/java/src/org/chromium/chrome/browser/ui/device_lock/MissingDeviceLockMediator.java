// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CHECKBOX_TOGGLED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CONTINUE_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.MissingDeviceLockProperties.REMOVE_ALL_LOCAL_DATA_CHECKED;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;

public class MissingDeviceLockMediator {
    private final PropertyModel mModel;

    private final Callback<Boolean> mOnContinueWithoutDeviceLock;
    private final Context mContext;

    MissingDeviceLockMediator(Callback<Boolean> onContinueWithoutDeviceLock, Context context) {
        mOnContinueWithoutDeviceLock = onContinueWithoutDeviceLock;
        mContext = context;

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(REMOVE_ALL_LOCAL_DATA_CHECKED, true)
                        .with(
                                ON_CREATE_DEVICE_LOCK_CLICKED,
                                DeviceLockUtils.isDeviceLockCreationIntentSupported(mContext)
                                        ? v -> createDeviceLockDirectly()
                                        : v -> createDeviceLockThroughOSSettings())
                        .with(ON_CONTINUE_CLICKED, v -> continueWithoutDeviceLock())
                        .with(ON_CHECKBOX_TOGGLED, (v, isChecked) -> onCheckboxToggled(isChecked))
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void createDeviceLockDirectly() {
        mContext.startActivity(DeviceLockUtils.createDeviceLockDirectlyIntent());
    }

    private void createDeviceLockThroughOSSettings() {
        mContext.startActivity(DeviceLockUtils.createDeviceLockThroughOSSettingsIntent());
    }

    private void onCheckboxToggled(boolean isChecked) {
        mModel.set(REMOVE_ALL_LOCAL_DATA_CHECKED, isChecked);
    }

    private void continueWithoutDeviceLock() {
        mOnContinueWithoutDeviceLock.onResult(mModel.get(REMOVE_ALL_LOCAL_DATA_CHECKED));
    }
}
