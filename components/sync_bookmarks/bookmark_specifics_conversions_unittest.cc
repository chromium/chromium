// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Eq;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

class TestBookmarkClientWithFaviconLoad : public bookmarks::TestBookmarkClient {
 public:
  TestBookmarkClientWithFaviconLoad() = default;
  ~TestBookmarkClientWithFaviconLoad() override = default;

  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::IconType type,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    ++load_favicon_requests;
    return TestBookmarkClient::GetFaviconImageForPageURL(
        page_url, type, std::move(callback), tracker);
  }

  int GetLoadFaviconRequestsForTest() { return load_favicon_requests; }

 private:
  int load_favicon_requests = 0;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClientWithFaviconLoad);
};

TEST(BookmarkSpecificsConversionsTest, ShouldCreateSpecificsFromBookmarkNode) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      kUrl);
  ASSERT_THAT(node, NotNull());
  model->SetDateAdded(node, kTime);
  model->SetNodeMetaInfo(node, kKey1, kValue1);
  model->SetNodeMetaInfo(node, kKey2, kValue2);

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model.get(), /*force_favicon_load=*/false);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_THAT(bm_specifics.guid(), Eq(node->guid()));
  EXPECT_THAT(bm_specifics.title(), Eq(kTitle));
  EXPECT_THAT(GURL(bm_specifics.url()), Eq(kUrl));
  EXPECT_THAT(
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(bm_specifics.creation_time_us())),
      Eq(kTime));
  for (const sync_pb::MetaInfo& meta_info : bm_specifics.meta_info()) {
    std::string value;
    node->GetMetaInfo(meta_info.key(), &value);
    EXPECT_THAT(meta_info.value(), Eq(value));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateSpecificsFromBookmarkNodeWithIllegalTitle) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const std::vector<std::string> illegal_titles = {"", ".", ".."};
  size_t index = 0;
  for (const std::string& illegal_title : illegal_titles) {
    const bookmarks::BookmarkNode* node = model->AddURL(
        /*parent=*/bookmark_bar_node, index++, base::UTF8ToUTF16(illegal_title),
        GURL("http://www.url.com"));
    ASSERT_THAT(node, NotNull());
    sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
        node, model.get(), /*force_favicon_load=*/false);
    // Legacy clients append a space to illegal titles.
    EXPECT_THAT(specifics.bookmark().title(), Eq(illegal_title + " "));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateSpecificsWithoutUrlFromFolderNode) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("Title"));
  ASSERT_THAT(node, NotNull());

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model.get(), /*force_favicon_load=*/false);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_FALSE(bm_specifics.has_url());
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldLoadFaviconWhenCreatingSpecificsFromBookmarkNode) {
  auto client = std::make_unique<TestBookmarkClientWithFaviconLoad>();
  TestBookmarkClientWithFaviconLoad* client_ptr = client.get();

  std::unique_ptr<bookmarks::BookmarkModel> model =
      TestBookmarkClientWithFaviconLoad::CreateModelWithClient(
          std::move(client));

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("Title"),
      GURL("http://www.url.com"));
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_THAT(client_ptr->GetLoadFaviconRequestsForTest(), Eq(0));
  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model.get(), /*force_favicon_load=*/true);
  EXPECT_THAT(client_ptr->GetLoadFaviconRequestsForTest(), Eq(1));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldNotLoadFaviconWhenCreatingSpecificsFromBookmarkNode) {
  auto client = std::make_unique<TestBookmarkClientWithFaviconLoad>();
  TestBookmarkClientWithFaviconLoad* client_ptr = client.get();

  std::unique_ptr<bookmarks::BookmarkModel> model =
      TestBookmarkClientWithFaviconLoad::CreateModelWithClient(
          std::move(client));

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("Title"),
      GURL("http://www.url.com"));
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_THAT(client_ptr->GetLoadFaviconRequestsForTest(), Eq(0));
  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model.get(), /*force_favicon_load=*/false);
  EXPECT_THAT(client_ptr->GetLoadFaviconRequestsForTest(), Eq(0));
}

