// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/constants.h"

#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"

namespace segmentation_platform {

// Returns the `TipIdentifier` corresponding to the given output `label`.
// If the `label` is unknown, returns `TipIdentifier::kUnknown`.
TipIdentifier TipIdentifierForOutputLabel(std::string_view label) {
  if (label == kLensEphemeralModule ||
      label == kLensEphemeralModuleSearchVariation) {
    return TipIdentifier::kLensSearch;
  }

  if (label == kLensEphemeralModuleShopVariation) {
    return TipIdentifier::kLensShop;
  }

  if (label == kLensEphemeralModuleTranslateVariation) {
    return TipIdentifier::kLensTranslate;
  }

  if (label == kAddressBarPositionEphemeralModule) {
    return TipIdentifier::kAddressBarPosition;
  }

  if (label == kSavePasswordsEphemeralModule) {
    return TipIdentifier::kSavePasswords;
  }

  if (label == kAutofillPasswordsEphemeralModule) {
    return TipIdentifier::kAutofillPasswords;
  }

  if (label == kEnhancedSafeBrowsingEphemeralModule) {
    return TipIdentifier::kEnhancedSafeBrowsing;
  }

  return TipIdentifier::kUnknown;
}

std::optional<std::string_view> OutputLabelForTipIdentifier(
    TipIdentifier identifier) {
  switch (identifier) {
    case TipIdentifier::kLensSearch:
      return kLensEphemeralModuleSearchVariation;
    case TipIdentifier::kLensShop:
      return kLensEphemeralModuleShopVariation;
    case TipIdentifier::kLensTranslate:
      return kLensEphemeralModuleTranslateVariation;
    case TipIdentifier::kAddressBarPosition:
      return kAddressBarPositionEphemeralModule;
    case TipIdentifier::kSavePasswords:
      return kSavePasswordsEphemeralModule;
    case TipIdentifier::kAutofillPasswords:
      return kAutofillPasswordsEphemeralModule;
    case TipIdentifier::kEnhancedSafeBrowsing:
      return kEnhancedSafeBrowsingEphemeralModule;
    case TipIdentifier::kUnknown:
      return std::nullopt;
  }
}

}  // namespace segmentation_platform
