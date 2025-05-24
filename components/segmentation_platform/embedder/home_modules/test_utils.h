// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_UTILS_H_

#include <vector>

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"

namespace segmentation_platform::home_modules {

// Helper function to create an `AllCardSignals` object for the given
// `CardSelectionInfo` and signal values. The `CardSignalMap` is populated
// with the required signals obtained from the `CardSelectionInfo`'s `GetInputs`
// method.
AllCardSignals CreateAllCardSignals(CardSelectionInfo* card,
                                    const std::vector<float>& signal_values);

// Helper function to extract the card names from a vector of CardSelectionInfo
// objects.
std::vector<std::string> ExtractCardNames(
    const std::vector<std::unique_ptr<CardSelectionInfo>>& all_cards);

// Helper function to retrieve all SignalKeys associated with a given CardName
// in the CardSignalMap.
std::vector<std::string> GetSignalKeys(const CardSignalMap& cardSignalMap,
                                       const char* cardName);

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TEST_UTILS_H_
