// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Utility class for querying whether the operating system has granted various security permissions.
 */
@NullMarked
public class OsAdditionalSecurityPermissionUtil {
    private static @Nullable OsAdditionalSecurityPermissionProvider sProviderInstance;

    /** Requires native to be loaded. */
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
