// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/offline_item_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;

namespace offline_pages {

TEST(OfflineItemConversionsTest, OfflinePageItemConversion) {
  std::string name_space = "test_namespace";
  std::string guid = "test_guid";
  ClientId client_id(name_space, guid);
  int64_t offline_id = 5;
  base::FilePath file_path(FILE_PATH_LITERAL("/tmp/example_file_path"));
  int64_t file_size = 200000;
  base::Time creation_time = base::Time::Now();
  base::Time last_access_time = base::Time::Now();
  std::string title = "test title";

  const GURL kTestUrl("http://www.example.com");
  const GURL kTestOriginalUrl("http://www.exampleoriginalurl.com");
  OfflinePageItem offline_page_item(kTestUrl, offline_id, client_id, file_path,
                                    file_size, creation_time);
  offline_page_item.original_url_if_different = kTestOriginalUrl;
  offline_page_item.title = base::UTF8ToUTF16(title);
  offline_page_item.last_access_time = last_access_time;
  offline_page_item.file_missing_time = base::Time::Now();

  OfflineItem offline_item =
      OfflineItemConversions::CreateOfflineItem(offline_page_item);

  EXPECT_EQ(ContentId(kOfflinePageNamespace, guid), offline_item.id);
  EXPECT_EQ(kTestUrl, offline_item.url);
  EXPECT_EQ(kTestOriginalUrl, offline_item.original_url);
  EXPECT_EQ(title, offline_item.title);
  EXPECT_EQ(file_path, offline_item.file_path);
  EXPECT_EQ(creation_time, offline_item.creation_time);
  EXPECT_EQ(creation_time, offline_item.completion_time);
  EXPECT_EQ(last_access_time, offline_item.last_accessed_time);
  EXPECT_EQ(file_size, offline_item.total_size_bytes);
  EXPECT_EQ(file_size, offline_item.received_bytes);
  EXPECT_EQ("multipart/related", offline_item.mime_type);
  EXPECT_EQ(OfflineItemFilter::FILTER_PAGE, offline_item.filter);
  EXPECT_EQ(OfflineItemState::COMPLETE, offline_item.state);
  EXPECT_EQ(100, offline_item.progress.value);
  EXPECT_TRUE(offline_item.progress.max.has_value());
  EXPECT_EQ(100, offline_item.progress.max.value());
  EXPECT_EQ(OfflineItemProgressUnit::PERCENTAGE, offline_item.progress.unit);
  EXPECT_TRUE(offline_item.is_openable);
  EXPECT_TRUE(offline_item.externally_removed);
  EXPECT_EQ(FailState::NO_FAILURE, offline_item.fail_state);
  EXPECT_EQ(PendingState::NOT_PENDING, offline_item.pending_state);
}

TEST(OfflineItemConversionsTest, SavePageRequestConversion) {
  std::string name_space = "test_namespace";
  std::string guid = "test_guid";
  ClientId client_id(name_space, guid);
  int64_t request_id = 5;
  base::Time creation_time = base::Time::Now();

  const GURL kTestUrl("http://www.example.com");
  const GURL kTestOriginalUrl("http://www.exampleoriginalurl.com");
  SavePageRequest save_page_request(request_id, kTestUrl, client_id,
                                    creation_time, false);
  save_page_request.set_original_url(kTestOriginalUrl);
  save_page_request.set_request_state(SavePageRequest::RequestState::OFFLINING);
  save_page_request.set_fail_state(FailState::NETWORK_FAILED);
  save_page_request.set_pending_state(PendingState::PENDING_ANOTHER_DOWNLOAD);

  OfflineItem offline_item =
      OfflineItemConversions::CreateOfflineItem(save_page_request);

  EXPECT_EQ(ContentId(kOfflinePageNamespace, guid), offline_item.id);
  EXPECT_EQ(kTestUrl, offline_item.url);
  EXPECT_EQ(kTestOriginalUrl, offline_item.original_url);
  EXPECT_EQ(kTestUrl.host(), offline_item.title);
  EXPECT_EQ(base::FilePath(), offline_item.file_path);
  EXPECT_EQ(creation_time, offline_item.creation_time);
  EXPECT_EQ(base::Time(), offline_item.completion_time);
  EXPECT_EQ(base::Time(), offline_item.last_accessed_time);
  EXPECT_EQ(-1L, offline_item.total_size_bytes);
  EXPECT_EQ(0, offline_item.received_bytes);
  EXPECT_EQ("multipart/related", offline_item.mime_type);
  EXPECT_EQ(OfflineItemFilter::FILTER_PAGE, offline_item.filter);
  EXPECT_EQ(OfflineItemState::IN_PROGRESS, offline_item.state);
  EXPECT_EQ(0, offline_item.progress.value);
  EXPECT_FALSE(offline_item.progress.max.has_value());
  EXPECT_EQ(OfflineItemProgressUnit::PERCENTAGE, offline_item.progress.unit);
  EXPECT_EQ(FailState::NETWORK_FAILED, offline_item.fail_state);
  EXPECT_EQ(PendingState::NOT_PENDING, offline_item.pending_state);

  // Check that the offline item matches one we create.
  OfflineItem offline_item_p2p =
      OfflineItemConversions::CreateOfflineItem(save_page_request);
  EXPECT_EQ(offline_item, offline_item_p2p);
}

}  // namespace offline_pages
