// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/types.h"

#include <string>

#include "base/base64.h"
#include "base/base64url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Optional;

namespace {

constexpr char kInvalidBase64String[] = "$$$";
constexpr char kBase64StringWithSlash[] = "abc/";
constexpr char kBase64StringWithPlus[] = "abc+";
constexpr char kBase64StringWithPadding[] = "aQ==";

}  // namespace

TEST(QuickStartTypesTest,
     Base64UrlTranscodeReturnsAnEmptyOptionalForInvalidInput) {
  absl::optional<Base64UrlString> result =
      Base64UrlTranscode(Base64String(kInvalidBase64String));
  EXPECT_THAT(result, Eq(absl::nullopt));
}

TEST(QuickStartTypesTest, Base64UrlTranscodeSubstitutesSlash) {
  absl::optional<Base64UrlString> result =
      Base64UrlTranscode(Base64String(kBase64StringWithSlash));
  ASSERT_THAT(result, Ne(absl::nullopt));

  const std::string& result_str = *result.value();
  EXPECT_THAT(result_str, Not(HasSubstr("/")));
}

TEST(QuickStartTypesTest, Base64UrlTranscodeSubstitutesPlus) {
  absl::optional<Base64UrlString> result =
      Base64UrlTranscode(Base64String(kBase64StringWithPlus));
  ASSERT_THAT(result, Ne(absl::nullopt));

  const std::string& result_str = *result.value();
  EXPECT_THAT(result_str, Not(HasSubstr("+")));
}

TEST(QuickStartTypesTest, Base64UrlTranscodeOmitsPadding) {
  absl::optional<Base64UrlString> result =
      Base64UrlTranscode(Base64String(kBase64StringWithPadding));
  ASSERT_THAT(result, Ne(absl::nullopt));

  const std::string& result_str = *result.value();
  EXPECT_THAT(result_str, Not(HasSubstr("=")));
}

TEST(QuickStartTypesTest, Base64UrlTranscodeWorksWithPaddedInput) {
  absl::optional<Base64UrlString> b64url_result =
      Base64UrlTranscode(Base64String(kBase64StringWithPadding));
  ASSERT_THAT(b64url_result, Ne(absl::nullopt));

  // The underlying decoded data must be the same.
  EXPECT_THAT(
      base::Base64UrlDecode(*b64url_result.value(),
                            base::Base64UrlDecodePolicy::DISALLOW_PADDING),
      Eq(base::Base64Decode(kBase64StringWithPadding)));
}

}  // namespace ash::quick_start
