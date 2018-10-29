// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_policy_controller.h"

#include <algorithm>

#include "base/stl_util.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

namespace {
const char kUndefinedNamespace[] = "undefined";

bool isTemporary(const OfflinePageClientPolicy& policy) {
  return policy.lifetime_policy.lifetime_type == LifetimeType::TEMPORARY;
}
}  // namespace

class ClientPolicyControllerTest : public testing::Test {
 public:
  ClientPolicyController* controller() { return controller_.get(); }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  void ExpectRemovedOnCacheReset(std::string name_space, bool expectation);
  void ExpectDownloadSupport(std::string name_space, bool expectation);
  void ExpectUserRequestedDownloadSupport(std::string name_space,
                                          bool expectation);
  void ExpectRecentTab(std::string name_space, bool expectation);
  void ExpectRestrictedToTabFromClientId(std::string name_space,
                                         bool expectation);
  void ExpectDisabledWhenPrefetchDisabled(std::string name_space,
                                          bool expectation);

 private:
  std::unique_ptr<ClientPolicyController> controller_;
};

void ClientPolicyControllerTest::SetUp() {
  controller_.reset(new ClientPolicyController());
}

void ClientPolicyControllerTest::TearDown() {
  controller_.reset();
}

void ClientPolicyControllerTest::ExpectRemovedOnCacheReset(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(expectation, controller()->IsRemovedOnCacheReset(name_space))
      << "Namespace " << name_space
      << " had incorrect removed_on_cache_reset setting when directly checking"
         " is removed-on-cache-reset.";
}

void ClientPolicyControllerTest::ExpectDownloadSupport(std::string name_space,
                                                       bool expectation) {
  EXPECT_EQ(expectation,
            base::ContainsValue(
                controller()->GetNamespacesSupportedByDownload(), name_space))
      << "Namespace " << name_space
      << " had incorrect download support when getting namespaces supported by"
         " download.";
  EXPECT_EQ(expectation, controller()->IsSupportedByDownload(name_space))
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if supported"
         " by download.";
}

void ClientPolicyControllerTest::ExpectUserRequestedDownloadSupport(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(
      expectation,
      base::ContainsValue(controller()->GetNamespacesForUserRequestedDownload(),
                          name_space))
      << "Namespace " << name_space
      << " had incorrect user generated download support when getting"
         " namespaces supported by user generaged download.";
  EXPECT_EQ(expectation, controller()->IsUserRequestedDownload(name_space))
      << "Namespace " << name_space
      << " had incorrect user generated download support when directly checking"
         " if supported by user generated download.";
}

void ClientPolicyControllerTest::ExpectRecentTab(std::string name_space,
                                                 bool expectation) {
  EXPECT_EQ(
      expectation,
      base::ContainsValue(
          controller()->GetNamespacesShownAsRecentlyVisitedSite(), name_space))
      << "Namespace " << name_space
      << " had incorrect recent tab support when getting namespaces shown as a"
         " recently visited site.";
  EXPECT_EQ(expectation, controller()->IsShownAsRecentlyVisitedSite(name_space))
      << "Namespace " << name_space
      << " had incorrect recent tab support when directly checking if shown as"
         " a recently visited site.";
}

void ClientPolicyControllerTest::ExpectRestrictedToTabFromClientId(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(
      expectation,
      base::ContainsValue(
          controller()->GetNamespacesRestrictedToTabFromClientId(), name_space))
      << "Namespace " << name_space
      << " had incorrect restriction when getting namespaces restricted to"
         " the tab from the client id field";
  EXPECT_EQ(expectation,
            controller()->IsRestrictedToTabFromClientId(name_space))
      << "Namespace " << name_space
      << " had incorrect restriction when directly checking if the namespace"
         " is restricted to the tab from the client id field";
}

void ClientPolicyControllerTest::ExpectDisabledWhenPrefetchDisabled(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(expectation,
            base::ContainsValue(
                controller()->GetNamespacesDisabledWhenPrefetchDisabled(),
                name_space))
      << "Namespace " << name_space
      << " had incorrect prefetch pref support when getting namespaces"
         " disabled when prefetch settings are disabled.";
  EXPECT_EQ(expectation,
            controller()->IsDisabledWhenPrefetchDisabled(name_space))
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if disabled"
         " when prefetch settings are disabled.";
}

