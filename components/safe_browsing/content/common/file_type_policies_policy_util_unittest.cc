// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_policy_util.h"

#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing::file_type {
class FileTypePoliciesPolicyUtilTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  FileTypePoliciesPolicyUtilTest() = default;
  FileTypePoliciesPolicyUtilTest(FileTypePoliciesPolicyUtilTest& other) =
      delete;
  FileTypePoliciesPolicyUtilTest& operator=(
      const FileTypePoliciesPolicyUtilTest&) = delete;
  void SetUp() override { RegisterProfilePrefs(pref_service_.registry()); }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(FileTypePoliciesPolicyUtilTest, InvalidUrlNoOverride) {
  EXPECT_EQ(ShouldOverrideFileTypePolicies("exe", GURL{}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
  EXPECT_EQ(
      ShouldOverrideFileTypePolicies("exe", GURL{"garbage"}, &pref_service_),
      FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, LocalFileUrlOverride) {
  EXPECT_EQ(ShouldOverrideFileTypePolicies("exe", GURL{"file:///foo.exe"},
                                           &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies("txt", GURL{"file:///foo.txt"},
                                           &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
}

TEST_F(FileTypePoliciesPolicyUtilTest, RemoteFileUrlNoOverride) {
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"file://drive.example/foo.exe"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"file://drive.example/foo.txt"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListIsIgnoredIfNotConfigured) {
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListIsIgnoredIfNoValuesSet) {
  base::Value::List list;
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest,
       OverrideListIsIgnoredIfNoDomainsSetForExtension) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {/* empty vector */}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanUseWildcards) {
  base::Value::List list;
  list.Append(
      CreateNotDangerousOverridePolicyEntryForTesting("txt", {"example.com"}));
  list.Append(
      CreateNotDangerousOverridePolicyEntryForTesting("exe", {"http://*"}));
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting("jpg", {"*"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "jpg", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"http://www.example1.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://foo.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example1.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanMatchExactly) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"http://www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://foo.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanMatchSubPaths) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"http://www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example.com/some/path/file.html"},
                &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
}

TEST_F(FileTypePoliciesPolicyUtilTest, CanLimitToHTTPS) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"https://example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"https://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"https://foo.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest,
       OverrideListOnlyWorksForListedExtension) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"http://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, ValuesAreNotCaseSensitive) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "TxT", {"www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "tXt", GURL{"hTTp://wWw.example.cOM"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "exe", GURL{"hTTp://wWw.example.cOM"}, &pref_service_),
            FileTypePoliciesOverrideResult::kDoNotOverride);
}

TEST_F(FileTypePoliciesPolicyUtilTest, NormalizesBlobURLs) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"https://example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  ASSERT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"https://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
  // The blob: version of this URL should also be allowed.
  EXPECT_EQ(ShouldOverrideFileTypePolicies(
                "txt", GURL{"blob:https://www.example.com"}, &pref_service_),
            FileTypePoliciesOverrideResult::kOverrideAsNotDangerous);
}

}  // namespace safe_browsing::file_type
