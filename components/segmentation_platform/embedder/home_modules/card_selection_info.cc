// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"

namespace segmentation_platform::home_modules {

CardSelectionInfo::ShowResult::ShowResult() = default;
CardSelectionInfo::ShowResult::ShowResult(EphemeralHomeModuleRank position)
    : position(position) {}
CardSelectionInfo::ShowResult::ShowResult(EphemeralHomeModuleRank position,
                                          const std::string& result_label)
    : position(position), result_label(result_label) {}
CardSelectionInfo::ShowResult::ShowResult(const ShowResult& result) = default;
CardSelectionInfo::ShowResult::~ShowResult() = default;

CardSelectionInfo::CardSelectionInfo(const char* card_name)
    : card_name_(card_name) {}
CardSelectionInfo::~CardSelectionInfo() = default;

std::vector<std::string> CardSelectionInfo::OutputLabels() {
  return {card_name_};
}

}  // namespace segmentation_platform::home_modules
