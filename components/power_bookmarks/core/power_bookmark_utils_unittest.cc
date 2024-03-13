// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {
namespace {

using testing::UnorderedElementsAre;

const std::string kLeadImageUrl = "image.png";

const char16_t kExampleTitle[] = u"Title";
const std::string kExampleUrl = "https://example.com";

class PowerBookmarkUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
  }

  bookmarks::BookmarkModel* model() { return model_.get(); }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> model_;
};

TEST_F(PowerBookmarkUtilsTest, TestAddAndAccess) {
  const bookmarks::BookmarkNode* node = model()->AddURL(
      model()->bookmark_bar_node(), 0, kExampleTitle, GURL(kExampleUrl));

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  meta->mutable_lead_image()->set_url(kLeadImageUrl);

  SetNodePowerBookmarkMeta(model(), node, std::move(meta));

  const std::unique_ptr<PowerBookmarkMeta> fetched_meta =
      GetNodePowerBookmarkMeta(model(), node);

  EXPECT_EQ(kLeadImageUrl, fetched_meta->lead_image().url());
}

TEST_F(PowerBookmarkUtilsTest, TestAddAndDelete) {
  const bookmarks::BookmarkNode* node = model()->AddURL(
      model()->bookmark_bar_node(), 0, kExampleTitle, GURL(kExampleUrl));

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  meta->mutable_lead_image()->set_url(kLeadImageUrl);

  SetNodePowerBookmarkMeta(model(), node, std::move(meta));

  DeleteNodePowerBookmarkMeta(model(), node);

  const std::unique_ptr<PowerBookmarkMeta> fetched_meta =
      GetNodePowerBookmarkMeta(model(), node);

  EXPECT_EQ(nullptr, fetched_meta.get());
}

TEST_F(PowerBookmarkUtilsTest, GetBookmarksMatchingPropertiesFilterTags) {
  const bookmarks::BookmarkNode* node1 = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
  std::unique_ptr<PowerBookmarkMeta> meta1 =
      std::make_unique<PowerBookmarkMeta>();
  meta1->add_tags()->set_display_name("search");
  SetNodePowerBookmarkMeta(model(), node1, std::move(meta1));

  const bookmarks::BookmarkNode* node2 = model()->AddURL(
      model()->other_node(), 0, u"baz buz", GURL("http://www.cnn.com"));
  std::unique_ptr<PowerBookmarkMeta> meta2 =
      std::make_unique<PowerBookmarkMeta>();
  meta2->add_tags()->set_display_name("news");
  SetNodePowerBookmarkMeta(model(), node2, std::move(meta2));

  PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();

  // Test that the correct bookmark is returned for the "search" tag.
  query.tags.push_back(u"search");
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1));
  query.tags.clear();

  // Test that the correct bookmark is returned for the "news" tag.
  query.tags.push_back(u"news");
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node2));
  query.tags.clear();

  // Test that there are no results when valid but mutually exclusive tags are
  // specified.
  query.tags.push_back(u"news");
  query.tags.push_back(u"search");
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());
  query.tags.clear();

  // Test that no bookmarks are returned for unknown tag.
  query.tags.push_back(u"foo");
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());
  query.tags.clear();

  // Test that no bookmarks are returned for a totally empty query.
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());
  query.tags.clear();

  // Test that a query plus tag returns the correct bookmark.
  query.tags.push_back(u"news");
  *query.word_phrase_query = u"baz";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node2));
  query.tags.clear();

  // Test that a mismatched query and tag returns nothing.
  query.tags.push_back(u"search");
  *query.word_phrase_query = u"baz";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());
  query.tags.clear();
}

TEST_F(PowerBookmarkUtilsTest, GetBookmarksMatchingPropertiesSearchTags) {
  const bookmarks::BookmarkNode* node1 = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
  std::unique_ptr<PowerBookmarkMeta> meta1 =
      std::make_unique<PowerBookmarkMeta>();
  meta1->add_tags()->set_display_name("search");
  SetNodePowerBookmarkMeta(model(), node1, std::move(meta1));

  const bookmarks::BookmarkNode* node2 = model()->AddURL(
      model()->other_node(), 0, u"baz buz", GURL("http://www.cnn.com"));
  std::unique_ptr<PowerBookmarkMeta> meta2 =
      std::make_unique<PowerBookmarkMeta>();
  meta2->add_tags()->set_display_name("news");
  SetNodePowerBookmarkMeta(model(), node2, std::move(meta2));

  PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();

  // Test that a query for a substring in a tag and having a specified tag
  // finds the correct node.
  query.tags.push_back(u"news");
  *query.word_phrase_query = u"ews";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node2));
  query.tags.clear();

  // Test that a query for a substring in a tag finds the correct node.
  *query.word_phrase_query = u"ews";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node2));
  query.tags.clear();

  // Test that a query for the start of a tag finds the correct node.
  *query.word_phrase_query = u"sea";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1));
  query.tags.clear();
}

