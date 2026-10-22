// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.device_signals;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.KeyguardManager;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

/** Helper exposing Android OS device security signals to native code. */
@NullMarked
public final class DeviceSignalsUtils {
    private DeviceSignalsUtils() {}

    /** Returns whether the device is secured with a PIN, pattern or password. */
    @CalledByNative
    private static boolean isDeviceSecure() {
        KeyguardManager keyguardManager =
                assertNonNull(
                        ContextUtils.getApplicationContext()
                                .getSystemService(KeyguardManager.class));
        return keyguardManager.isDeviceSecure();
    }
}
