// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

TEST(ExtractionUtilsTest, TestReadabilityHeuristicMinScoreAndContentLength) {
  std::string script = GetReadabilityTriggeringScript();
  EXPECT_EQ(std::string::npos, script.find("$$MIN_SCORE_PLACEHOLDER"));
  EXPECT_EQ(std::string::npos, script.find("$$MIN_CONTENT_LENGTH_PLACEHOLDER"));
  std::string expected_args =
      "(" + base::NumberToString(GetReadabilityHeuristicMinScore()) + ", " +
      base::NumberToString(GetReadabilityHeuristicMinContentLength()) + ");";
  EXPECT_NE(std::string::npos, script.find(expected_args));
}

}  // namespace dom_distiller
