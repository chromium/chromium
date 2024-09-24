// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_CONSTANTS_H_

#include <string>

namespace segmentation_platform {

// Identifiers for distinct in-product tips managed by the `TipsManager`.
//
// Each identifier represents a specific tip or variant, allowing the system
// to track, manage, and present the correct tip to the user.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(TipIdentifier)
enum class TipIdentifier {
  // Represents an unknown tip.
  kUnknown = 0,
  // Tip promoting the Lens Search feature.
  kLensSearch = 1,
  // Tip promoting the Lens Shop feature.
  kLensShop = 2,
  // Tip promoting the Lens Translate feature.
  kLensTranslate = 3,
  // Tip about changing the address bar position.
  kAddressBarPosition = 4,
  // Tip encouraging users to save passwords.
  kSavePasswords = 5,
  // Tip promoting the autofill passwords feature.
  kAutofillPasswords = 6,
  // Tip promoting enhanced safe browsing.
  kEnhancedSafeBrowsing = 7,
  kMaxValue = kEnhancedSafeBrowsing,
};
// LINT.ThenChange(/components/segmentation_platform/embedder/home_modules/tips_manager/constants.cc:NameForTipIdentifier)

// Returns the string representation of `tip`.
std::string NameForTipIdentifier(TipIdentifier tip);

// Represents the context in which a tip is presented to the user. This is
// used for tracking, analysis, and potentially adapting tip behavior or
// appearance based on the presentation context.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(TipPresentationContext)
enum class TipPresentationContext {
  // Represents an unknown presentation context.
  kUnknown = 0,
  // Tip presented within the Magic Stack on the Chrome iOS homepage.
  kIOSMagicStack = 1,
  kMaxValue = kIOSMagicStack,
};
// LINT.ThenChange(/components/segmentation_platform/embedder/home_modules/tips_manager/constants.cc:NameForTipPresentationContext)

// Returns the string representation of `context`.
std::string NameForTipPresentationContext(TipPresentationContext context);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_CONSTANTS_H_
