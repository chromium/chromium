// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

// Selection logic for promo to enable auxiliary search.
class AuxiliarySearchPromo : public CardSelectionInfo {
 public:
  explicit AuxiliarySearchPromo();
  ~AuxiliarySearchPromo() override;

  static bool IsEnabled(int impression_count);

  AuxiliarySearchPromo(const AuxiliarySearchPromo&) = delete;
  AuxiliarySearchPromo& operator=(const AuxiliarySearchPromo&) = delete;

  // CardSelectionInfo
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_AUXILIARY_SEARCH_PROMO_H_
