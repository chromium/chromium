// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;

/**
 * Utility class for querying whether the operating system has granted various security permissions.
 */
@NullMarked
public class OsAdditionalSecurityUtil {
    private static @Nullable OsAdditionalSecurityProvider sProviderInstance;

    /** Requires native to be loaded. */
    public static OsAdditionalSecurityProvider getProviderInstance() {
        if (sProviderInstance == null) {
            sProviderInstance = new OsAdditionalSecurityProvider();
        }
        return sProviderInstance;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(OsAdditionalSecurityProvider provider) {
        sProviderInstance = provider;
        ResettersForTesting.register(() -> sProviderInstance = null);
    }
}
