// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using testing::Eq;
using testing::HasSubstr;
using testing::IsTrue;

struct IwaVersionTestParam {
  std::string version_string;
  base::expected<std::vector<uint32_t>, IwaVersionParseError>
      expected_components;
};

using IwaVersionTest = testing::TestWithParam<IwaVersionTestParam>;

TEST_P(IwaVersionTest, ParsesSuccessfully) {
  base::expected<base::Version, IwaVersionParseError> version =
      ParseIwaVersion(GetParam().version_string);

  ASSERT_EQ(GetParam().expected_components,
            version.transform([](const base::Version& version) {
              return version.components();
            }));
  if (version.has_value()) {
    EXPECT_THAT(version->IsValid(), IsTrue());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix*/,
    IwaVersionTest,
    testing::ValuesIn(std::vector<IwaVersionTestParam>{
        {.version_string = "1",
         .expected_components = std::vector<uint32_t>{1}},
        {.version_string = "1.2",
         .expected_components = std::vector<uint32_t>{1, 2}},
        {.version_string = "1.2",
         .expected_components = std::vector<uint32_t>{1, 2}},
        {.version_string = "4294967295.4294967294.4294967293",
         .expected_components = std::vector<uint32_t>{4294967295, 4294967294,
                                                      4294967293}},
        // This is bigger than what uint32_t can handle
        {.version_string = "999994294967295.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kCannotConvertToNumber)},
        {.version_string = "0.0.0",
         .expected_components = std::vector<uint32_t>{0, 0, 0}},
        {.version_string = "1.2.3",
         .expected_components = std::vector<uint32_t>{1, 2, 3}},
        {.version_string = "1.2.3.4",
         .expected_components = std::vector<uint32_t>{1, 2, 3, 4}},
        {.version_string = "10.20.30",
         .expected_components = std::vector<uint32_t>{10, 20, 30}},
        {.version_string = "1.-2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1..2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kEmptyComponent)},
        {.version_string = "1..3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kEmptyComponent)},
        {.version_string = "1.--2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.+2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "a.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.a.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3-a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3+a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3-a+a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "01.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)},
        {.version_string = "1.02.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)},
        {.version_string = "1.2.03",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)}}));

using IwaVersionParseErrorToStringTest =
    ::testing::TestWithParam<std::pair<IwaVersionParseError, std::string>>;

TEST_P(IwaVersionParseErrorToStringTest, ConvertsErrorToString) {
  EXPECT_THAT(IwaVersionParseErrorToString(GetParam().first),
              HasSubstr(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaVersionParseErrorToStringTest,
    ::testing::Values(
        std::make_pair(IwaVersionParseError::kNoComponents,
                       "at least one number"),
        std::make_pair(IwaVersionParseError::kEmptyComponent,
                       "may not be empty"),
        std::make_pair(IwaVersionParseError::kNonDigit, "only contain digits"),
        std::make_pair(IwaVersionParseError::kLeadingZero,
                       "not have leading zeros"),
        std::make_pair(IwaVersionParseError::kCannotConvertToNumber,
                       "could not be converted into a number")));

}  // namespace
}  // namespace web_app
