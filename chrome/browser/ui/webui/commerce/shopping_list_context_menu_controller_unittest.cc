// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace commerce {
namespace {

class MockShoppingServiceHandler : public ShoppingServiceHandler {
 public:
  explicit MockShoppingServiceHandler(bookmarks::BookmarkModel* bookmark_model,
                                      ShoppingService* shopping_service)
      : ShoppingServiceHandler(
            mojo::PendingRemote<shopping_service::mojom::Page>(),
            mojo::PendingReceiver<
                shopping_service::mojom::ShoppingServiceHandler>(),
            bookmark_model,
            shopping_service,
            nullptr,
            nullptr,
            nullptr,
            nullptr) {}

  MOCK_METHOD(void, TrackPriceForBookmark, (int64_t bookmark_id));
  MOCK_METHOD(void, UntrackPriceForBookmark, (int64_t bookmark_id));
};

class ShoppingListContextMenuControllerTest : public testing::Test {
 public:
  ShoppingListContextMenuControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    shopping_service_ = std::make_unique<MockShoppingService>();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    bookmark_ = AddProductBookmark(bookmark_model_.get(), u"product 1",
                                   GURL("http://example.com/1"), 123L, true,
                                   1230000, "usd");
    handler_ = std::make_unique<MockShoppingServiceHandler>(
        bookmark_model_.get(), shopping_service_.get());
    controller_ = std::make_unique<commerce::ShoppingListContextMenuController>(
        bookmark_model_.get(), shopping_service_.get(), handler_.get());
  }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

  const bookmarks::BookmarkNode* bookmark_node() {
    return bookmarks::GetBookmarkNodeByID(bookmark_model_.get(),
                                          bookmark_->id());
  }

  MockShoppingService* shopping_service() { return shopping_service_.get(); }

  commerce::ShoppingListContextMenuController* controller() {
    return controller_.get();
  }

  ui::SimpleMenuModel* menu_mode() { return menu_model_.get(); }

  MockShoppingServiceHandler* handler() { return handler_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::UserActionTester user_action_tester_;
  raw_ptr<const bookmarks::BookmarkNode, DanglingUntriaged> bookmark_;

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingListContextMenuController> controller_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<MockShoppingServiceHandler> handler_;
};

TEST_F(ShoppingListContextMenuControllerTest, AddMenuItem) {
  shopping_service()->SetIsSubscribedCallbackValue(true);

  // Make sure the subscription is checked against the shopping service. It
  // should be checked twice -- once each for subscribe and unsubscribe.
  EXPECT_CALL(*shopping_service(),
              IsSubscribedFromCache(SubscriptionWithId("123")))
      .Times(2);

  controller()->AddPriceTrackingItemForBookmark(menu_mode(), bookmark_);
  ASSERT_EQ(menu_mode()->GetItemCount(), 1UL);
  ASSERT_EQ(menu_mode()->GetCommandIdAt(0),
            IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK);
  ASSERT_EQ(menu_mode()->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_SIDE_PANEL_UNTRACK_BUTTON));
  menu_mode()->Clear();

  shopping_service()->SetIsSubscribedCallbackValue(false);

  controller()->AddPriceTrackingItemForBookmark(menu_mode(), bookmark_);
  ASSERT_EQ(menu_mode()->GetItemCount(), 1UL);
  ASSERT_EQ(menu_mode()->GetCommandIdAt(0),
            IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK);
  ASSERT_EQ(menu_mode()->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TRACK_BUTTON));
}

TEST_F(ShoppingListContextMenuControllerTest, ExecuteMenuCommand) {
  EXPECT_CALL(*handler(), UntrackPriceForBookmark(bookmark_->id()));
  ASSERT_TRUE(controller()->ExecuteCommand(
      IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK, bookmark_));

  EXPECT_CALL(*handler(), TrackPriceForBookmark(bookmark_->id()));
  ASSERT_TRUE(controller()->ExecuteCommand(
      IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK, bookmark_));
}

}  // namespace
}  // namespace commerce
