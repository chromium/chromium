// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class HandwritingTest : public testing::Test {};

absl::optional<std::string> GetSecondUnderscorePart(
    const std::string& engine_id) {
  std::vector<std::string> split = base::SplitString(
      engine_id, "_", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2) {
    return absl::nullopt;
  } else {
    return split[1];
  }
}

TEST_F(HandwritingTest, EngineIdsToHandwritingLocalesNoInput) {
  EXPECT_THAT(
      EngineIdsToHandwritingLocales(
          {}, base::BindRepeating([](const std::string& unused_engine_id)
                                      -> absl::optional<std::string> {
            ADD_FAILURE() << "engine_id_to_handwriting_locale was called";
            return "en";
          })),
      IsEmpty());
}

TEST_F(HandwritingTest, EngineIdsToHandwritingLocalesAllToNullopt) {
  EXPECT_THAT(EngineIdsToHandwritingLocales(
                  {{"qwerty_en", "qwertz_de"}},
                  base::BindRepeating([](const std::string& unused_engine_id)
                                          -> absl::optional<std::string> {
                    return absl::nullopt;
                  })),
              IsEmpty());
}

TEST_F(HandwritingTest, EngineIdsToHandwritingLocalesAllToUniqueStrings) {
  EXPECT_THAT(EngineIdsToHandwritingLocales(
                  {{"qwerty_en", "qwertz_de"}},
                  base::BindRepeating(GetSecondUnderscorePart)),
              UnorderedElementsAre("en", "de"));
}

TEST_F(HandwritingTest, EngineIdsToHandwritingLocalesRepeatedString) {
  EXPECT_THAT(EngineIdsToHandwritingLocales(
                  {{"qwerty_en", "qzertz_de", "qwertz_en"}},
                  base::BindRepeating(GetSecondUnderscorePart)),
              UnorderedElementsAre("en", "de"));
}

TEST_F(HandwritingTest, EngineIdsToHandwritingLocalesSomeNullopt) {
  EXPECT_THAT(EngineIdsToHandwritingLocales(
                  {{"qwerty_en", "nohandwriting", "qwertz_de"}},
                  base::BindRepeating(GetSecondUnderscorePart)),
              UnorderedElementsAre("en", "de"));
}

}  // namespace

}  // namespace ash::language_packs
