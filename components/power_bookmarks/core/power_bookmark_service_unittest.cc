// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

class PowerBookmarkServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    service_ = std::make_unique<PowerBookmarkService>(model_.get());
  }

  void TearDown() override {
    // Manually free PowerBookmarkService because it uses BookmarkModel.
    service_.reset();
  }

  PowerBookmarkService* service() { return service_.get(); }

  bookmarks::BookmarkModel* model() { return model_.get(); }

 private:
  std::unique_ptr<PowerBookmarkService> service_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
};

class MockDataProvider : public PowerBookmarkDataProvider {
 public:
  MOCK_METHOD2(AttachMetadataForNewBookmark,
               void(const bookmarks::BookmarkNode* node,
                    PowerBookmarkMeta* meta));
};

TEST_F(PowerBookmarkServiceTest, AddDataProvider) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);

  EXPECT_CALL(data_provider,
              AttachMetadataForNewBookmark(testing::_, testing::_));

  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"Title",
                     GURL("https://example.com"));
}

TEST_F(PowerBookmarkServiceTest, RemoveDataProvider) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);
  service()->RemoveDataProvider(&data_provider);
  EXPECT_CALL(data_provider,
              AttachMetadataForNewBookmark(testing::_, testing::_))
      .Times(0);

  model()->AddNewURL(model()->bookmark_bar_node(), 0, u"Title",
                     GURL("https://example.com"));
}

TEST_F(PowerBookmarkServiceTest, AddDataProviderNoNewBookmark) {
  MockDataProvider data_provider;
  service()->AddDataProvider(&data_provider);
  EXPECT_CALL(data_provider,
              AttachMetadataForNewBookmark(testing::_, testing::_))
      .Times(0);

  // Data providers should only be called when new bookmarks are added.
  model()->AddURL(model()->bookmark_bar_node(), 0, u"Title",
                  GURL("https://example.com"));
}

}  // namespace power_bookmarks
