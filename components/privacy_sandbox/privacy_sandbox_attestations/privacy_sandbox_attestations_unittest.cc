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
        version, attestations_file_path);
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
        version, attestations_file_path);
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

  bool IsAttestationsDefaultAllowed() { return IsParamFeatureEnabled(); }

  // Return the final expected status of `IsSiteAttested` given the `status`
  // which represents the actual status of the attestation.
  Status GetExpectedStatus(Status status) {
    // If the attestations map is absent and feature
    // `kDefaultAllowPrivacySandboxAttestations` is on, the expected status is
    // default allow when the given status implies the map is absent.
    if (IsAttestationsDefaultAllowed() &&
        (status == Status::kAttestationsFileNotYetReady ||
         status == Status::kAttestationsDownloadedNotYetLoaded ||
         status == Status::kAttestationsFileCorrupt)) {
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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       DefaultDenyIfAttestationsMapNotPresent) {
  net::SchemefulSite site(GURL("https://example.com"));

  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest, AttestedIfOverridden) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_EQ(attestation_status,
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
      base::Version("0.0.1"), base::FilePath());
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
      base::Version("0.0.1"), base::FilePath());
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
      base::Version("0.0.1"), base::FilePath());
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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
  // For API not in the attestations list, the result should be
  // `kAttestationFailed`.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            GetExpectedStatus(Status::kAttestationFailed));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationFailed, 1);

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
}

TEST_P(PrivacySandboxAttestationsFeatureEnabledTest,
       LoadAttestationsFilePauseDuringParsing) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
            GetExpectedStatus(Status::kAttestationsFileNotYetReady));
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationStatusUMA,
                                       Status::kAttestationsFileNotYetReady, 1);

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
      base::Version("2023.1.23.0"), attestations_file_path);
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

class PrivacySandboxAttestationsSentinelTest
    : public base::test::WithFeatureOverride,
      public PrivacySandboxAttestationsTestBase {
 public:
  PrivacySandboxAttestationsSentinelTest()
      : base::test::WithFeatureOverride(kPrivacySandboxAttestationSentinel) {
    // Enforce the enrollment with the default-deny behavior.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kEnforcePrivacySandboxAttestations},
        /*disabled_features=*/{kDefaultAllowPrivacySandboxAttestations});
  }

  bool IsSentinelGuardEnabled() { return IsParamFeatureEnabled(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// If the sentinel feature is enabled, when parsing fails or crashes, a sentinel
// file is left in the installation direction. This file prevents further
// parsing attempts.
TEST_P(PrivacySandboxAttestationsSentinelTest,
       SentinelPreventsSubsequentParsingAfterCrashOrFailure) {
  // Write an invalid proto file, and try to parse it. Note here we are not
  // using `WriteAttestationsFileAndWaitForLoading()` because we need the second
  // attempt to parse to be in the same installation directory.
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  // Note the actual attestations file name required by the Privacy Sandbox
  // Attestations component is specified by
  // `kPrivacySandboxAttestationsFileName`. Here because this is a unit test so
  // it can be any file name.
  base::FilePath invalid_attestations_file_path =
      install_dir.GetPath().Append(FILE_PATH_LITERAL("attestations"));
  ASSERT_TRUE(base::WriteFile(invalid_attestations_file_path, "Invalid proto"));
  ASSERT_FALSE(
      base::PathExists(install_dir.GetPath().Append(kSentinelFileName)));

  base::RunLoop parsing_invalid_attestations;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_invalid_attestations.QuitClosure());

  // Load an attestations file that is invalid.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), invalid_attestations_file_path);
  parsing_invalid_attestations.Run();

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kCannotParseFile, 1);

  // Attempts to check attestation status should return that the file is
  // corrupt.
  std::string site = "https://example.com";
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileCorrupt);

  // If sentinel feature is enabled, failed parsing creates a sentinel file,
  // which prevents subsequent attempts to parse the attestations file. Since it
  // is difficult to check the parsing will not take place with a sentinel file
  // present, we parse again with a valid attestations file. By checking the
  // attestations map is still absent after attempts to load, we can verify that
  // the sentinel file is working as intended. Note: We do not overwrite the
  // previous attestations file because it will make this test flaky on windows
  // bot.
  ASSERT_EQ(base::PathExists(install_dir.GetPath().Append(kSentinelFileName)),
            IsSentinelGuardEnabled());

  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  // Create the new valid attestations file in the same directory of the
  // previous attestations.
  base::FilePath valid_attestations_file_path =
      install_dir.GetPath().Append(FILE_PATH_LITERAL("valid_attestations"));

  // Write the valid serialized proto to the attestations file.
  ASSERT_TRUE(base::WriteFile(valid_attestations_file_path, serialized_proto));

  // Try to load again, use the valid attestations file instead.
  base::RunLoop parsing_valid_attestations_with_sentinel;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_valid_attestations_with_sentinel.QuitClosure());
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), valid_attestations_file_path);
  parsing_valid_attestations_with_sentinel.Run();

  // If sentinel feature is enabled, the parsing should be aborted and the
  // attestation query result should stay the same as before.
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 2);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSentinelFilePresent,
                                       IsSentinelGuardEnabled() ? 1 : 0);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess,
                                       IsSentinelGuardEnabled() ? 0 : 1);

  attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, IsSentinelGuardEnabled()
                                    ? Status::kAttestationsFileCorrupt
                                    : Status::kAllowed);

  // Create a new version valid attestations file which is in a different
  // directory.
  base::ScopedTempDir new_version_dir;
  ASSERT_TRUE(new_version_dir.CreateUniqueTempDir());
  base::FilePath new_version_file_path =
      new_version_dir.GetPath().Append(FILE_PATH_LITERAL("attestations"));
  ASSERT_TRUE(base::WriteFile(new_version_file_path, serialized_proto));

  base::RunLoop parsing_new_version;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_new_version.QuitClosure());

  // Try to load the new version. The new version does not have a sentinel file.
  ASSERT_FALSE(
      base::PathExists(new_version_dir.GetPath().Append(kSentinelFileName)));
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.2"), new_version_file_path);
  parsing_new_version.Run();

  // The new version should be loaded successfully.
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 3);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess,
                                       IsSentinelGuardEnabled() ? 1 : 2);

  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.2"));
  attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAllowed);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PrivacySandboxAttestationsSentinelTest);

}  // namespace privacy_sandbox
