// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <string>
#include <string_view>
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

struct HandwritingLocaleToDlcTestCase {
  std::string test_name;
  std::string_view locale;
  absl::optional<std::string> expected;
};

class HandwritingLocaleToDlcTest
    : public HandwritingTest,
      public testing::WithParamInterface<HandwritingLocaleToDlcTestCase> {};

TEST_P(HandwritingLocaleToDlcTest, Test) {
  const HandwritingLocaleToDlcTestCase& test_case = GetParam();

  EXPECT_EQ(HandwritingLocaleToDlc(test_case.locale), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    HandwritingLocaleToDlcTests,
    HandwritingLocaleToDlcTest,
    testing::ValuesIn<HandwritingLocaleToDlcTestCase>(
        {{"InvalidEmpty", "", absl::nullopt},
         {"InvalidEn", "en", absl::nullopt},
         {"InvalidDeDe", "de-DE", absl::nullopt},
         {"InvalidCy", "cy", absl::nullopt},
         {"ValidDe", "de", "handwriting-de"},
         {"ValidZhHk", "zh-HK", "handwriting-zh-HK"}}),
    [](const testing::TestParamInfo<HandwritingLocaleToDlcTest::ParamType>&
           info) { return info.param.test_name; });

struct IsHandwritingDlcTestCase {
  std::string test_name;
  std::string_view dlc_id;
  bool expected;
};

class IsHandwritingDlcTest
    : public HandwritingTest,
      public testing::WithParamInterface<IsHandwritingDlcTestCase> {};

TEST_P(IsHandwritingDlcTest, Test) {
  const IsHandwritingDlcTestCase& test_case = GetParam();

  EXPECT_EQ(IsHandwritingDlc(test_case.dlc_id), test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    IsHandwritingDlcTests,
    IsHandwritingDlcTest,
    testing::ValuesIn<IsHandwritingDlcTestCase>(
        {{"InvalidEmpty", "", false},
         {"InvalidEn", "handwriting-en", false},
         {"InvalidCy", "handwriting-cy", false},
         {"InvalidDeDe", "handwriting-de-DE", false},
         {"InvalidTypoDe", "handwritting-de", false},
         {"InvalidTtsEnUs", "tts-en-us", false},
         {"InvalidDeWithoutPrefix", "de", false},
         {"ValidDe", "handwriting-de", true},
         {"ValidZhHk", "handwriting-zh-HK", true}}),
    [](const testing::TestParamInfo<IsHandwritingDlcTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace ash::language_packs
