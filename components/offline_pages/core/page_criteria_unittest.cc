// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/page_criteria.h"

#include "base/bind.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

GURL TestURL() {
  return GURL("http://someurl.com");
}
GURL OtherURL() {
  return GURL("http://other.com");
}
GURL TestURLWithFragment() {
  return GURL("http://someurl.com#fragment");
}

class PageCriteriaTest : public testing::Test {
};

TEST_F(PageCriteriaTest, MeetsCriteria_Url) {
  PageCriteria criteria;
  criteria.url = TestURL();

  OfflinePageItem item;

  item.url = TestURLWithFragment();
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.url = TestURL();
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.url = OtherURL();
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_UrlWithFragment) {
  PageCriteria criteria;
  criteria.url = TestURLWithFragment();

  OfflinePageItem item;

  item.url = TestURLWithFragment();
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.url = TestURL();
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.url = OtherURL();
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_ExcludeTabBoundPages) {
  PageCriteria criteria;
  criteria.exclude_tab_bound_pages = true;

  OfflinePageItem item;
  item.client_id.name_space = kLastNNamespace;
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id.name_space = "";
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id.name_space = kDownloadNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_PagesForTabId) {
  PageCriteria criteria;
  criteria.pages_for_tab_id = 0;

  OfflinePageItem item;
  item.client_id.id = "0";
  item.client_id.name_space = kLastNNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  // Namespace not restricted to tab.
  item.client_id.id = "1";
  item.client_id.name_space = kDownloadNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  // Different tab id.
  item.client_id.id = "1";
  item.client_id.name_space = kLastNNamespace;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_SupportedByDownloads) {
  PageCriteria criteria;
  criteria.supported_by_downloads = true;

  OfflinePageItem item;
  item.client_id.name_space = kDownloadNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));
  EXPECT_TRUE(MeetsCriteria(criteria, item.client_id));

  item.client_id.name_space = kLastNNamespace;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
  EXPECT_FALSE(MeetsCriteria(criteria, item.client_id));
}

TEST_F(PageCriteriaTest, MeetsCriteria_PersistentLifetime) {
  PageCriteria criteria;
  criteria.lifetime_type = LifetimeType::PERSISTENT;

  OfflinePageItem item;
  item.client_id.name_space = kDownloadNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));
  EXPECT_TRUE(MeetsCriteria(criteria, item.client_id));

  item.client_id.name_space = kLastNNamespace;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
  EXPECT_FALSE(MeetsCriteria(criteria, item.client_id));
}

TEST_F(PageCriteriaTest, MeetsCriteria_TemporaryLifetime) {
  PageCriteria criteria;
  criteria.lifetime_type = LifetimeType::TEMPORARY;

  OfflinePageItem item;
  item.client_id.name_space = kLastNNamespace;
  EXPECT_TRUE(MeetsCriteria(criteria, item));
  EXPECT_TRUE(MeetsCriteria(criteria, item.client_id));

  item.client_id.name_space = kDownloadNamespace;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
  EXPECT_FALSE(MeetsCriteria(criteria, item.client_id));
}

TEST_F(PageCriteriaTest, MeetsCriteria_FileSize) {
  PageCriteria criteria;
  criteria.file_size = 123;

  OfflinePageItem item;
  item.file_size = 123;
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.file_size = 124;
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.file_size = 0;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_Digest) {
  PageCriteria criteria;
  criteria.digest = "abc";

  OfflinePageItem item;
  item.digest = "abc";
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.digest = "";
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.digest = "def";
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_Namespaces) {
  PageCriteria criteria;
  criteria.client_namespaces = std::vector<std::string>{"namespace1"};

  OfflinePageItem item;
  item.client_id.name_space = "namespace1";
  EXPECT_TRUE(MeetsCriteria(criteria, item));
  EXPECT_TRUE(MeetsCriteria(criteria, item.client_id));

  item.client_id.name_space = "namespace2";
  EXPECT_FALSE(MeetsCriteria(criteria, item));
  EXPECT_FALSE(MeetsCriteria(criteria, item.client_id));

  item.client_id.name_space = "";
  EXPECT_FALSE(MeetsCriteria(criteria, item));
  EXPECT_FALSE(MeetsCriteria(criteria, item.client_id));
}

TEST_F(PageCriteriaTest, MeetsCriteria_MultipleNamespaces) {
  PageCriteria criteria;
  criteria.client_namespaces =
      std::vector<std::string>{"namespace1", "foobar1"};

  OfflinePageItem item;
  item.client_id.name_space = "namespace1";
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id.name_space = "foobar1";
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id.name_space = "namespace";
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id.name_space = "foobar";
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id.name_space = "";
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_ClientId) {
  PageCriteria criteria;
  criteria.client_ids = std::vector<ClientId>{ClientId("namespace1", "id")};

  OfflinePageItem item;
  item.client_id = ClientId("namespace1", "id");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace2", "id");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace1", "id2");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId();
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_MultipleClientId) {
  PageCriteria criteria;
  criteria.client_ids = std::vector<ClientId>{ClientId("namespace1", "id"),
                                              ClientId("namespace2", "id"),
                                              ClientId("namespace3", "id3")};

  OfflinePageItem item;
  item.client_id = ClientId("namespace1", "id");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace2", "id");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace3", "id3");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace", "i");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace", "");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("name", "id");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace", "foo");
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_Guid) {
  PageCriteria criteria;
  criteria.guid = "abc";

  OfflinePageItem item;
  item.client_id = ClientId("namespace", "abc");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace2", "abc");
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.client_id = ClientId("namespace", "abcd");
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.client_id = ClientId();
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_RequestOrigin) {
  PageCriteria criteria;
  criteria.request_origin = "abc";

  OfflinePageItem item;
  item.request_origin = "abc";
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.request_origin = "abcd";
  EXPECT_FALSE(MeetsCriteria(criteria, item));

  item.request_origin = "";
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_OfflineId) {
  PageCriteria criteria;
  criteria.offline_ids = std::vector<int64_t>{1, 5};

  OfflinePageItem item;
  item.offline_id = 5;
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.offline_id = 4;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

TEST_F(PageCriteriaTest, MeetsCriteria_AdditionalCriteria) {
  PageCriteria criteria;
  criteria.additional_criteria = base::BindRepeating(
      [](const OfflinePageItem& item) { return item.offline_id == 5; });

  OfflinePageItem item;
  item.offline_id = 5;
  EXPECT_TRUE(MeetsCriteria(criteria, item));

  item.offline_id = 4;
  EXPECT_FALSE(MeetsCriteria(criteria, item));
}

}  // namespace
}  // namespace offline_pages
