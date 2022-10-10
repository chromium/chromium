// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_list_context_menu_controller.h"

#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace commerce {

ShoppingListContextMenuController::ShoppingListContextMenuController(
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    const bookmarks::BookmarkNode* bookmark_node,
    ui::SimpleMenuModel* menu_model)
    : bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      bookmark_node_(bookmark_node),
      menu_model_(menu_model) {}

void ShoppingListContextMenuController::AddPriceTrackingItemForBookmark() {
  if (commerce::IsBookmarkPriceTracked(bookmark_model_, bookmark_node_)) {
    menu_model_->AddItem(
        IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK,
        l10n_util::GetStringUTF16(IDS_BOOKMARKS_MENU_UNTRACK_PRICE));
  } else {
    menu_model_->AddItem(
        IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK,
        l10n_util::GetStringUTF16(IDS_BOOKMARKS_MENU_TRACK_PRICE));
  }
}

bool ShoppingListContextMenuController::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK:
      commerce::SetPriceTrackingStateForBookmark(
          shopping_service_, bookmark_model_, bookmark_node_, true,
          base::DoNothing());
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.SidePanel.Track.ContextMenu"));
      return true;
    case IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK:
      commerce::SetPriceTrackingStateForBookmark(
          shopping_service_, bookmark_model_, bookmark_node_, false,
          base::DoNothing());
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.SidePanel.Untrack.ContextMenu"));
      return true;
    default:
      return false;
  }
}
}  // namespace commerce
