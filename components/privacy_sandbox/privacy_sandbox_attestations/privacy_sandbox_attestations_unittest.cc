// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/version.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/privacy_sandbox_attestations_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace privacy_sandbox {

namespace {

class MockAttestationsObserver
    : public content::PrivacySandboxAttestationsObserver {
 public:
  MOCK_METHOD(void, OnAttestationsLoaded, (), (override));
};

}  // namespace

class PrivacySandboxAttestationsTestBase : public testing::Test {
 public:
  PrivacySandboxAttestationsTestBase()
      : scoped_attestations_(PrivacySandboxAttestations::CreateForTesting()) {}

 protected:
  using Status = PrivacySandboxSettingsImpl::Status;

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  void WriteAttestationsFileAndWaitForLoading(base::Version version,
                                              std::string_view content) {
    base::ScopedTempDir component_install_dir;
    CHECK(
        component_install_dir.CreateUniqueTempDirUnderPath(GetTestDirectory()));
    // Note the actual attestations file name required by the Privacy Sandbox
    // Attestations component is specified by
    // `kPrivacySandboxAttestationsFileName`. Here because this is a unit test
    // so it can be any file name.
    base::FilePath attestations_file_path =
        component_install_dir.GetPath().Append(
            FILE_PATH_LITERAL("attestations"));
    CHECK(base::WriteFile(attestations_file_path, content));

    base::RunLoop run_loop;
    PrivacySandboxAttestations::GetInstance()
        ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

    PrivacySandboxAttestations::GetInstance()->LoadAttestations(
        version, attestations_file_path, /*is_pre_installed=*/false);
    run_loop.Run();
  }

  void WriteAttestationsFileAndPauseDuringParsing(base::Version version,
                                                  std::string_view content) {
    base::ScopedTempDir component_install_dir;
    CHECK(
        component_install_dir.CreateUniqueTempDirUnderPath(GetTestDirectory()));
    // Note the actual attestations file name required by the Privacy Sandbox
    // Attestations component is specified by
    // `kPrivacySandboxAttestationsFileName`. Here because this is a unit test
    // so it can be any file name.
    base::FilePath attestations_file_path =
        component_install_dir.GetPath().Append(
            FILE_PATH_LITERAL("attestations"));
    CHECK(base::WriteFile(attestations_file_path, content));

    base::RunLoop run_loop;
    PrivacySandboxAttestations::GetInstance()
        ->SetLoadAttestationsParsingStartedCallbackForTesting(
            run_loop.QuitClosure());

    PrivacySandboxAttestations::GetInstance()->LoadAttestations(
        version, attestations_file_path, /*is_pre_installed=*/false);
    run_loop.Run();
  }

  const base::FilePath& GetTestDirectory() const {
    return scoped_temp_dir_.GetPath();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  ScopedPrivacySandboxAttestations scoped_attestations_;
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrivacySandboxAttestationsTestBase, AddOverride) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

class PrivacySandboxAttestationsFeatureEnabledTest
    : public base::test::WithFeatureOverride,
      public PrivacySandboxAttestationsTestBase {
 public:
  PrivacySandboxAttestationsFeatureEnabledTest()
      : base::test::WithFeatureOverride(
            kDefaultAllowPrivacySandboxAttestations) {
    scoped_feature_list_.InitAndEnableFeature(
        kEnforcePrivacySandboxAttestations);
  }

  void SetUp() override {
    PrivacySandboxAttestationsTestBase::SetUp();

    // Reset the singleton recorder to avoid interference across test cases.
    startup_metric_utils::GetBrowser().ResetSessionForTesting();
  }

  bool IsAttestationsDefaultAllowed() const { return IsParamFeatureEnabled(); }

  // Return the final expected status of `IsSiteAttested` given the `status`
  // which represents the actual status of the attestation.
  Status GetExpectedStatus(Status status) {
    // If the attestations map is absent and feature
    // `kDefaultAllowPrivacySandboxAttestations` is on, the expected status is
    // default allow when the given status implies the map is absent.
    if (IsAttestationsDefaultAllowed() &&
        (status == Status::kAttestationsFileNotPresent ||
         status == Status::kAttestationsDownloadedNotYetLoaded ||
         status == Status::kAttestationsFileCorrupt ||
         status == Status::kAttestationsFileNotYetChecked)) {
      return Status::kAllowed;
    }

    return status;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       AttestationsBehaviorWithMapBeingAbsent) {
  net::SchemefulSite site(GURL("https://example.com"));

  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);
  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 0);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       DefaultDenyIfAttestationsComponentNotYetReady) {
  net::SchemefulSite site(GURL("https://example.com"));

  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest, AttestedIfOverridden) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       EnrolledWithoutAttestations) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{{site, {}}});

  Status new_attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(new_attestation_status,
            GetExpectedStatus(Status::kAttestationFailed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationFailed, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest, EnrolledAndAttested) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{
          {site, PrivacySandboxAttestationsGatedAPISet{
                     PrivacySandboxAttestationsGatedAPI::kTopics}}});

  Status new_attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(new_attestation_status, GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       NonExistentAttestationsFile) {
  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Call the parsing function with a non-existent file.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath(), /*is_pre_installed=*/false);
  run_loop.Run();

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kFileNotExist, 1);
}

