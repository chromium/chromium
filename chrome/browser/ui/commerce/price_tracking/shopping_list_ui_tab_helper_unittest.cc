// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace commerce {

namespace {

const char kProductUrl[] = "http://example.com";
const char kProductImageUrl[] = "http://example.com/image.png";
const uint64_t kClusterId = 12345L;

// Build a ProductInfo with the specified cluster ID and image URL. If the image
// URL is not specified, it is left empty in the info object.
absl::optional<ProductInfo> CreateProductInfo(uint64_t cluster_id,
                                              const GURL& url = GURL()) {
  absl::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  if (!url.is_empty()) {
    info->image_url = url;
  }
  return info;
}

}  // namespace

class ShoppingListUiTabHelperTest : public testing::Test {
 public:
  ShoppingListUiTabHelperTest()
      : shopping_service_(std::make_unique<MockShoppingService>()),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        image_fetcher_(std::make_unique<image_fetcher::MockImageFetcher>()) {}

  ShoppingListUiTabHelperTest(const ShoppingListUiTabHelperTest&) = delete;
  ShoppingListUiTabHelperTest operator=(const ShoppingListUiTabHelperTest&) =
      delete;
  ~ShoppingListUiTabHelperTest() override = default;

  void SetUp() override {
    web_contents_ = test_web_contents_factory_.CreateWebContents(&profile_);
    ShoppingListUiTabHelper::CreateForWebContents(
        web_contents_.get(), shopping_service_.get(), bookmark_model_.get(),
        image_fetcher_.get());
    tab_helper_ = ShoppingListUiTabHelper::FromWebContents(web_contents_.get());
  }

  void TestBody() override {}

  void TearDown() override {
    // Make sure the tab helper id destroyed before any of its dependencies are.
    web_contents_->RemoveUserData(ShoppingListUiTabHelper::UserDataKey());
  }

  void SetupImageFetcherForSimpleImage() {
    ON_CALL(*image_fetcher_, FetchImageAndData_)
        .WillByDefault(
            [](const GURL& image_url,
               image_fetcher::ImageDataFetcherCallback* image_data_callback,
               image_fetcher::ImageFetcherCallback* image_callback,
               image_fetcher::ImageFetcherParams params) {
              SkBitmap bitmap;
              bitmap.allocN32Pixels(1, 1);
              gfx::Image image =
                  gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

              std::move(*image_callback)
                  .Run(std::move(image), image_fetcher::RequestMetadata());
            });
  }

  void SimulateNavigationCommitted(const GURL& url) {
    content::WebContentsTester::For(web_contents_.get())
        ->SetLastCommittedURL(url);

    content::LoadCommittedDetails details;
    details.is_in_active_page = true;
    tab_helper_->NavigationEntryCommitted(details);
  }

  // Passthrough to private methods in ShoppindListUiTabHelper:
  void HandleProductInfoResponse(const GURL& url,
                                 const absl::optional<ProductInfo>& info) {
    tab_helper_->HandleProductInfoResponse(url, info);
  }

 protected:
  base::raw_ptr<ShoppingListUiTabHelper> tab_helper_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
  base::raw_ptr<content::WebContents> web_contents_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory test_web_contents_factory_;
};

TEST_F(ShoppingListUiTabHelperTest, TestSubscriptionEventsUpdateState) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));

  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  SimulateNavigationCommitted(GURL(kProductUrl));

  // First ensure that subscribe is successful.
  tab_helper_->OnSubscribe({CreateUserTrackedSubscription(kClusterId)}, true);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(tab_helper_->IsPriceTracking());

  // Now assume the user has unsubscribed again.
  shopping_service_->SetIsSubscribedCallbackValue(false);
  tab_helper_->OnUnsubscribe({CreateUserTrackedSubscription(kClusterId)}, true);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(tab_helper_->IsPriceTracking());
}

// The price tracking icon shouldn't be available if no image URL was provided
// by the shopping service.
TEST_F(ShoppingListUiTabHelperTest, TestIconAvailabilityIfNoImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  // Intentionally exclude the image here.
  absl::optional<ProductInfo> info = CreateProductInfo(kClusterId);
  // We do the setup for the image fetcher, but it won't be used since the
  // shopping service doesn't return an image URL.
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(tab_helper_->ShouldShowPriceTrackingIconView());
  ASSERT_TRUE(tab_helper_->GetProductImageURL().is_empty());
}

// The price tracking state should not update in the helper if there is no image
// returbed by the shopping service.
TEST_F(ShoppingListUiTabHelperTest, TestIconAvailabilityWithImage) {
  ASSERT_FALSE(tab_helper_->IsPriceTracking());

  AddProductBookmark(bookmark_model_.get(), u"title", GURL(kProductUrl),
                     kClusterId, true);

  absl::optional<ProductInfo> info =
      CreateProductInfo(kClusterId, GURL(kProductImageUrl));
  SetupImageFetcherForSimpleImage();

  shopping_service_->SetIsShoppingListEligible(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  SimulateNavigationCommitted(GURL(kProductUrl));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(tab_helper_->ShouldShowPriceTrackingIconView());
  ASSERT_EQ(GURL(kProductImageUrl), tab_helper_->GetProductImageURL());
}

}  // namespace commerce
