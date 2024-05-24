// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
enum class TestingErrorEnum { kFirst = 0, kSecond = 1, kMaxValue = kSecond };

struct CustomErrorType {
  std::string message;
  TestingErrorEnum error_enum;
};

TestingErrorEnum ToErrorEnum(CustomErrorType custom_error) {
  return custom_error.error_enum;
}
}  // namespace

// Testing wrapped in base::expected enumeration.
TEST(IsolatedWebAppUmaExpectedLog, Enumeration) {
  const std::string histogram_base_name("Testing.UMA.ExpectedEnumeration");
  const std::string error_histogram = ToErrorHistogramName(histogram_base_name);
  const std::string success_histogram =
      ToSuccessHistogramName(histogram_base_name);
  const base::HistogramTester tester;

  // Log an error.
  const base::expected<void, TestingErrorEnum> status_error_1 =
      base::unexpected(TestingErrorEnum::kFirst);
  UmaLogExpectedStatus(histogram_base_name, status_error_1);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 1);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kSecond), 0);
  tester.ExpectBucketCount(success_histogram, 0, 1);
  tester.ExpectBucketCount(success_histogram, 1, 0);

  // Log the same error one more time.
  UmaLogExpectedStatus(histogram_base_name, status_error_1);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 2);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kSecond), 0);
  tester.ExpectBucketCount(success_histogram, 0, 2);
  tester.ExpectBucketCount(success_histogram, 1, 0);

  // Log another error.
  const base::expected<void, TestingErrorEnum> status_error_2 =
      base::unexpected(TestingErrorEnum::kSecond);
  UmaLogExpectedStatus(histogram_base_name, status_error_2);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 2);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kSecond), 1);
  tester.ExpectBucketCount(success_histogram, 0, 3);
  tester.ExpectBucketCount(success_histogram, 1, 0);

  // Log no error status.
  const base::expected<void, TestingErrorEnum> status_ok = base::ok();
  UmaLogExpectedStatus(histogram_base_name, status_ok);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 2);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kSecond), 1);
  tester.ExpectBucketCount(success_histogram, 0, 3);
  tester.ExpectBucketCount(success_histogram, 1, 1);

  tester.ExpectTotalCount(error_histogram, 3);
  tester.ExpectTotalCount(success_histogram, 4);
}

// Tests of a custom error type wrapped in base::expected.
TEST(IsolatedWebAppUmaExpectedLog, CustomType) {
  const std::string histogram_base_name("Testing.UMA.ExpectedCustomType");
  const std::string error_histogram = ToErrorHistogramName(histogram_base_name);
  const std::string success_histogram =
      ToSuccessHistogramName(histogram_base_name);
  const base::HistogramTester tester;

  const base::expected<void, CustomErrorType> status_error_1 =
      base::unexpected(CustomErrorType{.message = "msg1",
                                       .error_enum = TestingErrorEnum::kFirst});
  UmaLogExpectedStatus(histogram_base_name, status_error_1);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 1);
  tester.ExpectBucketCount(success_histogram, 0, 1);

  const base::expected<void, CustomErrorType> status_ok = base::ok();
  UmaLogExpectedStatus(histogram_base_name, status_ok);
  tester.ExpectBucketCount(error_histogram,
                           static_cast<int>(TestingErrorEnum::kFirst), 1);
  tester.ExpectBucketCount(success_histogram, 0, 1);
  tester.ExpectBucketCount(success_histogram, 1, 1);
}

TEST(IsolatedWebAppUmaExpectedLog, HistogramNaming) {
  EXPECT_EQ(ToErrorHistogramName("Hist.Base.Name"), "Hist.Base.NameError");
  EXPECT_EQ(ToSuccessHistogramName("Hist.Base.Name"), "Hist.Base.NameSuccess");
}

}  // namespace web_app
