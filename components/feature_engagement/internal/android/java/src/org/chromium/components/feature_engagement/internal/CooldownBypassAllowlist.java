// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.Set;

/**
 * Central place for Feature Engagement cooldown group exemptions on Android.
 *
 * <p>{@code ALLOWLISTED_FEATURES} holds features that are permitted to show even while their {@code
 * UiFormat}'s cooldown group is active; they still contribute to the cooldown group when they show.
 * {@code DO_NOT_BLOCK_COOLDOWN_GROUP_FEATURES} holds features that do not contribute to their
 * format's cooldown group when shown.
 *
 * <p>Features should only be added to either list if they must show regardless of other recent
 * promos, for example because they are user-initiated, legally required, or part of a one-time flow
 * such as the FRE. Adding a feature to either list requires approval from User Education owners and
 * a {@code [UserEducation.BypassCooldownCheck]=<justification>} tag in the CL description.
 *
 * <p>Features permitted to bypass the {@code PromotionsEnabled} enterprise policy are defined
 * separately in {@link EnterprisePolicyBypassAllowlist} (in this same package).
 */
@NullMarked
public abstract class CooldownBypassAllowlist {
    private static final Set<String> ALLOWLISTED_FEATURES =
            Set.of(
                    // keep-sorted start
                    FeatureConstants.KEYBOARD_ACCESSORY_ACCOUNT_NAME_EMAIL_SUGGESTION_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_AT_MEMORY_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_AUTOFILL_AI_SPII_DISCLAIMER_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_AUTOFILL_AI_VALUABLES_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_ENABLE_LOYALTY_CARDS_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_HOME_WORK_PROFILE_SUGGESTION_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_CARD_INFO_RETRIEVAL_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_DISABLED_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                    FeatureConstants.KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_FILL_FEATURE,
                    FeatureConstants.NOTIFICATION_PERMISSION_PROMO_FEATURE,
                    FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE,
                    FeatureConstants.SEARCH_ENGINE_CHOICE_PROMO_FEATURE
                    // keep-sorted end
                    );

    // Features that do not contribute to their format's cooldown group when shown. Currently
    // there are no features that skip blocking their cooldown group.
    private static final Set<String> DO_NOT_BLOCK_COOLDOWN_GROUP_FEATURES =
            Set.of(
                    // keep-sorted start
                    // keep-sorted end
                    );

    /** Returns whether the given feature is permitted to bypass its format's cooldown group. */
    public static boolean isAllowedToBypassCooldownGroup(String feature) {
        return ALLOWLISTED_FEATURES.contains(feature);
    }

    /**
     * Returns whether the given feature is permitted to show without blocking (contributing to) its
     * format's cooldown group.
     */
    public static boolean isAllowedToSkipBlockingCooldownGroup(String feature) {
        return DO_NOT_BLOCK_COOLDOWN_GROUP_FEATURES.contains(feature);
    }
}
