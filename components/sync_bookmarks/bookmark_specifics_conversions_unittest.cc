// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

using testing::_;
using testing::Eq;
using testing::Ge;
using testing::IsEmpty;
using testing::Not;
using testing::NotNull;

// Fork of enum InvalidBookmarkSpecificsError.
enum class InvalidBookmarkSpecificsError {
  kEmptySpecifics = 0,
  kInvalidURL = 1,
  kIconURLWithoutFavicon = 2,
  kInvalidIconURL = 3,
  kNonUniqueMetaInfoKeys = 4,
  kInvalidUuid = 5,
  kInvalidParentUuid = 6,
  kInvalidUniquePosition = 7,
  kBannedUuid = 8,

  kMaxValue = kBannedUuid,
};

sync_pb::UniquePosition RandomUniquePosition() {
  return syncer::UniquePosition::InitialPosition(
             syncer::UniquePosition::RandomSuffix())
      .ToProto();
}

// Returns a single-color 16x16 image using |color|.
gfx::Image CreateTestImage(SkColor color) {
  return gfx::test::CreateImage(/*size=*/16, color);
}

TEST(BookmarkSpecificsConversionsTest, ShouldCreateSpecificsFromBookmarkNode) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";
  const syncer::UniquePosition kUniquePosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      kUrl);
  ASSERT_THAT(node, NotNull());
  model.underlying_model()->SetDateAdded(node, kTime);
  model.underlying_model()->UpdateLastUsedTime(node, kTime,
                                               /*just_opened=*/false);
  model.underlying_model()->SetNodeMetaInfo(node, kKey1, kValue1);
  model.underlying_model()->SetNodeMetaInfo(node, kKey2, kValue2);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, kUniquePosition.ToProto(),
                                      /*force_favicon_load=*/false);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_THAT(bm_specifics.guid(), Eq(node->uuid().AsLowercaseString()));
  EXPECT_THAT(bm_specifics.parent_guid(), Eq(bookmarks::kBookmarkBarNodeUuid));
  EXPECT_THAT(bm_specifics.type(), Eq(sync_pb::BookmarkSpecifics::URL));
  EXPECT_THAT(bm_specifics.legacy_canonicalized_title(), Eq(kTitle));
  EXPECT_THAT(GURL(bm_specifics.url()), Eq(kUrl));
  EXPECT_THAT(base::Time::FromDeltaSinceWindowsEpoch(
                  base::Microseconds(bm_specifics.creation_time_us())),
              Eq(kTime));
  EXPECT_THAT(base::Time::FromDeltaSinceWindowsEpoch(
                  base::Microseconds(bm_specifics.last_used_time_us())),
              Eq(kTime));
  EXPECT_TRUE(syncer::UniquePosition::FromProto(bm_specifics.unique_position())
                  .Equals(kUniquePosition));
  for (const sync_pb::MetaInfo& meta_info : bm_specifics.meta_info()) {
    std::string value;
    node->GetMetaInfo(meta_info.key(), &value);
    EXPECT_THAT(meta_info.value(), Eq(value));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateSpecificsFromBookmarkNodeNoDateLastUsed) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const syncer::UniquePosition kUniquePosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      kUrl);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, kUniquePosition.ToProto(),
                                      /*force_favicon_load=*/false);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_THAT(bm_specifics.guid(), Eq(node->uuid().AsLowercaseString()));
  EXPECT_THAT(bm_specifics.parent_guid(), Eq(bookmarks::kBookmarkBarNodeUuid));
  EXPECT_FALSE(bm_specifics.has_last_used_time_us());
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateSpecificsFromBookmarkNodeWithIllegalTitle) {
  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const std::vector<std::string> illegal_titles = {"", ".", ".."};
  size_t index = 0;
  for (const std::string& illegal_title : illegal_titles) {
    const bookmarks::BookmarkNode* node = model.AddURL(
        /*parent=*/bookmark_bar_node, index++, base::UTF8ToUTF16(illegal_title),
        GURL("http://www.url.com"));
    ASSERT_THAT(node, NotNull());
    sync_pb::EntitySpecifics specifics =
        CreateSpecificsFromBookmarkNode(node, &model, RandomUniquePosition(),
                                        /*force_favicon_load=*/false);
    // Legacy clients append a space to illegal titles.
    EXPECT_THAT(specifics.bookmark().legacy_canonicalized_title(),
                Eq(illegal_title + " "));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateSpecificsWithoutUrlFromFolderNode) {
  TestBookmarkModelView model;
  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Title");
  ASSERT_THAT(node, NotNull());

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, RandomUniquePosition(),
                                      /*force_favicon_load=*/false);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_FALSE(bm_specifics.has_url());
  EXPECT_THAT(bm_specifics.type(), Eq(sync_pb::BookmarkSpecifics::FOLDER));
  EXPECT_FALSE(bm_specifics.has_last_used_time_us());
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldLoadFaviconWhenCreatingSpecificsFromBookmarkNode) {
  TestBookmarkModelView model;
  bookmarks::TestBookmarkClient* client_ptr = model.underlying_client();

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Title",
      GURL("http://www.url.com"));
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_FALSE(client_ptr->HasFaviconLoadTasks());
  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, RandomUniquePosition(),
                                      /*force_favicon_load=*/true);
  EXPECT_TRUE(client_ptr->HasFaviconLoadTasks());
  EXPECT_FALSE(specifics.bookmark().has_favicon());
  EXPECT_FALSE(specifics.bookmark().has_icon_url());
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldNotLoadFaviconWhenCreatingSpecificsFromBookmarkNode) {
  TestBookmarkModelView model;
  bookmarks::TestBookmarkClient* client_ptr = model.underlying_client();

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Title",
      GURL("http://www.url.com"));
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());
  ASSERT_FALSE(client_ptr->HasFaviconLoadTasks());
  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, &model, RandomUniquePosition(), /*force_favicon_load=*/false);
  EXPECT_FALSE(client_ptr->HasFaviconLoadTasks());
  EXPECT_FALSE(specifics.bookmark().has_favicon());
  EXPECT_FALSE(specifics.bookmark().has_icon_url());
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldIncludeFaviconWhenCreatingSpecificsFromBookmarkNodeIfLoaded) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kIconUrl("http://www.icon-url.com");
  const SkColor kColor = SK_ColorRED;

  TestBookmarkModelView model;
  bookmarks::TestBookmarkClient* client_ptr = model.underlying_client();

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Title", kBookmarkUrl);
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());

  // Complete the loading of the favicon as part of the test setup.
  model.GetFavicon(node);
  ASSERT_TRUE(client_ptr->HasFaviconLoadTasks());
  client_ptr->SimulateFaviconLoaded(kBookmarkUrl, kIconUrl,
                                    CreateTestImage(kColor));
  ASSERT_TRUE(node->is_favicon_loaded());

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, RandomUniquePosition(),
                                      /*force_favicon_load=*/false);
  EXPECT_THAT(specifics.bookmark().favicon(), Not(IsEmpty()));
  EXPECT_THAT(specifics.bookmark().icon_url(), Eq(kIconUrl));

  // Verify that the |favicon| field is properly encoded.
  const gfx::Image favicon = gfx::Image::CreateFrom1xPNGBytes(
      base::as_byte_span(specifics.bookmark().favicon()));
  EXPECT_THAT(favicon.Width(), Eq(16));
  EXPECT_THAT(favicon.Height(), Eq(16));
  EXPECT_THAT(favicon.AsBitmap().getColor(1, 1), Eq(kColor));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldOmitLargeFaviconUrlWhenCreatingSpecificsFromBookmarkNode) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kIconUrl(
      base::StrCat({"http://www.icon-url.com/", std::string(5000, 'a')}));
  const SkColor kColor = SK_ColorRED;

  // This test uses a valid but very long icon URL, larger than
  // |kMaxFaviconUrlSize|.
  ASSERT_TRUE(kIconUrl.is_valid());
  ASSERT_THAT(kIconUrl.spec().size(), Ge(5000u));

  TestBookmarkModelView model;
  bookmarks::TestBookmarkClient* client_ptr = model.underlying_client();

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Title", kBookmarkUrl);
  ASSERT_THAT(node, NotNull());
  ASSERT_FALSE(node->is_favicon_loaded());

  // Complete the loading of the favicon as part of the test setup.
  model.GetFavicon(node);
  ASSERT_TRUE(client_ptr->HasFaviconLoadTasks());
  client_ptr->SimulateFaviconLoaded(kBookmarkUrl, kIconUrl,
                                    CreateTestImage(kColor));
  ASSERT_TRUE(node->is_favicon_loaded());

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, &model, RandomUniquePosition(),
                                      /*force_favicon_load=*/false);

  // The icon URL should be omitted (populated with the empty string).
  EXPECT_TRUE(specifics.bookmark().has_icon_url());
  EXPECT_THAT(specifics.bookmark().icon_url(), IsEmpty());

  // The favicon image itself should be synced.
  EXPECT_THAT(specifics.bookmark().favicon(), Not(IsEmpty()));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateNonFolderBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kUrl.spec());
  bm_specifics.set_guid(kGuid.AsLowercaseString());
  bm_specifics.set_icon_url(kIconUrl.spec());
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_legacy_canonicalized_title(kTitle);
  bm_specifics.set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  bm_specifics.set_last_used_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);

  // Parent UUID and unique position are ignored by
  // CreateBookmarkNodeFromSpecifics(), but are required here to pass DCHECKs.
  bm_specifics.set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  TestBookmarkModelView model;
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kIconUrl, _, _, _));
  base::HistogramTester histogram_tester;
  const bookmarks::BookmarkNode* node =
      CreateBookmarkNodeFromSpecifics(bm_specifics,
                                      /*parent=*/model.bookmark_bar_node(),
                                      /*index=*/0, &model, &favicon_service);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->uuid(), Eq(kGuid));
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kTitle)));
  EXPECT_FALSE(node->is_folder());
  EXPECT_THAT(node->url(), Eq(kUrl));
  EXPECT_THAT(node->date_added(), Eq(kTime));
  EXPECT_THAT(node->date_last_used(), Eq(kTime));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kValue2));

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarkSpecificsExcludingFoldersContainFavicon",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST(BookmarkSpecificsConversionsTest, ShouldCreateFolderFromSpecifics) {
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_guid(kGuid.AsLowercaseString());
  bm_specifics.set_legacy_canonicalized_title(kTitle);
  bm_specifics.set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);

  // Parent UUID and unique position are ignored by
  // CreateBookmarkNodeFromSpecifics(), but are required here to pass DCHECKs.
  bm_specifics.set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  TestBookmarkModelView model;
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service, AddPageNoVisitForBookmark).Times(0);
  EXPECT_CALL(favicon_service, MergeFavicon).Times(0);
  EXPECT_CALL(favicon_service, DeleteFaviconMappings).Times(0);
  base::HistogramTester histogram_tester;
  const bookmarks::BookmarkNode* node =
      CreateBookmarkNodeFromSpecifics(bm_specifics,
                                      /*parent=*/model.bookmark_bar_node(),
                                      /*index=*/0, &model, &favicon_service);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->uuid(), Eq(kGuid));
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kTitle)));
  EXPECT_TRUE(node->is_folder());
  // TODO(crbug.com/40769579): Folders should propagate the creation time into
  // BookmarkModel, just like non-folders.
  EXPECT_THAT(node->date_added(), Ge(kTime));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kValue2));

  // The histogram should not be recorded for folders.
  histogram_tester.ExpectTotalCount(
      "Sync.BookmarkSpecificsExcludingFoldersContainFavicon", 0);
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldPreferFullTitleOnCreatingBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const std::string kFullTitle = "Title Long Version";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kUrl.spec());
  bm_specifics.set_guid(kGuid.AsLowercaseString());
  bm_specifics.set_icon_url(kIconUrl.spec());
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_legacy_canonicalized_title(kTitle);
  bm_specifics.set_full_title(kFullTitle);
  bm_specifics.set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);

  // Parent UUID and unique position are ignored by
  // CreateBookmarkNodeFromSpecifics(), but are required here to pass DCHECKs.
  bm_specifics.set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  TestBookmarkModelView model;
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kFullTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kIconUrl, _, _, _));
  const bookmarks::BookmarkNode* node =
      CreateBookmarkNodeFromSpecifics(bm_specifics,
                                      /*parent=*/model.bookmark_bar_node(),
                                      /*index=*/0, &model, &favicon_service);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->uuid(), Eq(kGuid));
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kFullTitle)));
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
  TestBookmarkModelView model;
  testing::NiceMock<favicon::MockFaviconService> favicon_service;

  const std::vector<std::string> illegal_titles = {"", ".", ".."};

  size_t index = 0;
  for (const std::string& illegal_title : illegal_titles) {
    sync_pb::BookmarkSpecifics bm_specifics;
    bm_specifics.set_url("http://www.url.com");
    bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
    // Legacy clients append an extra space to illegal clients.
    bm_specifics.set_legacy_canonicalized_title(illegal_title + " ");
    bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);

    // Parent UUID and unique position are ignored by
    // CreateBookmarkNodeFromSpecifics(), but are required here to pass DCHECKs.
    bm_specifics.set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
    *bm_specifics.mutable_unique_position() = RandomUniquePosition();

    const bookmarks::BookmarkNode* node =
        CreateBookmarkNodeFromSpecifics(bm_specifics,
                                        /*parent=*/model.bookmark_bar_node(),
                                        index++, &model, &favicon_service);
    ASSERT_THAT(node, NotNull());
    // The node should be created without the extra space.
    EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(illegal_title)));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldCreateBookmarkNodeFromSpecificsWithFaviconAndWithoutIconUrl) {
  const GURL kUrl("http://www.url.com");
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const GURL kIconUrl("http://www.icon-url.com");

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kUrl.spec());
  bm_specifics.set_guid(kGuid.AsLowercaseString());
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_legacy_canonicalized_title(kTitle);
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);

  // Parent UUID and unique position are ignored by
  // CreateBookmarkNodeFromSpecifics(), but are required here to pass DCHECKs.
  bm_specifics.set_parent_guid(bookmarks::kBookmarkBarNodeUuid);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  TestBookmarkModelView model;
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  // The favicon service should be called with page url since the icon url is
  // missing.
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl, kUrl, _, _, _));
  const bookmarks::BookmarkNode* node =
      CreateBookmarkNodeFromSpecifics(bm_specifics,
                                      /*parent=*/model.bookmark_bar_node(),
                                      /*index=*/0, &model, &favicon_service);
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

  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());
  model.underlying_model()->SetNodeMetaInfo(node, kKey1, kValue1);
  model.underlying_model()->SetNodeMetaInfo(node, kKey2, kValue2);

  const GURL kNewUrl("http://www.new-url.com");
  const std::string kNewTitle = "NewTitle";
  const GURL kNewIconUrl("http://www.new-icon-url.com");
  const std::string kNewValue1 = "new-value1";
  const std::string kNewValue2 = "new-value2";

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kNewUrl.spec());
  bm_specifics.set_guid(node->uuid().AsLowercaseString());
  bm_specifics.set_icon_url(kNewIconUrl.spec());
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_legacy_canonicalized_title(kNewTitle);
  bm_specifics.set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  bm_specifics.set_last_used_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kNewValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kNewValue2);

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kNewUrl, base::UTF8ToUTF16(kNewTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewIconUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(bm_specifics, node, &model, &favicon_service);
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
     ShouldPreferFullTitleOnUpdateBookmarkNodeFromSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());
  model.underlying_model()->SetNodeMetaInfo(node, kKey1, kValue1);
  model.underlying_model()->SetNodeMetaInfo(node, kKey2, kValue2);

  const GURL kNewUrl("http://www.new-url.com");
  const std::string kNewTitle = "NewTitle";
  const std::string kNewFullTitle = "NewTitle Long Version";
  const GURL kNewIconUrl("http://www.new-icon-url.com");
  const std::string kNewValue1 = "new-value1";
  const std::string kNewValue2 = "new-value2";

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kNewUrl.spec());
  bm_specifics.set_guid(node->uuid().AsLowercaseString());
  bm_specifics.set_icon_url(kNewIconUrl.spec());
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_legacy_canonicalized_title(kNewTitle);
  bm_specifics.set_full_title(kNewFullTitle);
  bm_specifics.set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kNewValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kNewValue2);

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  EXPECT_CALL(favicon_service, AddPageNoVisitForBookmark(
                                   kNewUrl, base::UTF8ToUTF16(kNewFullTitle)));
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewIconUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(bm_specifics, node, &model, &favicon_service);
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kNewFullTitle)));
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

  TestBookmarkModelView model;

  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  ASSERT_THAT(node, NotNull());

  const GURL kNewUrl("http://www.new-url.com");

  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url(kNewUrl.spec());
  bm_specifics.set_guid(node->uuid().AsLowercaseString());
  bm_specifics.set_favicon("PNG");

  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  // The favicon service should be called with page url since the icon url is
  // missing.
  EXPECT_CALL(favicon_service, MergeFavicon(kNewUrl, kNewUrl, _, _, _));
  UpdateBookmarkNodeFromSpecifics(bm_specifics, node, &model, &favicon_service);
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeValidBookmarkSpecifics) {
  sync_pb::BookmarkSpecifics bm_specifics;

  // URL is irrelevant for a folder.
  bm_specifics.set_url("INVALID_URL");
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_parent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();
  EXPECT_TRUE(IsValidBookmarkSpecifics(bm_specifics));

  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);
  ASSERT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  bm_specifics.set_url("http://www.valid-url.com");
  EXPECT_TRUE(IsValidBookmarkSpecifics(bm_specifics));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeValidBookmarkSpecificsWithFaviconAndWithoutIconUrl) {
  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url("http://www.valid-url.com");
  bm_specifics.set_favicon("PNG");
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_parent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();
  EXPECT_TRUE(IsValidBookmarkSpecifics(bm_specifics));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithoutFaviconAndWithIconUrl) {
  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_url("http://www.valid-url.com");
  bm_specifics.set_icon_url("http://www.valid-icon-url.com");
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kIconURLWithoutFavicon,
      /*expected_count=*/1);
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithInvalidUuid) {
  base::HistogramTester histogram_tester;
  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);
  bm_specifics.set_parent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  // No UUID.
  bm_specifics.clear_guid();
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidUuid,
      /*expected_count=*/1);

  // Add empty UUID.
  bm_specifics.set_guid("");
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidUuid,
      /*expected_count=*/2);

  // Add invalid UUID.
  bm_specifics.set_guid("INVALID UUID");
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidUuid,
      /*expected_count=*/3);

  // Add valid UUID.
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  ASSERT_TRUE(IsValidBookmarkSpecifics(bm_specifics));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithInvalidParentUuid) {
  base::HistogramTester histogram_tester;
  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();

  // No parent UUID.
  bm_specifics.clear_parent_guid();
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidParentUuid,
      /*expected_count=*/1);

  // Add invalid parent UUID.
  bm_specifics.set_parent_guid("INVALID UUID");
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidParentUuid,
      /*expected_count=*/2);

  // Add valid UUID.
  bm_specifics.set_parent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  ASSERT_TRUE(IsValidBookmarkSpecifics(bm_specifics));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsDueToInvalidUniquePosition) {
  sync_pb::BookmarkSpecifics bm_specifics;
  bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);

  // Leave |unique_position| field populated but empty.
  bm_specifics.mutable_unique_position();

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kInvalidUniquePosition,
      /*expected_count=*/1);
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldBeInvalidBookmarkSpecificsWithBannedUuid) {
  ASSERT_THAT(bookmarks::kBannedUuidDueToPastSyncBug,
              Eq(InferGuidFromLegacyOriginatorId(
                     /*originator_cache_guid=*/"",
                     /*originator_client_item_id=*/"")
                     .AsLowercaseString()));

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  *bm_specifics->mutable_unique_position() = RandomUniquePosition();
  bm_specifics->set_guid(bookmarks::kBannedUuidDueToPastSyncBug);
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kBannedUuid,
      /*expected_count=*/1);
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeInvalidBookmarkSpecifics) {
  sync_pb::BookmarkSpecifics bm_specifics;

  // Empty specifics.
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
    histogram_tester.ExpectBucketCount(
        "Sync.InvalidBookmarkSpecifics",
        /*sample=*/InvalidBookmarkSpecificsError::kEmptySpecifics,
        /*expected_count=*/1);
  }

  {
    base::HistogramTester histogram_tester;
    bm_specifics.set_type(sync_pb::BookmarkSpecifics::FOLDER);
    EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
    histogram_tester.ExpectBucketCount(
        "Sync.InvalidBookmarkSpecifics",
        /*sample=*/InvalidBookmarkSpecificsError::kInvalidUuid,
        /*expected_count=*/1);
  }

  // Populate the required fields.
  bm_specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  bm_specifics.set_parent_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  *bm_specifics.mutable_unique_position() = RandomUniquePosition();
  ASSERT_TRUE(IsValidBookmarkSpecifics(bm_specifics));

  {
    base::HistogramTester histogram_tester;
    bm_specifics.set_type(sync_pb::BookmarkSpecifics::URL);
    EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
    histogram_tester.ExpectBucketCount(
        "Sync.InvalidBookmarkSpecifics",
        /*sample=*/InvalidBookmarkSpecificsError::kInvalidURL,
        /*expected_count=*/1);
  }

  // Add invalid url.
  {
    base::HistogramTester histogram_tester;
    bm_specifics.set_url("INVALID_URL");
    EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
    histogram_tester.ExpectBucketCount(
        "Sync.InvalidBookmarkSpecifics",
        /*sample=*/InvalidBookmarkSpecificsError::kInvalidURL,
        /*expected_count=*/1);
  }

  // Add a valid url.
  bm_specifics.set_url("http://www.valid-url.com");
  ASSERT_TRUE(IsValidBookmarkSpecifics(bm_specifics));

  sync_pb::MetaInfo* meta_info1 = bm_specifics.add_meta_info();
  meta_info1->set_key("key");
  meta_info1->set_value("value1");
  ASSERT_TRUE(IsValidBookmarkSpecifics(bm_specifics));

  // Add redudant keys in meta_info.
  base::HistogramTester histogram_tester;
  sync_pb::MetaInfo* meta_info2 = bm_specifics.add_meta_info();
  meta_info2->set_key("key");
  meta_info2->set_value("value2");
  EXPECT_FALSE(IsValidBookmarkSpecifics(bm_specifics));
  histogram_tester.ExpectBucketCount(
      "Sync.InvalidBookmarkSpecifics",
      /*sample=*/InvalidBookmarkSpecificsError::kNonUniqueMetaInfoKeys,
      /*expected_count=*/1);
}

