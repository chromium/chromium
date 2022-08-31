// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/test_utils.h"
#include "components/commerce/core/webui/shopping_list_handler.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class ShoppingListHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(ShoppingListHandlerTest, ConvertToMojoTypes) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);

  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingListHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
}

}  // namespace
}  // namespace commerce