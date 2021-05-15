// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIALS_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIALS_H_

#include <vector>

#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A tutorial's identifier. Each defined tutorial has a FeatureTutorial enum
// value.
enum class FeatureTutorial {
  kTabGroups,
};

// Get a string identifier for a FeatureTutorial value. This string cannot be
// used on its own, but may eventually be translated back to a FeatureTutorial
// value. It may be used for identifying a tutorial from a WebUI, for example.
base::StringPiece GetStringIdForFeatureTutorial(FeatureTutorial tutorial);

// Translate a string ID GetStringIdForFeatureTutorial() back to a
// FeatureTutorial.
absl::optional<FeatureTutorial> GetFeatureTutorialFromStringId(
    base::StringPiece id);

// Get the string IDs of all defined tutorials.
std::vector<base::StringPiece> GetAllFeatureTutorialStringIds();

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_TUTORIALS_H_
