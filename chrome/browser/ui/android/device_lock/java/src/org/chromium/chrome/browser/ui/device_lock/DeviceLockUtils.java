// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.device_lock;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

public class DeviceLockUtils {
    static boolean isDeviceLockCreationIntentSupported(Context context) {
        return new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD)
                        .resolveActivity(context.getPackageManager())
                != null;
    }

    static Intent createDeviceLockDirectlyIntent() {
        return new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
    }

    static Intent createDeviceLockThroughOSSettingsIntent() {
        return new Intent(Settings.ACTION_SECURITY_SETTINGS);
    }
}
