// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.ui.base.WindowAndroid;

/** This bridge allows native web C++ code to launch DeviceLockActivity. */
public class DeviceLockBridge {
    /**
     * Whether the Device Lock page has been shown to the user and acknowledged with a device lock
     * present. This is used to determine whether to show the device lock page when the user is
     * interacting with sensitive personal data on the device.
     */
    public static final String DEVICE_LOCK_PAGE_HAS_BEEN_PASSED =
            "Chrome.DeviceLockPage.HasBeenPassed";

    private long mNativeDeviceLockBridge;

    private DeviceLockBridge(long nativeDeviceLockBridge) {
        mNativeDeviceLockBridge = nativeDeviceLockBridge;
    }

    @CalledByNative
    static DeviceLockBridge create(long nativeDeviceLockBridge) {
        return new DeviceLockBridge(nativeDeviceLockBridge);
    }

    /**
     * Launches DeviceLockActivity (explainer dialog and PIN/password setup flow) before allowing
     * users to continue if the user's device is not secure (ex: no PIN or password set).
     *
     * <p>TODO(crbug.com/40927226): Handle edge case where Chrome is killed when switching to OS PIN
     * flow.
     */
    @CalledByNative
    private void launchDeviceLockUiBeforeRunningCallback(@NonNull WindowAndroid windowAndroid) {
        if (mNativeDeviceLockBridge == 0) {
            return;
        }
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            DeviceLockActivityLauncher deviceLockActivityLauncher =
                    DeviceLockActivityLauncherSupplier.from(windowAndroid).get();
            deviceLockActivityLauncher.launchDeviceLockActivity(
                    context,
                    null,
                    false,
                    windowAndroid,
                    (resultCode, unused) ->
                            DeviceLockBridgeJni.get()
                                    .onDeviceLockUiFinished(
                                            mNativeDeviceLockBridge,
                                            resultCode == Activity.RESULT_OK),
                    DeviceLockActivityLauncher.Source.AUTOFILL);
        } else {
            DeviceLockBridgeJni.get().onDeviceLockUiFinished(mNativeDeviceLockBridge, false);
        }
    }

    @CalledByNative
    private void clearNativePointer() {
        mNativeDeviceLockBridge = 0;
    }

    @CalledByNative
    private static boolean isDeviceSecure() {
        return ((KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE))
                .isDeviceSecure();
    }

    @CalledByNative
    public static boolean deviceLockPageHasBeenPassed() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(DEVICE_LOCK_PAGE_HAS_BEEN_PASSED, false);
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        void onDeviceLockUiFinished(long nativeDeviceLockBridge, boolean isDeviceLockSet);
    }
}
