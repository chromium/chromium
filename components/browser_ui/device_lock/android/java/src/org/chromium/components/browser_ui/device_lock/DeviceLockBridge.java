// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** This bridge allows native web C++ code to launch DeviceLockActivity. */
@NullMarked
public class DeviceLockBridge {
    /**
     * Whether the Device Lock page has been shown to the user and acknowledged with a device lock
     * present. This is used to determine whether to show the device lock page when the user is
     * interacting with sensitive personal data on the device.
     */
    public static final String DEVICE_LOCK_PAGE_HAS_BEEN_PASSED =
            "Chrome.DeviceLockPage.HasBeenPassed";

    // Do not instantiate this class; all methods are static.
    private DeviceLockBridge() {}

    /**
     * Returns true iff the device lock page has already been passed (i.e. the device lock page has
     * been shown to and affirmatively acknowledged by the user).
     */
    @CalledByNative
    public static boolean deviceLockPageHasBeenPassed() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false);
    }

    /**
     * Launches DeviceLockActivity (explainer dialog and PIN/password setup flow) before allowing
     * users to continue if the user's device is not secure (ex: no PIN or password set).
     *
     * <p>TODO(crbug.com/40927226): Handle edge case where Chrome is killed when switching to OS PIN
     * flow.
     */
    @CalledByNative
    private static void launchDeviceLockUiBeforeRunningCallback(
            WindowAndroid windowAndroid, JniOnceCallback<Boolean> callback) {
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            DeviceLockActivityLauncher deviceLockActivityLauncher =
                    assumeNonNull(DeviceLockActivityLauncherSupplier.from(windowAndroid))
                            .asNonNull()
                            .get();
            deviceLockActivityLauncher.launchDeviceLockActivity(
                    context,
                    null,
                    false,
                    windowAndroid,
                    (resultCode, unused) -> callback.onResult(resultCode == Activity.RESULT_OK),
                    DeviceLockActivityLauncher.Source.AUTOFILL);
        } else {
            callback.onResult(false);
        }
    }

    @CalledByNative
    private static boolean isDeviceSecure() {
        return ((KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE))
                .isDeviceSecure();
    }
}