TEST(BookmarkSpecificsConversionsTest, ReplaceUrlNodeWithUpdatedUuid) {
  TestBookmarkModelView model;
  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::u16string kTitle = u"bar";
  const GURL kUrl = GURL("http://foo.com");
  const base::Time kCreationTime = base::Time::Now();

  auto meta_info_map = std::make_unique<bookmarks::BookmarkNode::MetaInfoMap>();
  const std::string kKey = "key";
  const std::string kValue = "value";
  (*meta_info_map)[kKey] = kValue;

  // Add a bookmark URL.
  const bookmarks::BookmarkNode* original_url = model.AddURL(
      bookmark_bar_node, 0, kTitle, kUrl, meta_info_map.get(), kCreationTime);

  // Replace url1.
  const bookmarks::BookmarkNode* new_url =
      ReplaceBookmarkNodeUuid(original_url, kGuid, &model);

  // All data except for the UUID should be the same.
  EXPECT_EQ(kGuid, new_url->uuid());
  EXPECT_EQ(kTitle, new_url->GetTitle());
  EXPECT_EQ(bookmark_bar_node, new_url->parent());
  EXPECT_EQ(0u, bookmark_bar_node->GetIndexOf(new_url));
  EXPECT_EQ(kUrl, new_url->url());
  EXPECT_EQ(kCreationTime, new_url->date_added());
  std::string out_value_url;
  EXPECT_TRUE(new_url->GetMetaInfo(kKey, &out_value_url));
  EXPECT_EQ(kValue, out_value_url);
}

