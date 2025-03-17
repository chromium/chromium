// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Utility class for querying whether the operating system has granted various security permissions.
 */
@NullMarked
public class OsAdditionalSecurityPermissionUtil {
    private static @Nullable OsAdditionalSecurityPermissionProvider sProviderInstance;

    /**
     * Returns whether the operating system has granted permission to enable javascript optimizers.
     * Can be queried from any thread.
     */
    @CalledByNative
    public static boolean hasJavascriptOptimizerPermission() {
        if (PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH)) {
            return true;
        }

        OsAdditionalSecurityPermissionProvider provider = getProviderInstance();
        return provider == null || provider.hasJavascriptOptimizerPermission();
    }

    public static @Nullable OsAdditionalSecurityPermissionProvider getProviderInstance() {
        if (sProviderInstance == null) {
            sProviderInstance =
                    ServiceLoaderUtil.maybeCreate(OsAdditionalSecurityPermissionProvider.class);
        }
        return sProviderInstance;
    }

    public static void resetForTesting() {
        sProviderInstance = null;
    }
}
