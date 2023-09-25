// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace privacy_sandbox {

class PrivacySandboxAttestationsTestBase : public testing::Test {
 public:
  PrivacySandboxAttestationsTestBase()
      : scoped_attestations_(PrivacySandboxAttestations::CreateForTesting()) {}

 protected:
  using Status = PrivacySandboxSettingsImpl::Status;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  ScopedPrivacySandboxAttestations scoped_attestations_;
};

TEST_F(PrivacySandboxAttestationsTestBase, AddOverride) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

TEST_F(PrivacySandboxAttestationsTestBase,
       SiteDefaultNotAttestedWithFeatureDefaultEnabled) {
  // Enrollment feature should be enabled by default.
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));
  net::SchemefulSite site(GURL("https://example.com"));

  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileNotYetReady);
}

class PrivacySandboxAttestationsFeatureEnabledTest
    : public PrivacySandboxAttestationsTestBase {
 public:
  PrivacySandboxAttestationsFeatureEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        privacy_sandbox::kEnforcePrivacySandboxAttestations);
  }

  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  void WriteAttestationsFileAndWaitForLoading(base::Version version,
                                              std::string_view content) {
    base::ScopedTempDir component_install_dir;
    CHECK(component_install_dir.CreateUniqueTempDirUnderPath(
        scoped_temp_dir_.GetPath()));
    // Note the actual attestations file name required by the Privacy Sandbox
    // Attestations component is specified by
    // `kPrivacySandboxAttestationsFileName`. Here because this is a unit test
    // so it can be any file name.
    base::FilePath attestations_file_path =
        component_install_dir.GetPath().Append(
            FILE_PATH_LITERAL("attestations"));
    CHECK(base::WriteFile(attestations_file_path, content));

    base::RunLoop run_loop;
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

    PrivacySandboxAttestations::GetInstance()->LoadAttestations(
        version, attestations_file_path);
    run_loop.Run();
  }

  void WriteAttestationsFileAndPauseDuringParsing(base::Version version,
                                                  std::string_view content) {
    base::ScopedTempDir component_install_dir;
    CHECK(component_install_dir.CreateUniqueTempDirUnderPath(
        scoped_temp_dir_.GetPath()));
    // Note the actual attestations file name required by the Privacy Sandbox
    // Attestations component is specified by
    // `kPrivacySandboxAttestationsFileName`. Here because this is a unit test
    // so it can be any file name.
    base::FilePath attestations_file_path =
        component_install_dir.GetPath().Append(
            FILE_PATH_LITERAL("attestations"));
    CHECK(base::WriteFile(attestations_file_path, content));

    base::RunLoop run_loop;
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetLoadAttestationsParsingStartedCallbackForTesting(
            run_loop.QuitClosure());

    PrivacySandboxAttestations::GetInstance()->LoadAttestations(
        version, attestations_file_path);
    run_loop.Run();
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       DefaultDenyIfAttestationsMapNotPresent) {
  net::SchemefulSite site(GURL("https://example.com"));

  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileNotYetReady);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest, AttestedIfOverridden) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_NE(attestation_status, Status::kAllowed);

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       EnrolledWithoutAttestations) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_NE(attestation_status, Status::kAllowed);

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{{site, {}}});

  Status new_attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_NE(new_attestation_status, Status::kAllowed);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest, EnrolledAndAttested) {
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  ASSERT_NE(attestation_status, Status::kAllowed);

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{
          {site, PrivacySandboxAttestationsGatedAPISet{
                     PrivacySandboxAttestationsGatedAPI::kTopics}}});

  Status new_attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(new_attestation_status, Status::kAllowed);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       NonExistentAttestationsFile) {
  base::RunLoop run_loop;
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Call the parsing function with a non-existent file.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath());
  run_loop.Run();

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
}

// The parsing progress may end up being
// `PrivacySandboxAttestations::Progress::kFinished` but there is no in-memory
// attestations map. Verify that the second attempt to parse should not crash.
TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       TryParseNonExistentAttestationsFileTwice) {
  base::RunLoop first_attempt;
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(first_attempt.QuitClosure());

  // Call the parsing function with a non-existent file.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath());
  first_attempt.Run();

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());

  base::RunLoop second_attempt;
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(second_attempt.QuitClosure());
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), base::FilePath());
  second_attempt.Run();

  // The parsing should fail again, without crashes.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       InvalidAttestationsFileIsNotLoaded) {
  // Write an invalid proto file, and try to parse it.
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         "invalid proto");

  // The parsing should fail.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());

  // Attempts to check attestation status should return that the file is
  // corrupt.
  net::SchemefulSite site(GURL("https://example.com"));
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          site, PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileCorrupt);
}

