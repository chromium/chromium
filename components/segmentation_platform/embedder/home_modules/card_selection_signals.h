// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_SIGNALS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_SIGNALS_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace segmentation_platform::home_modules {

using SignalKey = std::string;
using CardName = std::string;

// Where the position of the card should be placed.
enum class EphemeralHomeModuleRank { kTop, kLast, kNotShown };

constexpr float EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank rank) {
  switch (rank) {
    case EphemeralHomeModuleRank::kTop:
      return 1;
    case EphemeralHomeModuleRank::kLast:
      return 0.01;
    case EphemeralHomeModuleRank::kNotShown:
      return -1;
  }
}

using CardSignalMap = std::map<CardName, std::map<SignalKey, /*index=*/size_t>>;

// Holds all signals of all the cards in the registry, and a key to fetch each
// card's signal.
struct AllCardSignals {
  AllCardSignals(CardSignalMap signal_map, std::vector<float> signals);
  ~AllCardSignals();

  CardSignalMap signal_map_;
  const std::vector<float> signals_;

  std::optional<float> GetSignal(const CardName& card_name,
                                 const SignalKey& signal_key) const;
};

// Provides the signals needed by each card.
class CardSelectionSignals {
 public:
  CardSelectionSignals(const AllCardSignals* all_signals,
                       const CardName& card_name);
  ~CardSelectionSignals();

  CardSelectionSignals(const CardSelectionSignals&) = delete;
  CardSelectionSignals& operator=(const CardSelectionSignals&) = delete;

  // Returns the result of the signal query corresponding to the `signal_key`.
  // Returns null if the key is not found. The key should always be found, we
  // should consider throwing error instead.
  std::optional<float> GetSignal(const SignalKey& signal_key) const;

 private:
  const raw_ptr<const AllCardSignals> all_signals_;
  const CardName card_name_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CARD_SELECTION_SIGNALS_H_
