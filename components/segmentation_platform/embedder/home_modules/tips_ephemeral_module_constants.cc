// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"

#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"

namespace segmentation_platform::home_modules {

TipIdentifier TipIdentifierForOutputLabel(std::string_view label) {
  if (label == kTipsLensSearchVariation) {
    return TipIdentifier::kLensSearch;
  }

  if (label == kTipsLensShopVariation) {
    return TipIdentifier::kLensShop;
  }

  if (label == kTipsLensTranslateVariation) {
    return TipIdentifier::kLensTranslate;
  }

  if (label == kTipsAddressBarPositionVariation) {
    return TipIdentifier::kAddressBarPosition;
  }

  if (label == kTipsSavePasswordsVariation) {
    return TipIdentifier::kSavePasswords;
  }

  if (label == kTipsAutofillPasswordsVariation) {
    return TipIdentifier::kAutofillPasswords;
  }

  if (label == kTipsEnhancedSafeBrowsingVariation) {
    return TipIdentifier::kEnhancedSafeBrowsing;
  }

  return TipIdentifier::kUnknown;
}

}  // namespace segmentation_platform::home_modules