TEST(BookmarkSpecificsConversionsTest, ReplaceFolderNodeWithUpdatedUuid) {
  TestBookmarkModelView model;
  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const base::Uuid kGuid = base::Uuid::GenerateRandomV4();
  const std::u16string kTitle = u"foobar";

  auto meta_info_map = std::make_unique<bookmarks::BookmarkNode::MetaInfoMap>();
  const std::string kKey = "key";
  const std::string kValue = "value";
  (*meta_info_map)[kKey] = kValue;

  // Add a folder with child URLs.
  const bookmarks::BookmarkNode* original_folder =
      model.AddFolder(bookmark_bar_node, 0, kTitle, meta_info_map.get());
  const bookmarks::BookmarkNode* url1 =
      model.AddURL(original_folder, 0, u"bar", GURL("http://bar.com"));
  const bookmarks::BookmarkNode* url2 =
      model.AddURL(original_folder, 1, u"foo", GURL("http://foo.com"));

  // Replace folder1.
  const bookmarks::BookmarkNode* new_folder =
      ReplaceBookmarkNodeUuid(original_folder, kGuid, &model);

  // All data except for the UUID should be the same.
  EXPECT_EQ(kGuid, new_folder->uuid());
  EXPECT_EQ(kTitle, new_folder->GetTitle());
  EXPECT_EQ(bookmark_bar_node, new_folder->parent());
  EXPECT_EQ(0u, bookmark_bar_node->GetIndexOf(new_folder));
  std::string out_value_folder;
  EXPECT_TRUE(new_folder->GetMetaInfo(kKey, &out_value_folder));
  EXPECT_EQ(kValue, out_value_folder);
  EXPECT_EQ(2u, new_folder->children().size());
  EXPECT_EQ(0u, new_folder->GetIndexOf(url1));
  EXPECT_EQ(1u, new_folder->GetIndexOf(url2));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldConsiderValidBookmarkGuidIfMatchesClientTag) {
  const std::string kGuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::BookmarkSpecifics specifics;
  specifics.set_guid(kGuid);

  EXPECT_TRUE(HasExpectedBookmarkGuid(
      specifics, syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS, kGuid),
      /*originator_cache_guid=*/"",
      /*originator_client_item_id=*/""));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldConsiderValidBookmarkGuidIfMatchesOriginator) {
  const std::string kGuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::BookmarkSpecifics specifics;
  specifics.set_guid(kGuid);

  EXPECT_TRUE(HasExpectedBookmarkGuid(specifics, syncer::ClientTagHash(),
                                      /*originator_cache_guid=*/"",
                                      /*originator_client_item_id=*/kGuid));
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldConsiderInvalidBookmarkGuidIfEmptyOriginator) {
  const std::string kGuid = InferGuidFromLegacyOriginatorId(
                                /*originator_cache_guid=*/"",
                                /*=originator_client_item_id=*/"")
                                .AsLowercaseString();

  sync_pb::BookmarkSpecifics specifics;
  specifics.set_guid(kGuid);

  EXPECT_FALSE(HasExpectedBookmarkGuid(specifics,
                                       syncer::ClientTagHash::FromHashed("foo"),
                                       /*originator_cache_guid=*/"",
                                       /*originator_client_item_id=*/""));
}

}  // namespace

}  // namespace sync_bookmarks
