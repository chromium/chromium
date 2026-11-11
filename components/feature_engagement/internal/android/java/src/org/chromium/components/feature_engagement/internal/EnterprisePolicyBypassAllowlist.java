// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement.internal;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.Set;

/**
 * Allowlist of Feature Engagement features that are permitted to bypass the {@code
 * PromotionsEnabled} enterprise policy on Android.
 *
 * <p>Features should only be added to this list if they are required for legal, regulatory,
 * privacy, or security reasons, or are explicitly user-initiated. Adding a feature to this list
 * requires approval from User Education owners and a {@code
 * [UserEducation.BypassPolicyCheck]=<justification>} tag in the CL description.
 */
@NullMarked
public abstract class EnterprisePolicyBypassAllowlist {
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
                    FeatureConstants.SEARCH_ENGINE_CHOICE_PROMO_FEATURE
                    // keep-sorted end
                    );

    /**
     * Returns whether the given feature is permitted to bypass the {@code PromotionsEnabled}
     * enterprise policy.
     *
     * @param feature The {@link FeatureConstants} name of the feature.
     * @return True if the feature is in the enterprise policy bypass allowlist.
     */
    public static boolean isAllowedToBypassEnterprisePolicy(String feature) {
        return ALLOWLISTED_FEATURES.contains(feature);
    }
}
