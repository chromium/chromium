// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/commerce/core/webui/shopping_list_handler.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class MockPage : public shopping_list::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<shopping_list::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<shopping_list::mojom::Page> receiver_{this};

  MOCK_METHOD1(PriceTrackedForBookmark,
               void(shopping_list::mojom::BookmarkProductInfoPtr product));
  MOCK_METHOD1(PriceUntrackedForBookmark, void(int64_t bookmark_id));
};

void GetEvaluationProductInfos(
    base::OnceClosure closure,
    std::vector<shopping_list::mojom::BookmarkProductInfoPtr> expected,
    std::vector<shopping_list::mojom::BookmarkProductInfoPtr> found) {
  ASSERT_EQ(expected.size(), found.size());
  for (size_t i = 0; i < expected.size(); i++) {
    ASSERT_EQ(expected[i]->bookmark_id, found[i]->bookmark_id);
    ASSERT_EQ(expected[i]->info->current_price, found[i]->info->current_price);
    ASSERT_EQ(expected[i]->info->domain, found[i]->info->domain);
    ASSERT_EQ(expected[i]->info->title, found[i]->info->title);
    ASSERT_EQ(expected[i]->info->image_url.spec(),
              found[i]->info->image_url.spec());
  }
  std::move(closure).Run();
}

class ShoppingListHandlerTest : public testing::Test {
 public:
  ShoppingListHandlerTest() { features_.InitAndEnableFeature(kShoppingList); }

 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    shopping_service_ = std::make_unique<MockShoppingService>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterPrefs(pref_service_->registry());
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);
    handler_ = std::make_unique<commerce::ShoppingListHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, "en-us");
  }

  MockPage page_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingListHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(ShoppingListHandlerTest, ConvertToMojoTypes) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  const std::string image_url = "https://example.com/image.png";
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), product);
  meta->mutable_lead_image()->set_url(image_url);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_amount_micros(4560000);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_currency_code("usd");
  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_.get(), product,
                                            std::move(meta));

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);

  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingListHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_EQ(mojo_list[0]->info->previous_price, "$4.56");
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
  EXPECT_EQ(mojo_list[0]->info->image_url.spec(), image_url);
}

// If the new price is greater than the old price, we shouldn't include the
// |previous_price| field in the mojo data type.
TEST_F(ShoppingListHandlerTest, ConvertToMojoTypes_PriceIncrease) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  const std::string image_url = "https://example.com/image.png";
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), product);
  meta->mutable_lead_image()->set_url(image_url);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_amount_micros(1000000);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_currency_code("usd");
  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_.get(), product,
                                            std::move(meta));

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);

  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingListHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_TRUE(mojo_list[0]->info->previous_price.empty());
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
  EXPECT_EQ(mojo_list[0]->info->image_url.spec(), image_url);
}

TEST_F(ShoppingListHandlerTest, TestTrackProdcutSuccess) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      false, 1230000, "usd");
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));

  EXPECT_CALL(page_, PriceUntrackedForBookmark(product->id())).Times(1);
  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(1);
  handler_->TrackPriceForBookmark(product->id());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(ShoppingListHandlerTest, TestUntrackProductSuccess) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");
  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product));

  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(1);
  EXPECT_CALL(page_, PriceUntrackedForBookmark(product->id())).Times(1);
  handler_->UntrackPriceForBookmark(product->id());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(ShoppingListHandlerTest, TestTrackProdcutFailure) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      false, 1230000, "usd");
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));

  // Simulate failed calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetUnsubscribeCallbackValue(false);

  EXPECT_CALL(page_, PriceUntrackedForBookmark(product->id())).Times(2);
  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(0);
  handler_->TrackPriceForBookmark(product->id());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(ShoppingListHandlerTest, TestUntrackProductFailure) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");
  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product));

  // Simulate failed calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetUnsubscribeCallbackValue(false);

  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(2);
  EXPECT_CALL(page_, PriceUntrackedForBookmark(product->id())).Times(0);
  handler_->UntrackPriceForBookmark(product->id());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(ShoppingListHandlerTest, PageUpdateForPriceTrackChange) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  EXPECT_CALL(page_, PriceUntrackedForBookmark(product->id()));
  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(
      shopping_service_.get(), bookmark_model_.get(), product, false,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            EXPECT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(ShoppingListHandlerTest, PageNotUpdateForIrrelevantChange) {
  const bookmarks::BookmarkNode* node =
      bookmark_model_->AddNewURL(bookmark_model_->other_node(), 0, u"product 1",
                                 GURL("http://example.com/1"));
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), node));

  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(0);
  EXPECT_CALL(page_, PriceUntrackedForBookmark(node->id())).Times(0);
  bookmark_model_->SetNodeMetaInfo(node, "test_key", "test_value");
}

TEST_F(ShoppingListHandlerTest, TestGetProductInfo_FeatureEnabled) {
  base::RunLoop run_loop;
  EXPECT_CALL(tracker_, NotifyEvent("price_tracking_side_panel_shown"));

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingListHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  handler_->GetAllPriceTrackedBookmarkProductInfo(
      base::BindOnce(&GetEvaluationProductInfos, run_loop.QuitClosure(),
                     std::move(mojo_list)));
}

class ShoppingListHandlerFeatureDisableTest : public testing::Test {
 public:
  ShoppingListHandlerFeatureDisableTest() {
    features_.InitAndDisableFeature(kShoppingList);
  }

 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    shopping_service_ = std::make_unique<MockShoppingService>();
    handler_ = std::make_unique<commerce::ShoppingListHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, "en-us");
  }

  MockPage page_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingListHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(ShoppingListHandlerFeatureDisableTest,
       TestGetProductInfo_FeatureDisabled) {
  shopping_service_->SetIsShoppingListEligible(false);
  base::RunLoop run_loop;
  EXPECT_CALL(tracker_, NotifyEvent("price_tracking_side_panel_shown"))
      .Times(0);

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> empty_list;

  handler_->GetAllPriceTrackedBookmarkProductInfo(
      base::BindOnce(&GetEvaluationProductInfos, run_loop.QuitClosure(),
                     std::move(empty_list)));
}

}  // namespace
}  // namespace commerce