TEST(BookmarkSpecificsConversionsTest, ShouldCreateBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kUrl.spec());
  bm_specifics->set_guid(kGuid);
  bm_specifics->set_icon_url(kIconUrl.spec());
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_title(kTitle);
  bm_specifics->set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kIconUrl, _, _, _));
  const bookmarks::BookmarkNode* node = CreateBookmarkNodeFromSpecifics(
      *bm_specifics,
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      /*is_folder=*/false, model.get(), &favicon_service);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->guid(), Eq(kGuid));
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kTitle)));
  EXPECT_THAT(node->url(), Eq(kUrl));
  EXPECT_THAT(node->date_added(), Eq(kTime));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kValue2));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateBookmarkNodeFromSpecificsWithIllegalTitle) {
  const std::string kGuid = base::GenerateGUID();
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::NiceMock<favicon::MockFaviconService> favicon_service;

  const std::vector<std::string> illegal_titles = {"", ".", ".."};

  size_t index = 0;
  for (const std::string& illegal_title : illegal_titles) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
    bm_specifics->set_url("http://www.url.com");
    bm_specifics->set_guid(kGuid);
    // Legacy clients append an extra space to illegal clients.
    bm_specifics->set_title(illegal_title + " ");
    const bookmarks::BookmarkNode* node = CreateBookmarkNodeFromSpecifics(
        *bm_specifics,
        /*parent=*/model->bookmark_bar_node(), index++,
        /*is_folder=*/false, model.get(), &favicon_service);
    ASSERT_THAT(node, NotNull());
    // The node should be created without the extra space.
    EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(illegal_title)));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateBookmarkNodeFromSpecificsWithFaviconAndWithoutIconUrl) {
  const GURL kUrl("http://www.url.com");
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "Title";
  const GURL kIconUrl("http://www.icon-url.com");

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kUrl.spec());
  bm_specifics->set_guid(kGuid);
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_title(kTitle);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  // The favicon service should be called with page url since the icon url is
  // missing.
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kUrl, _, _, _));
  const bookmarks::BookmarkNode* node = CreateBookmarkNodeFromSpecifics(
      *bm_specifics,
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      /*is_folder=*/false, model.get(), &favicon_service);
  EXPECT_THAT(node, NotNull());
}

TEST(BookmarkSpecificsConversionsTest, ShouldUpdateBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());
  model->SetNodeMetaInfo(node, kKey1, kValue1);
  model->SetNodeMetaInfo(node, kKey2, kValue2);

  const GURL kNewUrl("http://www.new-url.com");
  const std::string kNewTitle = "NewTitle";
  const GURL kNewIconUrl("http://www.new-icon-url.com");
  const std::string kNewValue1 = "new-value1";
  const std::string kNewValue2 = "new-value2";

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kNewUrl.spec());
  bm_specifics->set_guid(node->guid());
  bm_specifics->set_icon_url(kNewIconUrl.spec());
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_title(kNewTitle);
  bm_specifics->set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kNewValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kNewValue2);

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kNewUrl, base::UTF8ToUTF16(kNewTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewIconUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(*bm_specifics, node, model.get(),
                                  &favicon_service);
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kNewTitle)));
  EXPECT_THAT(node->url(), Eq(kNewUrl));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kNewValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kNewValue2));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldUpdateBookmarkNodeFromSpecificsWithFaviconAndWithoutIconUrl) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());

  const GURL kNewUrl("http://www.new-url.com");

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kNewUrl.spec());
  bm_specifics->set_guid(node->guid());
  bm_specifics->set_favicon("PNG");

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  // The favicon service should be called with page url since the icon url is
  // missing.
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(*bm_specifics, node, model.get(),
                                  &favicon_service);
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeValidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();

  // URL is irrelevant for a folder.
  bm_specifics->set_url("INVALID_URL");
  bm_specifics->set_guid(base::GenerateGUID());
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  bm_specifics->set_url("http://www.valid-url.com");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeValidBookmarkSpecificsWithFaviconAndWithoutIconUrl) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url("http://www.valid-url.com");
  bm_specifics->set_favicon("PNG");
  bm_specifics->set_guid(base::GenerateGUID());
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithoutFaviconAndWithIconUrl) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url("http://www.valid-url.com");
  bm_specifics->set_icon_url("http://www.valid-icon-url.com");
  bm_specifics->set_guid(base::GenerateGUID());
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithInvalidGUID) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();

  // Add empty GUID.
  bm_specifics->set_guid("");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  // Add invalid GUID.
  bm_specifics->set_guid("INVALID GUID");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  // Add valid GUID.
  bm_specifics->set_guid(base::GenerateGUID());
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeInvalidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  // Empty specifics.
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  // Add invalid url.
  bm_specifics->set_url("INVALID_URL");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));

  // Add a valid url.
  bm_specifics->set_url("http://www.valid-url.com");
  // Add redudant keys in meta_info.
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key("key");
  meta_info1->set_value("value1");

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key("key");
  meta_info2->set_value("value2");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));
}

