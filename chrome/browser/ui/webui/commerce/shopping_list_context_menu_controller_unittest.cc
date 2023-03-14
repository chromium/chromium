// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace commerce {
namespace {

class ShoppingListContextMenuControllerTest : public testing::Test {
 public:
  ShoppingListContextMenuControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    shopping_service_ = std::make_unique<MockShoppingService>();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    const bookmarks::BookmarkNode* product = AddProductBookmark(
        bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
        true, 1230000, "usd");
    bookmark_id_ = product->id();
    controller_ = std::make_unique<commerce::ShoppingListContextMenuController>(
        bookmark_model_.get(), shopping_service_.get(), product,
        menu_model_.get());
  }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

  const bookmarks::BookmarkNode* bookmark_node() {
    return bookmarks::GetBookmarkNodeByID(bookmark_model_.get(), bookmark_id_);
  }

  MockShoppingService* shopping_service() { return shopping_service_.get(); }

  commerce::ShoppingListContextMenuController* controller() {
    return controller_.get();
  }

  ui::SimpleMenuModel* menu_mode() { return menu_model_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::UserActionTester user_action_tester_;

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingListContextMenuController> controller_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  int64_t bookmark_id_;
};

TEST_F(ShoppingListContextMenuControllerTest, AddMenuItem) {
  shopping_service()->SetIsSubscribedCallbackValue(true);

  // Make sure the subscription is checked against the shopping service. It
  // should be checked twice -- once each for subscribe and unsubscribe.
  EXPECT_CALL(*shopping_service(),
              IsSubscribedFromCache(SubscriptionWithId("123")))
      .Times(2);

  controller()->AddPriceTrackingItemForBookmark();
  ASSERT_EQ(menu_mode()->GetItemCount(), 1UL);
  ASSERT_EQ(menu_mode()->GetCommandIdAt(0),
            IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK);
  ASSERT_EQ(menu_mode()->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_SIDE_PANEL_UNTRACK_BUTTON));
  menu_mode()->Clear();

  shopping_service()->SetIsSubscribedCallbackValue(false);

  controller()->AddPriceTrackingItemForBookmark();
  ASSERT_EQ(menu_mode()->GetItemCount(), 1UL);
  ASSERT_EQ(menu_mode()->GetCommandIdAt(0),
            IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK);
  ASSERT_EQ(menu_mode()->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TRACK_BUTTON));
}

TEST_F(ShoppingListContextMenuControllerTest, ExecuteMenuCommand) {
  shopping_service()->SetIsSubscribedCallbackValue(true);

  // Make sure unsubscribe is called with the expected cluster ID.
  EXPECT_CALL(*shopping_service(),
              Unsubscribe(VectorHasSubscriptionWithId("123"), testing::_));
  ASSERT_TRUE(controller()->ExecuteCommand(
      IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK));
  task_environment_.RunUntilIdle();

  shopping_service()->SetIsSubscribedCallbackValue(false);

  ASSERT_EQ(0, user_action_tester_.GetActionCount(
                   "Commerce.PriceTracking.SidePanel.Track.ContextMenu"));
  ASSERT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.PriceTracking.SidePanel.Untrack.ContextMenu"));

  // Make sure subscribe is called with the expected cluster ID.
  EXPECT_CALL(*shopping_service(),
              Subscribe(VectorHasSubscriptionWithId("123"), testing::_));
  ASSERT_TRUE(controller()->ExecuteCommand(
      IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK));
  task_environment_.RunUntilIdle();

  shopping_service()->SetIsSubscribedCallbackValue(true);

  ASSERT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.PriceTracking.SidePanel.Track.ContextMenu"));
  ASSERT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.PriceTracking.SidePanel.Untrack.ContextMenu"));

  // Ignore commands that are not price tracking-related.
  ASSERT_FALSE(controller()->ExecuteCommand(IDC_BOOKMARK_BAR_OPEN_ALL));
}

}  // namespace
}  // namespace commerce
