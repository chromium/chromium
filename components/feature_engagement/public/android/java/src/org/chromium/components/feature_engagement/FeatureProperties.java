// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Properties describing a user education feature's UI format, enterprise policy compliance, and
 * cooldown group behavior.
 */
@NullMarked
public class FeatureProperties {
    private final @UiFormat int mFormat;
    private final boolean mIsBlockedByEnterprisePolicy;
    private final boolean mIsBlockedByCooldownGroup;
    private final boolean mBlocksCooldownGroup;

    /**
     * Constructs a {@link FeatureProperties} instance.
     *
     * @param format The {@link UiFormat} of the user education surface.
     * @param isBlockedByEnterprisePolicy Whether the feature is blocked when the PromotionsEnabled
     *     enterprise policy disables promotions.
     * @param isBlockedByCooldownGroup Whether the feature is blocked by the cooldown group for its
     *     format type.
     * @param blocksCooldownGroup Whether triggering this feature contributes to the cooldown group
     *     for its format type.
     */
    public FeatureProperties(
            @UiFormat int format,
            boolean isBlockedByEnterprisePolicy,
            boolean isBlockedByCooldownGroup,
            boolean blocksCooldownGroup) {
        mFormat = format;
        mIsBlockedByEnterprisePolicy = isBlockedByEnterprisePolicy;
        mIsBlockedByCooldownGroup = isBlockedByCooldownGroup;
        mBlocksCooldownGroup = blocksCooldownGroup;
    }

    /** Returns the {@link UiFormat} of the user education surface. */
    public @UiFormat int getFormat() {
        return mFormat;
    }

    /**
     * Returns whether the feature is blocked when the PromotionsEnabled enterprise policy disables
     * promotions.
     */
    public boolean isBlockedByEnterprisePolicy() {
        return mIsBlockedByEnterprisePolicy;
    }

    /** Returns whether the feature is blocked by the cooldown group for its format type. */
    public boolean isBlockedByCooldownGroup() {
        return mIsBlockedByCooldownGroup;
    }

    /**
     * Returns whether triggering this feature contributes to the cooldown group for its format
     * type.
     */
    public boolean blocksCooldownGroup() {
        return mBlocksCooldownGroup;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof FeatureProperties other)) return false;
        return mFormat == other.mFormat
                && mIsBlockedByEnterprisePolicy == other.mIsBlockedByEnterprisePolicy
                && mIsBlockedByCooldownGroup == other.mIsBlockedByCooldownGroup
                && mBlocksCooldownGroup == other.mBlocksCooldownGroup;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mFormat,
                mIsBlockedByEnterprisePolicy,
                mIsBlockedByCooldownGroup,
                mBlocksCooldownGroup);
    }

    @Override
    public String toString() {
        return "FeatureProperties{format="
                + formatToString(mFormat)
                + ", isBlockedByEnterprisePolicy="
                + mIsBlockedByEnterprisePolicy
                + ", isBlockedByCooldownGroup="
                + mIsBlockedByCooldownGroup
                + ", blocksCooldownGroup="
                + mBlocksCooldownGroup
                + "}";
    }

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
}
