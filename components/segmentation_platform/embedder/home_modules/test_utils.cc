// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/test_utils.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform::home_modules {

using ::testing::Contains;
using ::testing::Not;

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

std::vector<std::string> ExtractCardNames(
    const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards) {
  std::vector<std::string> card_names;
  for (const auto& card : all_cards) {
    if (card) {
      card_names.push_back(card->card_name());
    }
  }
  return card_names;
}

std::vector<std::string> GetSignalKeys(const CardSignalMap& card_signal_map,
                                       const char* card_name) {
  std::vector<std::string> signal_keys;

  auto single_card_signal_map = card_signal_map.find(card_name);
  if (single_card_signal_map != card_signal_map.end()) {
    const auto& signal_map = single_card_signal_map->second;
    for (const auto& signal_pair : signal_map) {
      signal_keys.push_back(signal_pair.first);
    }
  }

  return signal_keys;
}

void ExpectCardRegistered(HomeModulesCardRegistry* registry,
                          const std::string& card_name,
                          const std::vector<std::string>& expected_keys) {
  EXPECT_THAT(registry->all_output_labels(), Contains(card_name));

  std::vector<std::string> card_names =
      ExtractCardNames(registry->get_all_cards_by_priority());
  EXPECT_THAT(card_names, Contains(card_name));

  std::vector<std::string> signal_keys =
      GetSignalKeys(registry->get_card_signal_map(), card_name.c_str());
  for (const auto& key : expected_keys) {
    EXPECT_THAT(signal_keys, Contains(key));
  }
}

void ExpectCardNotRegistered(
    HomeModulesCardRegistry* registry,
    const std::string& card_name,
    const std::vector<std::string>& expected_absent_keys) {
  EXPECT_THAT(registry->all_output_labels(), Not(Contains(card_name)));

  std::vector<std::string> card_names =
      ExtractCardNames(registry->get_all_cards_by_priority());
  EXPECT_THAT(card_names, Not(Contains(card_name)));

  std::vector<std::string> signal_keys =
      GetSignalKeys(registry->get_card_signal_map(), card_name.c_str());
  for (const auto& key : expected_absent_keys) {
    EXPECT_THAT(signal_keys, Not(Contains(key)));
  }
}

}  // namespace segmentation_platform::home_modules
