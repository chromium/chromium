// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.os.SystemProperties;

import androidx.annotation.VisibleForTesting;

import java.util.Map;

@VisibleForTesting
public final class AndroidOsSystemProperties {
    private static Map<String, String> sOverridesForTesting;

    /** See {@link android.os.SystemProperties#get} */
    public static String get(String key, String def) {
        if (sOverridesForTesting == null) return SystemProperties.get(key, def);

        var overrideForTesting = sOverridesForTesting.get(key);
        return overrideForTesting != null ? overrideForTesting : def;
    }

    public static final class WithOverridesForTesting implements AutoCloseable {
        public WithOverridesForTesting(Map<String, String> overrides) {
            assert sOverridesForTesting == null;
            sOverridesForTesting = overrides;
        }

        @Override
        public void close() {
            assert sOverridesForTesting != null;
            sOverridesForTesting = null;
        }
    }
}