TEST_F(ClientPolicyControllerTest, FallbackTest) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kUndefinedNamespace);
  EXPECT_EQ(policy.name_space, kDefaultNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kUndefinedNamespace));
  ExpectRemovedOnCacheReset(kUndefinedNamespace, true);
  ExpectDownloadSupport(kUndefinedNamespace, false);
  ExpectUserRequestedDownloadSupport(kUndefinedNamespace, false);
  ExpectRecentTab(kUndefinedNamespace, false);
  ExpectRestrictedToTabFromClientId(kUndefinedNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kUndefinedNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckBookmarkDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kBookmarkNamespace);
  EXPECT_EQ(policy.name_space, kBookmarkNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kBookmarkNamespace));
  ExpectRemovedOnCacheReset(kBookmarkNamespace, true);
  ExpectDownloadSupport(kBookmarkNamespace, false);
  ExpectUserRequestedDownloadSupport(kBookmarkNamespace, false);
  ExpectRecentTab(kBookmarkNamespace, false);
  ExpectRestrictedToTabFromClientId(kBookmarkNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kBookmarkNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckLastNDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kLastNNamespace);
  EXPECT_EQ(policy.name_space, kLastNNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kLastNNamespace));
  ExpectRemovedOnCacheReset(kLastNNamespace, true);
  ExpectDownloadSupport(kLastNNamespace, false);
  ExpectUserRequestedDownloadSupport(kLastNNamespace, false);
  ExpectRecentTab(kLastNNamespace, true);
  ExpectRestrictedToTabFromClientId(kLastNNamespace, true);
  ExpectDisabledWhenPrefetchDisabled(kLastNNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckAsyncDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kAsyncNamespace);
  EXPECT_EQ(policy.name_space, kAsyncNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_FALSE(controller()->IsRemovedOnCacheReset(kAsyncNamespace));
  ExpectRemovedOnCacheReset(kAsyncNamespace, false);
  ExpectDownloadSupport(kAsyncNamespace, true);
  ExpectUserRequestedDownloadSupport(kAsyncNamespace, true);
  ExpectRecentTab(kAsyncNamespace, false);
  ExpectRestrictedToTabFromClientId(kAsyncNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kAsyncNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckCCTDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kCCTNamespace);
  EXPECT_EQ(policy.name_space, kCCTNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kCCTNamespace));
  ExpectRemovedOnCacheReset(kCCTNamespace, true);
  ExpectDownloadSupport(kCCTNamespace, false);
  ExpectUserRequestedDownloadSupport(kCCTNamespace, false);
  ExpectRecentTab(kCCTNamespace, false);
  ExpectRestrictedToTabFromClientId(kCCTNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kCCTNamespace, true);
}

TEST_F(ClientPolicyControllerTest, CheckDownloadDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kDownloadNamespace);
  EXPECT_EQ(policy.name_space, kDownloadNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_FALSE(controller()->IsRemovedOnCacheReset(kDownloadNamespace));
  ExpectRemovedOnCacheReset(kDownloadNamespace, false);
  ExpectDownloadSupport(kDownloadNamespace, true);
  ExpectUserRequestedDownloadSupport(kDownloadNamespace, true);
  ExpectRecentTab(kDownloadNamespace, false);
  ExpectRestrictedToTabFromClientId(kDownloadNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kDownloadNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckNTPSuggestionsDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kNTPSuggestionsNamespace);
  EXPECT_EQ(policy.name_space, kNTPSuggestionsNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_FALSE(controller()->IsRemovedOnCacheReset(kNTPSuggestionsNamespace));
  ExpectRemovedOnCacheReset(kNTPSuggestionsNamespace, false);
  ExpectDownloadSupport(kNTPSuggestionsNamespace, true);
  ExpectUserRequestedDownloadSupport(kNTPSuggestionsNamespace, true);
  ExpectRecentTab(kNTPSuggestionsNamespace, false);
  ExpectRestrictedToTabFromClientId(kNTPSuggestionsNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kNTPSuggestionsNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckSuggestedArticlesDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kSuggestedArticlesNamespace);
  EXPECT_EQ(policy.name_space, kSuggestedArticlesNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kSuggestedArticlesNamespace));
  ExpectRemovedOnCacheReset(kSuggestedArticlesNamespace, true);
  ExpectDownloadSupport(kSuggestedArticlesNamespace, false);
  ExpectUserRequestedDownloadSupport(kSuggestedArticlesNamespace, false);
  ExpectRecentTab(kSuggestedArticlesNamespace, false);
  ExpectRestrictedToTabFromClientId(kSuggestedArticlesNamespace, false);
  ExpectDisabledWhenPrefetchDisabled(kSuggestedArticlesNamespace, true);
}

TEST_F(ClientPolicyControllerTest, CheckLivePageSharingDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kLivePageSharingNamespace);
  EXPECT_EQ(policy.name_space, kLivePageSharingNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kLivePageSharingNamespace));
  ExpectRemovedOnCacheReset(kLivePageSharingNamespace, true);
  ExpectDownloadSupport(kLivePageSharingNamespace, false);
  ExpectUserRequestedDownloadSupport(kLivePageSharingNamespace, false);
  ExpectRecentTab(kLivePageSharingNamespace, false);
  ExpectRestrictedToTabFromClientId(kLivePageSharingNamespace, true);
  ExpectDisabledWhenPrefetchDisabled(kLivePageSharingNamespace, false);
}

TEST_F(ClientPolicyControllerTest, GetNamespacesRemovedOnCacheReset) {
  std::vector<std::string> all_namespaces = controller()->GetAllNamespaces();
  const std::vector<std::string>& cache_reset_namespaces_list =
      controller()->GetNamespacesRemovedOnCacheReset();
  std::set<std::string> cache_reset_namespaces(
      cache_reset_namespaces_list.begin(), cache_reset_namespaces_list.end());
  for (auto name_space : cache_reset_namespaces) {
    if (cache_reset_namespaces.count(name_space) > 0)
      EXPECT_TRUE(controller()->IsRemovedOnCacheReset(name_space));
    else
      EXPECT_FALSE(controller()->IsRemovedOnCacheReset(name_space));
  }
}

}  // namespace offline_pages
