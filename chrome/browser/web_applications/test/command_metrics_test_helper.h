// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_COMMAND_METRICS_TEST_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_COMMAND_METRICS_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/web_applications/commands/command_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app::test {

// Returns all combinations of "ResultCode" install command metrics histograms
// recorded by RecordInstallMetrics.
std::vector<std::string> GetInstallCommandResultHistogramNames(
    std::string_view command_str,
    std::string_view app_type);

// Returns all combinations of "Surface" install command metrics histograms
// recorded by RecordInstallMetrics.
std::vector<std::string> GetInstallCommandSourceHistogramNames(
    std::string_view command_str,
    std::string_view app_type);

// Shortcut matcher to check a list of histograms with the same matcher. Usage:
// EXPECT_THAT(histogram_tester,
//             test::ForAllGetAllSamples(
//                 test::GetInstallCommandResultHistogramNames(
//                     ".FetchManifestAndInstalll", app_type_str),
//                 base::BucketsAre(base::Bucket(
//                     webapps::InstallResultCode::kSuccessNewInstall, 1))));
MATCHER_P2(ForAllGetAllSamples,
           histogram_list,
           matcher,
           base::StrCat({"All buckets from each specified histograms ",
                         testing::DescribeMatcher<std::vector<base::Bucket>>(
                             matcher,
                             negation)})) {
  bool success = true;
  for (const auto& histogram : histogram_list) {
    auto result = arg.GetAllSamples(histogram);
    bool pass = testing::ExplainMatchResult(
        matcher, arg.GetAllSamples(histogram), result_listener);
    if (!pass) {
      *result_listener << " for " << histogram << "\n";
    }
    success = success && pass;
  }
  return success;
}

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_COMMAND_METRICS_TEST_HELPER_H_
