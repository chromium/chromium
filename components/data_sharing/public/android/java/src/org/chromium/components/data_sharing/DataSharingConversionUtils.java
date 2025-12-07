// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.data_sharing.server_environment.ServerEnvironment;

// Utility class used to expose native data_sharing C++ APIs and configurations to Java.
@JNINamespace("data_sharing")
@NullMarked
public class DataSharingConversionUtils {

    /**
     * Fetches the current server environment from the native side.
     * This value can be configured by various native-side settings.
     *
     * @return The server environment as an {@link ServerEnvironment} IntDef.
     */
    public static @ServerEnvironment int getServerEnvironment() {
        return DataSharingConversionUtilsJni.get().getServerEnvironment();
    }

    @NativeMethods
    interface Natives {
        int getServerEnvironment();
    }
}
