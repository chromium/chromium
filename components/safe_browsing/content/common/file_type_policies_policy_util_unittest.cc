// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_policy_util.h"

#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

base::Value::List CreateStringListValueForTest(
    const std::vector<std::string>& items) {
  base::Value::List list;
  for (const auto& item : items) {
    list.Append(item);
  }
  return list;
}

base::Value::Dict CreatePolicyEntry(const std::string& extension,
                                    const std::vector<std::string>& domains) {
  base::Value::Dict out;
  out.Set("file_extension", base::Value{extension});
  out.Set("domains", CreateStringListValueForTest(domains));
  return out;
}

}  // namespace

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
  list.Append(CreatePolicyEntry("txt", {/* empty vector */}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_FALSE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com"}, &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, OverrideListCanUseWildcards) {
  base::Value::List list;
  list.Append(CreatePolicyEntry("txt", {"example.com"}));
  list.Append(CreatePolicyEntry("exe", {"http://*"}));
  list.Append(CreatePolicyEntry("jpg", {"*"}));
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
  list.Append(CreatePolicyEntry("txt", {"http://www.example.com"}));
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
  list.Append(CreatePolicyEntry("txt", {"http://www.example.com"}));
  pref_service_.SetList(
      prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));
  EXPECT_TRUE(IsInNotDangerousOverrideList(
      "txt", GURL{"http://www.example.com/some/path/file.html"},
      &pref_service_));
}

TEST_F(FileTypePoliciesPolicyUtilTest, CanLimitToHTTPS) {
  base::Value::List list;
  list.Append(CreatePolicyEntry("txt", {"https://example.com"}));
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
  list.Append(CreatePolicyEntry("txt", {"www.example.com"}));
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
  list.Append(CreatePolicyEntry("TxT", {"www.example.com"}));
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
  list.Append(CreatePolicyEntry("txt", {"https://example.com"}));
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