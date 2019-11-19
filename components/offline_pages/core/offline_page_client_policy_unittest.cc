// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_client_policy.h"

#include <algorithm>
#include <memory>
#include <set>

#include "base/stl_util.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kUndefinedNamespace[] = "undefined";

bool isTemporary(const OfflinePageClientPolicy& policy) {
  return policy.lifetime_type == LifetimeType::TEMPORARY;
}
}  // namespace

class ClientPolicyTest : public testing::Test {
 public:
  // testing::Test
  void SetUp() override {}
  void TearDown() override {}

 protected:
  void ExpectTemporary(std::string name_space);
  void ExpectDownloadSupport(std::string name_space, bool expectation);
  void ExpectPersistent(std::string name_space);
  void ExpectRestrictedToTabFromClientId(std::string name_space,
                                         bool expectation);
  void ExpectRequiresSpecificUserSettings(std::string name_space,
                                          bool expectation);
};

void ClientPolicyTest::ExpectTemporary(std::string name_space) {
  EXPECT_TRUE(base::Contains(GetTemporaryPolicyNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting temporary namespaces.";
  EXPECT_EQ(GetPolicy(name_space).lifetime_type, LifetimeType::TEMPORARY)
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is temporary.";
  EXPECT_FALSE(base::Contains(GetPersistentPolicyNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting persistent namespaces.";
}

void ClientPolicyTest::ExpectDownloadSupport(std::string name_space,
                                             bool expectation) {
  EXPECT_EQ(expectation, GetPolicy(name_space).is_supported_by_download)
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if supported"
         " by download.";
}

void ClientPolicyTest::ExpectPersistent(std::string name_space) {
  EXPECT_FALSE(base::Contains(GetTemporaryPolicyNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting temporary namespaces.";
  EXPECT_EQ(GetPolicy(name_space).lifetime_type, LifetimeType::PERSISTENT)
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is temporary.";
  EXPECT_TRUE(base::Contains(GetPersistentPolicyNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting persistent namespaces.";
}

void ClientPolicyTest::ExpectRestrictedToTabFromClientId(std::string name_space,
                                                         bool expectation) {
  EXPECT_EQ(expectation,
            GetPolicy(name_space).is_restricted_to_tab_from_client_id)
      << "Namespace " << name_space
      << " had incorrect restriction when directly checking if the namespace"
         " is restricted to the tab from the client id field";
}

void ClientPolicyTest::ExpectRequiresSpecificUserSettings(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(expectation, GetPolicy(name_space).requires_specific_user_settings)
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if disabled"
         " when prefetch settings are disabled.";
}

TEST_F(ClientPolicyTest, FallbackTest) {
  const OfflinePageClientPolicy& policy = GetPolicy(kUndefinedNamespace);
  EXPECT_EQ(policy.name_space, kDefaultNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kDefaultNamespace);
  EXPECT_FALSE(
      base::Contains(GetTemporaryPolicyNamespaces(), kUndefinedNamespace));
  EXPECT_EQ(GetPolicy(kUndefinedNamespace).lifetime_type,
            LifetimeType::TEMPORARY);
  ExpectDownloadSupport(kUndefinedNamespace, false);
  ExpectDownloadSupport(kDefaultNamespace, false);
  ExpectRestrictedToTabFromClientId(kUndefinedNamespace, false);
  ExpectRestrictedToTabFromClientId(kDefaultNamespace, false);
  ExpectRequiresSpecificUserSettings(kUndefinedNamespace, false);
  ExpectRequiresSpecificUserSettings(kDefaultNamespace, false);
}

TEST_F(ClientPolicyTest, CheckBookmarkDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kBookmarkNamespace);
  EXPECT_EQ(policy.name_space, kBookmarkNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kBookmarkNamespace);
  ExpectDownloadSupport(kBookmarkNamespace, false);
  ExpectRestrictedToTabFromClientId(kBookmarkNamespace, false);
  ExpectRequiresSpecificUserSettings(kBookmarkNamespace, false);
}

TEST_F(ClientPolicyTest, CheckLastNDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kLastNNamespace);
  EXPECT_EQ(policy.name_space, kLastNNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kLastNNamespace);
  ExpectDownloadSupport(kLastNNamespace, false);
  ExpectRestrictedToTabFromClientId(kLastNNamespace, true);
  ExpectRequiresSpecificUserSettings(kLastNNamespace, false);
}

TEST_F(ClientPolicyTest, CheckAsyncDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kAsyncNamespace);
  EXPECT_EQ(policy.name_space, kAsyncNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kAsyncNamespace, true);
  ExpectPersistent(kAsyncNamespace);
  ExpectRestrictedToTabFromClientId(kAsyncNamespace, false);
  ExpectRequiresSpecificUserSettings(kAsyncNamespace, false);
}

TEST_F(ClientPolicyTest, CheckCCTDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kCCTNamespace);
  EXPECT_EQ(policy.name_space, kCCTNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kCCTNamespace);
  ExpectDownloadSupport(kCCTNamespace, false);
  ExpectRestrictedToTabFromClientId(kCCTNamespace, false);
  ExpectRequiresSpecificUserSettings(kCCTNamespace, true);
}

TEST_F(ClientPolicyTest, CheckDownloadDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kDownloadNamespace);
  EXPECT_EQ(policy.name_space, kDownloadNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kDownloadNamespace, true);
  ExpectPersistent(kDownloadNamespace);
  ExpectRestrictedToTabFromClientId(kDownloadNamespace, false);
  ExpectRequiresSpecificUserSettings(kDownloadNamespace, false);
}

TEST_F(ClientPolicyTest, CheckNTPSuggestionsDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kNTPSuggestionsNamespace);
  EXPECT_EQ(policy.name_space, kNTPSuggestionsNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kNTPSuggestionsNamespace, true);
  ExpectPersistent(kNTPSuggestionsNamespace);
  ExpectRestrictedToTabFromClientId(kNTPSuggestionsNamespace, false);
  ExpectRequiresSpecificUserSettings(kNTPSuggestionsNamespace, false);
}

TEST_F(ClientPolicyTest, CheckSuggestedArticlesDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kSuggestedArticlesNamespace);
  EXPECT_EQ(policy.name_space, kSuggestedArticlesNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kSuggestedArticlesNamespace);
  ExpectDownloadSupport(kSuggestedArticlesNamespace, true);
  ExpectRestrictedToTabFromClientId(kSuggestedArticlesNamespace, false);
  ExpectRequiresSpecificUserSettings(kSuggestedArticlesNamespace, false);
}

TEST_F(ClientPolicyTest, CheckLivePageSharingDefined) {
  OfflinePageClientPolicy policy = GetPolicy(kLivePageSharingNamespace);
  EXPECT_EQ(policy.name_space, kLivePageSharingNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kLivePageSharingNamespace);
  ExpectDownloadSupport(kLivePageSharingNamespace, false);
  ExpectRestrictedToTabFromClientId(kLivePageSharingNamespace, true);
  ExpectRequiresSpecificUserSettings(kLivePageSharingNamespace, false);
}

TEST_F(ClientPolicyTest, AllTemporaryNamespaces) {
  std::vector<std::string> all_namespaces = GetAllPolicyNamespaces();
  const std::vector<std::string>& cache_reset_namespaces_list =
      GetTemporaryPolicyNamespaces();
  std::set<std::string> cache_reset_namespaces(
      cache_reset_namespaces_list.begin(), cache_reset_namespaces_list.end());
  for (auto name_space : cache_reset_namespaces) {
    if (cache_reset_namespaces.count(name_space) > 0)
      ExpectTemporary(name_space);
    else
      ExpectPersistent(name_space);
  }
}

}  // namespace offline_pages
