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

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListIsIgnoredIfNotConfigured) {
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "exe", GURL{"http://www.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListIsIgnoredIfNoValuesSet) {
  base::Value::List list;
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "exe", GURL{"http://www.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest,
       OverrideListIsIgnoredIfNoDomainsSetForExtension) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {/* empty vector */}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com"}, &pref_service_));
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
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "exe", GURL{"http://www.example.com"}, &pref_service_));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "jpg", GURL{"http://www.example.com"}, &pref_service_));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "exe", GURL{"http://www.example1.com"}, &pref_service_));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://foo.example.com"}, &pref_service_));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example1.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanMatchExactly) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"http://www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com"}, &pref_service_));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://foo.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanMatchSubPaths) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"http://www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com/some/path/file.html"},
      &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, CanLimitToHTTPS) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"https://example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"https://www.example.com"}, &pref_service_));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"https://foo.example.com"}, &pref_service_));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest,
       OverrideListOnlyWorksForListedExtension) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com"}, &pref_service_));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "exe", GURL{"http://www.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, ValuesAreNotCaseSensitive) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "TxT", {"www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "tXt", GURL{"hTTp://wWw.example.cOM"}, &pref_service_));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "exe", GURL{"hTTp://wWw.example.cOM"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, NormalizesBlobURLs) {
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "txt", {"https://example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  ASSERT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"https://www.example.com"}, &pref_service_));
  // The blob: version of this URL should also be allowed.
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"blob:https://www.example.com"}, &pref_service_));
}

}  // namespace safe_browsing::file_type
