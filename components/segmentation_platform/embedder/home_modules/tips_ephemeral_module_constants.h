// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_CONSTANTS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

namespace segmentation_platform::home_modules {

// `TipsEphemeralModule` Variation Labels.

// Variation that promotes the autofill passwords
// feature.
static constexpr char kTipsAutofillPasswordsVariation[] =
    "tips_autofill_passwords_variation";

// Variation that promotes changing the address bar
// position.
static constexpr char kTipsAddressBarPositionVariation[] =
    "tips_address_bar_position_variation";

// Variation that promotes Enhanced Safe Browsing.
static constexpr char kTipsEnhancedSafeBrowsingVariation[] =
    "tips_enhanced_safe_browsing_variation";

// Variation that promotes the Lens Search feature.
static constexpr char kTipsLensSearchVariation[] = "tips_lens_search_variation";

// Variation that promotes the Lens Shop feature.
static constexpr char kTipsLensShopVariation[] = "tips_lens_shop_variation";

// Variation that promotes the Lens Translate feature.
static constexpr char kTipsLensTranslateVariation[] =
    "tips_lens_translate_variation";

// Variation that promotes saving passwords.
static constexpr char kTipsSavePasswordsVariation[] =
    "tips_save_passwords_variation";

// The set of variation labels that correspond to `TipsEphemeralModule`.
inline constexpr auto kTipsOutputLabels =
    base::MakeFixedFlatSet<std::string_view>({
        kTipsAddressBarPositionVariation,
        kTipsAutofillPasswordsVariation,
        kTipsEnhancedSafeBrowsingVariation,
        kTipsLensSearchVariation,
        kTipsLensShopVariation,
        kTipsLensTranslateVariation,
        kTipsSavePasswordsVariation,
    });

// Returns the `TipIdentifier` corresponding to the given output `label`.
// If the `label` is unknown, returns `TipIdentifier::kUnknown`.
TipIdentifier TipIdentifierForOutputLabel(std::string_view label);

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_EPHEMERAL_MODULE_CONSTANTS_H_
