// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Properties describing a user education feature's UI format.
 *
 * <p>Whether a feature is blocked by the {@code PromotionsEnabled} enterprise policy or may bypass
 * its format's cooldown group is determined centrally by {@code EnterprisePolicyBypassAllowlist}
 * and {@code CooldownBypassAllowlist}.
 */
@NullMarked
public class FeatureProperties {
    private final @UiFormat int mFormat;

    /**
     * Constructs a {@link FeatureProperties} instance.
     *
     * @param format The {@link UiFormat} of the user education surface.
     */
    public FeatureProperties(@UiFormat int format) {
        mFormat = format;
    }

    /** Returns the {@link UiFormat} of the user education surface. */
    public @UiFormat int getFormat() {
        return mFormat;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof FeatureProperties other)) return false;
        return mFormat == other.mFormat;
    }

    @Override
    public int hashCode() {
        return Integer.hashCode(mFormat);
    }

    @Override
    public String toString() {
        return "FeatureProperties{format=" + formatToString(mFormat) + "}";
    }

    // LINT.IfChange(UiFormat)
    private static String formatToString(@UiFormat int format) {
        switch (format) {
            case UiFormat.UNKNOWN:
                return "UNKNOWN";
            case UiFormat.BLUE_BUBBLE:
                return "BLUE_BUBBLE";
            case UiFormat.BOTTOM_SHEET:
                return "BOTTOM_SHEET";
            case UiFormat.FRE:
                return "FRE";
            case UiFormat.SET_UP_LIST:
                return "SET_UP_LIST";
            case UiFormat.ALERTS:
                return "ALERTS";
            case UiFormat.MAGIC_STACK:
                return "MAGIC_STACK";
            case UiFormat.NOTIFICATIONS:
                return "NOTIFICATIONS";
            case UiFormat.MESSAGES:
                return "MESSAGES";
            case UiFormat.INLINE_PROMO:
                return "INLINE_PROMO";
            case UiFormat.DIALOG:
                return "DIALOG";
            default:
                return Integer.toString(format);
        }
    }
    // LINT.ThenChange(//components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/UiFormat.java:UiFormat)
}
