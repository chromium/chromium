// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_power_bookmark_data_provider.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class ShoppingPowerBookmarkDataProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    power_bookmark_service_ =
        std::make_unique<power_bookmarks::PowerBookmarkService>(
            bookmark_model_.get(), temp_directory_.GetPath(),
            backend_task_runner_);
    shopping_service_ = std::make_unique<MockShoppingService>();

    data_provider_ = std::make_unique<ShoppingPowerBookmarkDataProvider>(
        bookmark_model_.get(), power_bookmark_service_.get(),
        shopping_service_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<power_bookmarks::PowerBookmarkService>
      power_bookmark_service_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<ShoppingPowerBookmarkDataProvider> data_provider_;
};

// Ensure a new bookmark with a cluster ID that is already tracked by the user
// is automatically tracked.
TEST_F(ShoppingPowerBookmarkDataProviderTest,
       PriceTrackingStateForNewBookmark_Tracked) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* new_bookmark = bookmark_model_->AddNewURL(
      bookmark_model_->other_node(), 0, u"Title", GURL("https://example.com"));

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();

  ProductInfo info;
  info.title = "Product";
  info.product_cluster_id = cluster_id;

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsClusterIdTrackedByUserResponse(true);

  // Add a product that is already tracked with the same cluster ID as the new
  // product.
  AddProductBookmark(bookmark_model_.get(), u"Product",
                     GURL("http://example.com/1"), cluster_id, true);

  data_provider_->AttachMetadataForNewBookmark(new_bookmark, meta.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), new_bookmark));
}

TEST_F(ShoppingPowerBookmarkDataProviderTest,
       PriceTrackingStateForNewBookmark_NotTracked) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* new_bookmark = bookmark_model_->AddNewURL(
      bookmark_model_->other_node(), 0, u"Title", GURL("https://example.com"));

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();

  ProductInfo info;
  info.title = "Product";
  info.product_cluster_id = cluster_id;

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsClusterIdTrackedByUserResponse(false);

  // Add a product with the same ID that isn't tracked.
  AddProductBookmark(bookmark_model_.get(), u"Product",
                     GURL("http://example.com/1"), cluster_id, false);

  data_provider_->AttachMetadataForNewBookmark(new_bookmark, meta.get());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), new_bookmark));
}

}  // namespace
}  // namespace commerce