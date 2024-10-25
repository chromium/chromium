// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/test_utils.h"

#include <string_view>

namespace segmentation_platform::home_modules {

AllCardSignals CreateAllCardSignals(CardSelectionInfo* card,
                                    const std::vector<float>& signal_values) {
  CardSignalMap card_signal_map;

  std::map<SignalKey, size_t> card_signals;

  size_t input_counter = 0;

  const auto& card_inputs = card->GetInputs();

  for (const auto& key_and_input : card_inputs) {
    card_signals[key_and_input.first] = input_counter;
    input_counter++;
  }

  card_signal_map[card->card_name()] = card_signals;

  return AllCardSignals(card_signal_map, signal_values);
}

}  // namespace segmentation_platform::home_modules
