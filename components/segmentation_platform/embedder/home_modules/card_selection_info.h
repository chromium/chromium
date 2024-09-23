// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_INFO_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_INFO_H_

#include <optional>
#include <string>
#include <vector>

#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"

namespace segmentation_platform::home_modules {

// Interface implemented by each ephemeral card shown on home modules stack.
class CardSelectionInfo {
 public:
  explicit CardSelectionInfo(const char* card_name);
  virtual ~CardSelectionInfo() = 0;

  struct ShowResult {
    ShowResult();

    // Select position for the default label (card_name).
    explicit ShowResult(EphemeralHomeModuleRank position);
    // Select position with the card label variation.
    ShowResult(EphemeralHomeModuleRank position,
               const std::string& result_label);

    ShowResult(const ShowResult& result);
    ~ShowResult();

    // Where the position of the card should be placed.
    EphemeralHomeModuleRank position;

    // If the card has variations, then the label that corresponds to the
    // variation. If the card does not need variations, this can be empty and
    // the backend would use the `card_name`.
    std::optional<std::string> result_label;
  };

  // Optional implementation, if the Card has multiple variations like
  // different text or simple behavior difference based on the user signals,
  // each of these can be different labels.
  virtual std::vector<std::string> OutputLabels();

  // Returns a list of feature queries mapped by SignalKey. These will be
  // processed and sent to `ComputeCardResult()` to fetch the availability of
  // the card.
  virtual std::map<SignalKey, FeatureQuery> GetInputs() = 0;

  // The logic to decide whether the card should be shown in the home modules.
  virtual ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const = 0;

  const char* card_name() const { return card_name_; }

  CardSelectionInfo(const CardSelectionInfo&) = delete;
  CardSelectionInfo& operator=(const CardSelectionInfo&) = delete;

 protected:
  const char* const card_name_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_INFO_H_
