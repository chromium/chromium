// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

signin_metrics::ProfilesStatus MakeProfilesStatus(size_t num_opened_profiles,
                                                  size_t num_signed_in_profiles,
                                                  size_t num_syncing_profiles) {
  return {.num_opened_profiles = num_opened_profiles,
          .num_signed_in_profiles = num_signed_in_profiles,
          .num_syncing_profiles = num_syncing_profiles};
}

}  // anonymous namespace

using signin_metrics::SigninOrSyncStatus;

struct TestItem {
  size_t num_opened_profiles;
  size_t num_signed_in_profiles;
  size_t num_syncing_profiles;
  SigninOrSyncStatus expected_signin_status_bucket;
  SigninOrSyncStatus expected_sync_status_bucket;
};

class SigninAndSyncStatusMetricsProviderHelpersTest
    : public ::testing::TestWithParam<TestItem> {};

TEST_P(SigninAndSyncStatusMetricsProviderHelpersTest, HistogramEmit) {
  auto item = GetParam();
  auto profiles_status =
      MakeProfilesStatus(item.num_opened_profiles, item.num_signed_in_profiles,
                         item.num_syncing_profiles);
  base::HistogramTester histogram_tester;
  signin_metrics::EmitHistograms(profiles_status);
  histogram_tester.ExpectUniqueSample("UMA.ProfileSignInStatusV2",
                                      item.expected_signin_status_bucket, 1);
  histogram_tester.ExpectUniqueSample("UMA.ProfileSyncStatusV2",
                                      item.expected_sync_status_bucket, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SigninAndSyncStatusMetricsProviderHelpersTest_HistogramEmit,
    SigninAndSyncStatusMetricsProviderHelpersTest,
    ::testing::Values(TestItem{0, 0, 0, SigninOrSyncStatus::kUnknown,
                               SigninOrSyncStatus::kUnknown},
                      TestItem{1, 0, 0, SigninOrSyncStatus::kNoProfiles,
                               SigninOrSyncStatus::kNoProfiles},
                      TestItem{1, 1, 0, SigninOrSyncStatus::kAllProfiles,
                               SigninOrSyncStatus::kNoProfiles},
                      TestItem{1, 0, 1, SigninOrSyncStatus::kNoProfiles,
                               SigninOrSyncStatus::kAllProfiles},
                      TestItem{1, 1, 1, SigninOrSyncStatus::kAllProfiles,
                               SigninOrSyncStatus::kAllProfiles},
                      TestItem{2, 0, 0, SigninOrSyncStatus::kNoProfiles,
                               SigninOrSyncStatus::kNoProfiles},
                      TestItem{2, 0, 1, SigninOrSyncStatus::kNoProfiles,
                               SigninOrSyncStatus::kMixedProfiles},
                      TestItem{2, 0, 2, SigninOrSyncStatus::kNoProfiles,
                               SigninOrSyncStatus::kAllProfiles},
                      TestItem{2, 1, 0, SigninOrSyncStatus::kMixedProfiles,
                               SigninOrSyncStatus::kNoProfiles},
                      TestItem{2, 1, 1, SigninOrSyncStatus::kMixedProfiles,
                               SigninOrSyncStatus::kMixedProfiles},
                      TestItem{2, 1, 2, SigninOrSyncStatus::kMixedProfiles,
                               SigninOrSyncStatus::kAllProfiles},
                      TestItem{2, 2, 0, SigninOrSyncStatus::kAllProfiles,
                               SigninOrSyncStatus::kNoProfiles},
                      TestItem{2, 2, 1, SigninOrSyncStatus::kAllProfiles,
                               SigninOrSyncStatus::kMixedProfiles},
                      TestItem{2, 2, 2, SigninOrSyncStatus::kAllProfiles,
                               SigninOrSyncStatus::kAllProfiles}));
