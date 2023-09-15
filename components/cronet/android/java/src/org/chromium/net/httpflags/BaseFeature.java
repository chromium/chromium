// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import androidx.annotation.VisibleForTesting;

import java.util.Map;

/**
 * Utility class for bridging the gap between HTTP flags and the native `base::Feature` framework.
 */
public final class BaseFeature {
    /**
     * HTTP flags that start with this name will be turned into base::Feature overrides.
     */
    @VisibleForTesting
    public static final String FLAG_PREFIX = "ChromiumBaseFeature_";

    private BaseFeature() {}

    /**
     * Turns a set of resolved HTTP flags into native {@code base::Feature} overrides.
     *
     * <p>Only HTTP flags whose name start with {@link #FLAG_PREFIX} are considered. The name of the
     * base::Feature being overridden is the name of the HTTP flag with the prefix removed; for
     * example, an HTTP flag called {@code ChromiumBaseFeature_CronetLogMe} is turned into an
     * override for the base::Feature named {@code CronetLogMe}.
     *
     * <p>A base::Feature is always a boolean (disabled or enabled), and for this reason, only
     * boolean HTTP flags can be used as base::Feature overrides.
     *
     * @throws IllegalArgumentException if an override has a non-boolean type
     *
     * @see org.chromium.net.impl.CronetLibraryLoader#getBaseFeatureOverrides
     */
    // TODO: add support for Feature Params
    public static BaseFeatureOverrides getOverrides(ResolvedFlags flags) {
        BaseFeatureOverrides.Builder builder = BaseFeatureOverrides.newBuilder();
        for (Map.Entry<String, ResolvedFlags.Value> flag : flags.flags().entrySet()) {
            String flagName = flag.getKey();
            if (!flagName.startsWith(FLAG_PREFIX)) continue;

            ResolvedFlags.Value flagValue = flag.getValue();

            ResolvedFlags.Value.Type valueType = flagValue.getType();
            if (valueType != ResolvedFlags.Value.Type.BOOL) {
                throw new IllegalArgumentException("HTTP flag `" + flagName + "` has type "
                        + valueType
                        + ", but only boolean flags are supported as base::Feature overrides");
            }

            builder.putOverrides(
                    flagName.substring(FLAG_PREFIX.length()), flagValue.getBoolValue());
        }
        return builder.build();
    }
}