// The parsing progress may end up being
// `PrivacySandboxAttestations::Progress::kFinished` but there is no in-memory
// attestations map. Verify that the second attempt to parse should not crash.
TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       TryParseNonExistentAttestationsFileTwice) {
  base::RunLoop first_attempt;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(first_attempt.QuitClosure());

  // Call the parsing function with a non-existent file.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath(), /*is_pre_installed=*/false);
  first_attempt.Run();

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kFileNotExist, 1);

  base::RunLoop second_attempt;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(second_attempt.QuitClosure());
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath(), /*is_pre_installed=*/false);
  second_attempt.Run();

  // The parsing should fail again, without crashes.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kFileNotExist, 2);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       InvalidAttestationsFileIsNotLoaded) {
  // Write an invalid proto file, and try to parse it.
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         "invalid proto");

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kCannotParseFile, 1);

  // Attempts to check attestation status should return that the file is
  // corrupt.
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileCorrupt));
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest, LoadAttestationsFile) {
  MockAttestationsObserver observer;
  base::ScopedObservation<PrivacySandboxAttestations,
                          content::PrivacySandboxAttestationsObserver>
      observation(&observer);
  observation.Observe(PrivacySandboxAttestations::GetInstance());

  EXPECT_CALL(observer, OnAttestationsLoaded).Times(2);

  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.1"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       1);
  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kDownloaded, 1);
  // For API not in the attestations list, the result should be
  // `kAttestationFailed`.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            GetExpectedStatus(Status::kAttestationFailed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationFailed, 1);
  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kDownloaded, 2);

  // Add attestation for Protected Audience.
  site_attestation.add_attested_apis(PROTECTED_AUDIENCE);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  // Parse the new version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.2"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 2);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 2);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 2);

  // Now the site should be attested for both APIs.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.2"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 4);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       2);
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 5);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       3);
  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 4);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kDownloaded, 4);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       AttestationFirstCheckTimeHistogram) {
  histogram_tester().ExpectTotalCount(kAttestationFirstCheckTimeUMA, 0);

  std::string site = "https://example.com";
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationFirstCheckTimeUMA, 1);

  // The first attestation check histogram should be only recorded once for each
  // Chrome session.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectTotalCount(kAttestationFirstCheckTimeUMA, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       LoadAttestationsFilePauseDuringParsing) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  WriteAttestationsFileAndPauseDuringParsing(base::Version("0.0.1"),
                                             serialized_proto);

  // The attestation check should return an error indicating that parsing is in
  // progress.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsDownloadedNotYetLoaded));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsDownloadedNotYetLoaded, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       OlderVersionAttestationsFileIsNotLoaded) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  WriteAttestationsFileAndWaitForLoading(base::Version("1.2.3"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("1.2.3"));
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       1);

  // Clear the proto attestations.
  proto.clear_site_attestations();

  // Load the attestations file, which has an older version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kNotNewerVersion, 1);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The attestations map should still be the old one.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("1.2.3"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       2);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       NewerVersionAttestationsFileIsLoaded) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.1"));
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       1);

  // Clear the attestations.
  proto.clear_site_attestations();

  // Load the attestations file, which has a newer version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.2"),
                                         serialized_proto);
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 2);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 2);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 2);

  // The newer version should override the existing attestations map.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.2"));

  // The site should not be attested for the API according to the new
  // attestations map.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationFailed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationFailed, 1);
}

// Test that the parsing works as expected when there are combining characters
// in the file path to the attestation list. There are exhaustive tests on this
// in `base/files/file_path_unittest.cc`.
TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       CombiningCharacterFilePath) {
  MockAttestationsObserver observer;
  base::ScopedObservation<PrivacySandboxAttestations,
                          content::PrivacySandboxAttestationsObserver>
      observation(&observer);
  observation.Observe(PrivacySandboxAttestations::GetInstance());

  EXPECT_CALL(observer, OnAttestationsLoaded).Times(1);

  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetChecked));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA, Status::kAttestationsFileNotYetChecked, 1);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  base::ScopedTempDir scoped_temp_dir;
  CHECK(scoped_temp_dir.CreateUniqueTempDirUnderPath(GetTestDirectory()));
  // Create an install directory that contains combining characters.
  base::FilePath component_install_dir = scoped_temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("k\u0301u\u032Do\u0304\u0301n"));
  CHECK(base::CreateDirectory(component_install_dir));
  base::FilePath attestations_file_path =
      component_install_dir.Append(FILE_PATH_LITERAL("attestations"));
  CHECK(base::WriteFile(attestations_file_path, serialized_proto));

  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("2023.1.23.0"), attestations_file_path,
      /*is_pre_installed=*/false);
  run_loop.Run();

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  histogram_tester().ExpectTotalCount(kAttestationsFileParsingTimeUMA, 1);
  histogram_tester().ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("2023.1.23.0"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAllowed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA, Status::kAllowed,
                                       1);
  // For API not in the attestations list, the result should be
  // `kAttestationFailed`.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            GetExpectedStatus(Status::kAttestationFailed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationFailed, 1);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PrivacySandboxAttestationsFeatureEnabledTest);

}  // namespace privacy_sandbox