TEST_F(PowerBookmarkUtilsTest,
       GetBookmarksMatchingPropertiesSearchMultipleTags) {
  const bookmarks::BookmarkNode* node1 = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
  std::unique_ptr<PowerBookmarkMeta> meta1 =
      std::make_unique<PowerBookmarkMeta>();
  meta1->add_tags()->set_display_name("search");
  meta1->add_tags()->set_display_name("news");
  SetNodePowerBookmarkMeta(model(), node1, std::move(meta1));

  const bookmarks::BookmarkNode* node2 = model()->AddURL(
      model()->other_node(), 0, u"baz buz", GURL("http://www.cnn.com"));
  std::unique_ptr<PowerBookmarkMeta> meta2 =
      std::make_unique<PowerBookmarkMeta>();
  meta2->add_tags()->set_display_name("news");
  SetNodePowerBookmarkMeta(model(), node2, std::move(meta2));

  PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();

  // Test that a query that contains multiple tags finds results that have all
  // of those tags.
  *query.word_phrase_query = u"news search";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1));
  query.tags.clear();

  // Make sure searching for one tag finds both bookmarks.
  *query.word_phrase_query = u"news";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1, node2));
  query.tags.clear();
}

TEST_F(PowerBookmarkUtilsTest, GetBookmarksMatchingPropertiesStringSearch) {
  const bookmarks::BookmarkNode* node1 = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));

  const bookmarks::BookmarkNode* node2 = model()->AddURL(
      model()->other_node(), 0, u"baz buz", GURL("http://www.cnn.com"));

  PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();

  *query.word_phrase_query = u"bar";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1));

  *query.word_phrase_query = u"baz";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node2));

  // A string search for "ba" should find both nodes.
  *query.word_phrase_query = u"ba";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1, node2));

  // Ensure a search checks the URL.
  *query.word_phrase_query = u"goog";
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1));

  // Ensure a string that doesn't exist in the bookmarks returns nothing.
  *query.word_phrase_query = u"zzz";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());

  // Check that two strings from different bookmarks returns nothing.
  *query.word_phrase_query = u"foo buz";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());

  // Ensure an empty string returns no bookmarks.
  *query.word_phrase_query = u"";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model(), query, 100).empty());
}

TEST_F(PowerBookmarkUtilsTest, GetBookmarksMatchingPropertiesFolderSearch) {
  model()->AddURL(model()->other_node(), 0, u"foo example",
                  GURL("http://www.google.com"));

  model()->AddURL(model()->other_node(), 0, u"baz example",
                  GURL("http://www.cnn.com"));

  const bookmarks::BookmarkNode* folder =
      model()->AddFolder(model()->other_node(), 0, u"test folder");

  const bookmarks::BookmarkNode* node = model()->AddURL(
      folder, 0, u"buz example", GURL("http://www.example.com"));

  PowerBookmarkQueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();

  *query.word_phrase_query = u"example";
  query.folder = nullptr;
  EXPECT_EQ(3U, GetBookmarksMatchingProperties(model(), query, 100).size());

  *query.word_phrase_query = u"example";
  query.folder = folder;
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node));
}

TEST_F(PowerBookmarkUtilsTest, GetBookmarksMatchingPropertiesTypeSearch) {
  const bookmarks::BookmarkNode* node1 = model()->AddURL(
      model()->other_node(), 0, u"foo bar", GURL("http://www.google.com"));
  std::unique_ptr<PowerBookmarkMeta> meta1 =
      std::make_unique<PowerBookmarkMeta>();
  meta1->mutable_shopping_specifics()->set_title("Example Title");
  SetNodePowerBookmarkMeta(model(), node1, std::move(meta1));

  const bookmarks::BookmarkNode* node2 = model()->AddURL(
      model()->other_node(), 0, u"baz buz", GURL("http://www.cnn.com"));
  std::unique_ptr<PowerBookmarkMeta> meta2 =
      std::make_unique<PowerBookmarkMeta>();
  meta2->mutable_shopping_specifics()->set_title("Example Title");
  SetNodePowerBookmarkMeta(model(), node2, std::move(meta2));

  const bookmarks::BookmarkNode* node3 = model()->AddURL(
      model()->other_node(), 0, u"chromium", GURL("http://www.chromium.org"));
  std::unique_ptr<PowerBookmarkMeta> meta3 =
      std::make_unique<PowerBookmarkMeta>();
  SetNodePowerBookmarkMeta(model(), node3, std::move(meta3));

  const bookmarks::BookmarkNode* normal_node =
      model()->AddURL(model()->other_node(), 0, u"example page",
                      GURL("http://www.example.com"));

  PowerBookmarkQueryFields query;

  // Test that a query with no type returns all results.
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1, node2, node3, normal_node));

  // Test that a query for the SHOPPING type returns the correct results.
  query.type = PowerBookmarkType::SHOPPING;
  EXPECT_THAT(GetBookmarksMatchingProperties(model(), query, 100),
              UnorderedElementsAre(node1, node2));
}

TEST_F(PowerBookmarkUtilsTest, EncodeAndDecodeForPersistence) {
  PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_title("Example Title");

  std::string encoded_data;
  EncodeMetaForStorage(meta, &encoded_data);

  PowerBookmarkMeta out_meta;
  EXPECT_TRUE(DecodeMetaFromStorage(encoded_data, &out_meta));

  ASSERT_EQ(meta.has_shopping_specifics(), out_meta.has_shopping_specifics());
  ASSERT_EQ(meta.shopping_specifics().title(),
            out_meta.shopping_specifics().title());
}

}  // namespace
}  // namespace power_bookmarks