// When parsing fails or crashes, a sentinel file is left in the installation
// direction. This file prevents further parsing attempts.
TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       SentinelPreventsSubsequentParsingAfterCrashOrFailure) {
  base::HistogramTester histogram_tester;
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
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
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
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 0);

  // Attempts to check attestation status should return that the file is
  // corrupt.
  std::string site = "https://example.com";
  Status attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileCorrupt);
  ASSERT_TRUE(
      base::PathExists(install_dir.GetPath().Append(kSentinelFileName)));

  // Failed parsing creates a sentinel file, which prevents subsequent attempts
  // to parse the attestations file. Since it is difficult to check the parsing
  // will not take place with a sentinel file present, we parse again with a
  // valid attestations file. By checking the attestations map is still absent
  // after attempts to load, we can verify that the sentinel file is working as
  // intended.
  // Note: We do not overwrite the previous attestations file because it will
  // make this test flaky on windows bot.
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

  // Override the attestations file with the valid serialized proto.
  ASSERT_TRUE(base::WriteFile(valid_attestations_file_path, serialized_proto));

  // Attempt to load with a valid attestations file.
  base::RunLoop parsing_valid_attestations_with_sentinel;
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_valid_attestations_with_sentinel.QuitClosure());

  // Try to load again, use the valid attestations file instead.
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.1"), valid_attestations_file_path);
  parsing_valid_attestations_with_sentinel.Run();

  // Sentinel file should prevent parsing. The query result should stay the same
  // as before.
  attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAttestationsFileCorrupt);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 0);

  // Create a new version valid attestations file which is in a different
  // directory.
  base::ScopedTempDir new_version_dir;
  ASSERT_TRUE(new_version_dir.CreateUniqueTempDir());
  base::FilePath new_version_file_path =
      new_version_dir.GetPath().Append(FILE_PATH_LITERAL("attestations"));
  ASSERT_TRUE(base::WriteFile(new_version_file_path, serialized_proto));

  base::RunLoop parsing_new_version;
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_new_version.QuitClosure());

  // Try to load the new version. The new version does not have a sentinel file.
  ASSERT_FALSE(
      base::PathExists(new_version_dir.GetPath().Append(kSentinelFileName)));
  PrivacySandboxAttestations::GetInstance()->LoadAttestations(
      base::Version("0.0.2"), new_version_file_path);
  parsing_new_version.Run();

  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 1);

  // The new version should be loaded successfully.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.2"));
  attestation_status =
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics);
  EXPECT_EQ(attestation_status, Status::kAllowed);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest, LoadAttestationsFile) {
  base::HistogramTester histogram_tester;
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAttestationsFileNotYetReady);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 1);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.1"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAllowed);
  // For API not in the attestations list, the result should be
  // `kAttestationFailed`.
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            Status::kAttestationFailed);

  // Add attestation for Protected Audience.
  site_attestation.add_attested_apis(PROTECTED_AUDIENCE);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  // Parse the new version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.2"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 2);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 2);

  // Now the site should be attested for both APIs.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.2"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAllowed);
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
            Status::kAllowed);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       LoadAttestationsFilePauseDuringParsing) {
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAttestationsFileNotYetReady);

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
            Status::kAttestationsDownloadedNotYetLoaded);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       OlderVersionAttestationsFileIsNotLoaded) {
  base::HistogramTester histogram_tester;
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAttestationsFileNotYetReady);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  WriteAttestationsFileAndWaitForLoading(base::Version("1.2.3"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 1);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("1.2.3"));
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAllowed);

  // Clear the proto attestations.
  proto.clear_site_attestations();

  // Load the attestations file, which has an older version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 1);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The attestations map should still be the old one.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("1.2.3"));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAllowed);
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       NewerVersionAttestationsFileIsLoaded) {
  base::HistogramTester histogram_tester;
  PrivacySandboxAttestationsProto proto;
  ASSERT_TRUE(proto.site_attestations_size() == 0);

  std::string site = "https://example.com";
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAttestationsFileNotYetReady);

  // Add attestation for the site.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.1"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 1);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 1);

  // The site should be attested for the API.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.1"));
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics),
            Status::kAllowed);

  // Clear the attestations.
  proto.clear_site_attestations();

  // Load the attestations file, which has a newer version.
  proto.SerializeToString(&serialized_proto);
  WriteAttestationsFileAndWaitForLoading(base::Version("0.0.2"),
                                         serialized_proto);
  histogram_tester.ExpectTotalCount(kAttestationsFileParsingUMA, 2);
  histogram_tester.ExpectTotalCount(kAttestationsMapMemoryUsageUMA, 2);

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
            Status::kAttestationFailed);
}

}  // namespace privacy_sandbox
