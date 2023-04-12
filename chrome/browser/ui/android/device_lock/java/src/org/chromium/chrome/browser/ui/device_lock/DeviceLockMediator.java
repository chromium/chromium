// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator handles which design the device lock UI displays and interacts through the
 * coordinator delegate.
 */
public class DeviceLockMediator {
    private final PropertyModel mModel;
    private final DeviceLockCoordinator.Delegate mDelegate;
    private final Context mContext;

    public DeviceLockMediator(
            boolean inSignInFlow, DeviceLockCoordinator.Delegate delegate, Context context) {
        mDelegate = delegate;
        mContext = context;
        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(PREEXISTING_DEVICE_LOCK, isDeviceLockPresent())
                         .with(DEVICE_SUPPORTS_PIN_CREATION_INTENT,
                                 isDeviceLockCreationIntentSupported())
                         .with(IN_SIGN_IN_FLOW, inSignInFlow)
                         .with(ON_CREATE_DEVICE_LOCK_CLICKED, v -> navigateToDeviceLockCreation())
                         .with(ON_GO_TO_OS_SETTINGS_CLICKED, v -> navigateToOSSettings())
                         .with(ON_USER_UNDERSTANDS_CLICKED, v -> delegate.onDeviceLockReady())
                         .with(ON_DISMISS_CLICKED, v -> delegate.onDeviceLockRefused())
                         .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    private boolean isDeviceLockPresent() {
        KeyguardManager keyguardManager =
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager.isDeviceSecure();
    }

    private boolean isDeviceLockCreationIntentSupported() {
        return new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD)
                       .resolveActivity(mContext.getPackageManager())
                != null;
    }

    private void navigateToDeviceLockCreation() {
        // TODO(crbug.com/1432024): Add logic to navigate to device lock creation
        mDelegate.onDeviceLockReady();
    }

    private void navigateToOSSettings() {
        // TODO(crbug.com/1432024): Add logic to navigate to OS security settings
        mDelegate.onDeviceLockReady();
    }
}
