// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_tutorials.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const std::pair<FeatureTutorial, const char*> kTutorialIds[] = {
    {FeatureTutorial::kTabGroups, "tab_groups"},
};

}  // namespace

base::StringPiece GetStringIdForFeatureTutorial(FeatureTutorial tutorial) {
  for (const auto& p : kTutorialIds) {
    if (p.first == tutorial)
      return p.second;
  }

  NOTREACHED();
  return "";
}

absl::optional<FeatureTutorial> GetFeatureTutorialFromStringId(
    base::StringPiece id) {
  for (const auto& p : kTutorialIds) {
    if (p.second == id)
      return p.first;
  }

  return absl::nullopt;
}

std::vector<base::StringPiece> GetAllFeatureTutorialStringIds() {
  std::vector<base::StringPiece> result;
  for (const auto& p : kTutorialIds)
    result.push_back(p.second);
  return result;
}
