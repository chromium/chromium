// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_metrics {

struct TestItem {
  ProfilesStatus profiles_status;
  SigninOrSyncStatus expected_signin_status_bucket;
  SigninOrSyncStatus expected_sync_status_bucket;
};

class SigninAndSyncStatusMetricsProviderHelpersTest
    : public ::testing::TestWithParam<TestItem> {};

TEST_P(SigninAndSyncStatusMetricsProviderHelpersTest, HistogramEmit) {
  const TestItem& item = GetParam();
  base::HistogramTester histogram_tester;
  signin_metrics::EmitHistograms(item.profiles_status);
  histogram_tester.ExpectUniqueSample("UMA.ProfileSignInStatusV2",
                                      item.expected_signin_status_bucket, 1);
  histogram_tester.ExpectUniqueSample("UMA.ProfileSyncStatusV2",
                                      item.expected_sync_status_bucket, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SigninAndSyncStatusMetricsProviderHelpersTest_HistogramEmit,
    SigninAndSyncStatusMetricsProviderHelpersTest,
    ::testing::Values(
        TestItem{{},
                 SigninOrSyncStatus::kUnknown,
                 SigninOrSyncStatus::kUnknown},
        TestItem{{.num_opened_profiles = 1},
                 SigninOrSyncStatus::kNoProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 1, .num_signed_in_profiles = 1},
                 SigninOrSyncStatus::kAllProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 1, .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kNoProfiles,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{{.num_opened_profiles = 1,
                  .num_signed_in_profiles = 1,
                  .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kAllProfiles,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{{.num_opened_profiles = 2},
                 SigninOrSyncStatus::kNoProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 2, .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kNoProfiles,
                 SigninOrSyncStatus::kMixedProfiles},
        TestItem{{.num_opened_profiles = 2, .num_syncing_profiles = 2},
                 SigninOrSyncStatus::kNoProfiles,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{{.num_opened_profiles = 2, .num_signed_in_profiles = 1},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 1,
                  .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kMixedProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 1,
                  .num_syncing_profiles = 2},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{{.num_opened_profiles = 2, .num_signed_in_profiles = 2},
                 SigninOrSyncStatus::kAllProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 2,
                  .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kAllProfiles,
                 SigninOrSyncStatus::kMixedProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 2,
                  .num_syncing_profiles = 2},
                 SigninOrSyncStatus::kAllProfiles,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{
            {.num_opened_profiles = 2, .num_signed_in_profiles_with_error = 2},
            SigninOrSyncStatus::kAllProfilesInError,
            SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 1,
                  .num_signed_in_profiles_with_error = 1},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kNoProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles_with_error = 2,
                  .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kAllProfilesInError,
                 SigninOrSyncStatus::kMixedProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 1,
                  .num_signed_in_profiles_with_error = 1,
                  .num_syncing_profiles = 1},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kMixedProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles_with_error = 2,
                  .num_syncing_profiles = 2},
                 SigninOrSyncStatus::kAllProfilesInError,
                 SigninOrSyncStatus::kAllProfiles},
        TestItem{{.num_opened_profiles = 2,
                  .num_signed_in_profiles = 1,
                  .num_signed_in_profiles_with_error = 1,
                  .num_syncing_profiles = 2},
                 SigninOrSyncStatus::kMixedProfiles,
                 SigninOrSyncStatus::kAllProfiles}));

TEST(SigninAndSyncStatusMetricsProviderHelpersTest,
     UpdateProfilesStatusBasedOnSignInAndSyncStatus) {
  ProfilesStatus status;
  UpdateProfilesStatusBasedOnSignInAndSyncStatus(
      status, SingleProfileSigninStatus::kSignedOut,
      /*syncing=*/false);
  EXPECT_THAT(
      status,
      testing::AllOf(
          testing::Field(&ProfilesStatus::num_opened_profiles, 1),
          testing::Field(&ProfilesStatus::num_signed_in_profiles, 0),
          testing::Field(&ProfilesStatus::num_signed_in_profiles_with_error, 0),
          testing::Field(&ProfilesStatus::num_syncing_profiles, 0)));

  UpdateProfilesStatusBasedOnSignInAndSyncStatus(
      status, SingleProfileSigninStatus::kSignedInWithError,
      /*syncing=*/false);
  EXPECT_THAT(
      status,
      testing::AllOf(
          testing::Field(&ProfilesStatus::num_opened_profiles, 2),
          testing::Field(&ProfilesStatus::num_signed_in_profiles, 0),
          testing::Field(&ProfilesStatus::num_signed_in_profiles_with_error, 1),
          testing::Field(&ProfilesStatus::num_syncing_profiles, 0)));

  UpdateProfilesStatusBasedOnSignInAndSyncStatus(
      status, SingleProfileSigninStatus::kSignedIn,
      /*syncing=*/false);
  EXPECT_THAT(
      status,
      testing::AllOf(
          testing::Field(&ProfilesStatus::num_opened_profiles, 3),
          testing::Field(&ProfilesStatus::num_signed_in_profiles, 1),
          testing::Field(&ProfilesStatus::num_signed_in_profiles_with_error, 1),
          testing::Field(&ProfilesStatus::num_syncing_profiles, 0)));

  UpdateProfilesStatusBasedOnSignInAndSyncStatus(
      status, SingleProfileSigninStatus::kSignedOut,
      /*syncing=*/false);
  EXPECT_THAT(
      status,
      testing::AllOf(
          testing::Field(&ProfilesStatus::num_opened_profiles, 4),
          testing::Field(&ProfilesStatus::num_signed_in_profiles, 1),
          testing::Field(&ProfilesStatus::num_signed_in_profiles_with_error, 1),
          testing::Field(&ProfilesStatus::num_syncing_profiles, 0)));

  // This scenario cannot happen in practice.
  UpdateProfilesStatusBasedOnSignInAndSyncStatus(
      status, SingleProfileSigninStatus::kSignedOut,
      /*syncing=*/true);
  EXPECT_THAT(
      status,
      testing::AllOf(
          testing::Field(&ProfilesStatus::num_opened_profiles, 5),
          testing::Field(&ProfilesStatus::num_signed_in_profiles, 1),
          testing::Field(&ProfilesStatus::num_signed_in_profiles_with_error, 1),
          testing::Field(&ProfilesStatus::num_syncing_profiles, 1)));
}

}  // namespace signin_metrics
