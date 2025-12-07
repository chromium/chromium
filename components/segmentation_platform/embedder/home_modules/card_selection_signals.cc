// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"

#include "base/check.h"

namespace segmentation_platform::home_modules {

AllCardSignals::AllCardSignals(CardSignalMap signal_map,
                               std::vector<float> signals)
    : signal_map_(std::move(signal_map)), signals_(std::move(signals)) {}

AllCardSignals::~AllCardSignals() = default;

std::optional<float> AllCardSignals::GetSignal(
    const CardName& card_name,
    const SignalKey& signal_key) const {
  auto card_it = signal_map_.find(card_name);
  if (card_it != signal_map_.end()) {
    auto signal_it = card_it->second.find(signal_key);
    if (signal_it != card_it->second.end()) {
      size_t index = signal_it->second;
      CHECK(index < signals_.size());
      return signals_[index];
    }
  }
  return std::nullopt;
}

CardSelectionSignals::CardSelectionSignals(const AllCardSignals* all_signals,
                                           const CardName& card_name)
    : all_signals_(all_signals), card_name_(card_name) {}

CardSelectionSignals::~CardSelectionSignals() = default;

std::optional<float> CardSelectionSignals::GetSignal(
    const SignalKey& signal_key) const {
  return all_signals_->GetSignal(card_name_, signal_key);
}

}  // namespace segmentation_platform::home_modules