TEST(BookmarkSpecificsConversionsTest, ReplaceUrlNodeWithUpdatedGUID) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const std::string kGuid = base::GenerateGUID();
  const base::string16 kTitle = base::ASCIIToUTF16("bar");
  const GURL kUrl = GURL("http://foo.com");
  const base::Time kCreationTime = base::Time::Now();

  auto meta_info_map = std::make_unique<bookmarks::BookmarkNode::MetaInfoMap>();
  const std::string kKey = "key";
  const std::string kValue = "value";
  (*meta_info_map)[kKey] = kValue;

  // Add a bookmark URL.
  const bookmarks::BookmarkNode* original_url = model->AddURL(
      bookmark_bar_node, 0, kTitle, kUrl, meta_info_map.get(), kCreationTime);

  // Replace url1.
  const bookmarks::BookmarkNode* new_url =
      ReplaceBookmarkNodeGUID(original_url, kGuid, model.get());

  // All data except for the GUID should be the same.
  EXPECT_EQ(kGuid, new_url->guid());
  EXPECT_EQ(kTitle, new_url->GetTitle());
  EXPECT_EQ(bookmark_bar_node, new_url->parent());
  EXPECT_EQ(0, bookmark_bar_node->GetIndexOf(new_url));
  EXPECT_EQ(kUrl, new_url->url());
  EXPECT_EQ(kCreationTime, new_url->date_added());
  std::string out_value_url;
  EXPECT_TRUE(new_url->GetMetaInfo(kKey, &out_value_url));
  EXPECT_EQ(kValue, out_value_url);
}

TEST(BookmarkSpecificsConversionsTest, ReplaceFolderNodeWithUpdatedGUID) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const std::string kGuid = base::GenerateGUID();
  const base::string16 kTitle = base::ASCIIToUTF16("foobar");

  auto meta_info_map = std::make_unique<bookmarks::BookmarkNode::MetaInfoMap>();
  const std::string kKey = "key";
  const std::string kValue = "value";
  (*meta_info_map)[kKey] = kValue;

  // Add a folder with child URLs.
  const bookmarks::BookmarkNode* original_folder =
      model->AddFolder(bookmark_bar_node, 0, kTitle, meta_info_map.get());
  const bookmarks::BookmarkNode* url1 = model->AddURL(
      original_folder, 0, base::ASCIIToUTF16("bar"), GURL("http://bar.com"));
  const bookmarks::BookmarkNode* url2 = model->AddURL(
      original_folder, 1, base::ASCIIToUTF16("foo"), GURL("http://foo.com"));

  // Replace folder1.
  const bookmarks::BookmarkNode* new_folder =
      ReplaceBookmarkNodeGUID(original_folder, kGuid, model.get());

  // All data except for the GUID should be the same.
  EXPECT_EQ(kGuid, new_folder->guid());
  EXPECT_EQ(kTitle, new_folder->GetTitle());
  EXPECT_EQ(bookmark_bar_node, new_folder->parent());
  EXPECT_EQ(0, bookmark_bar_node->GetIndexOf(new_folder));
  std::string out_value_folder;
  EXPECT_TRUE(new_folder->GetMetaInfo(kKey, &out_value_folder));
  EXPECT_EQ(kValue, out_value_folder);
  EXPECT_EQ(2u, new_folder->children().size());
  EXPECT_EQ(0, new_folder->GetIndexOf(url1));
  EXPECT_EQ(1, new_folder->GetIndexOf(url2));
}

}  // namespace

}  // namespace sync_bookmarks
