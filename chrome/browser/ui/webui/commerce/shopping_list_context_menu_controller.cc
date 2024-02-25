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
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace commerce {

namespace {
// Check if a bookmark is cached using the cached subscription data from the
// shopping service.
bool IsBookmarkPriceTrackedFromCache(ShoppingService* service,
                                     bookmarks::BookmarkModel* model,
                                     const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics() ||
      !meta->shopping_specifics().has_product_cluster_id()) {
    return false;
  }

  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(meta->shopping_specifics().product_cluster_id()),
      ManagementType::kUserManaged);

  return service->IsSubscribedFromCache(sub);
}
}  // namespace

ShoppingListContextMenuController::ShoppingListContextMenuController(
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    ShoppingServiceHandler* shopping_list_hander)
    : bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      shopping_list_hander_(shopping_list_hander) {}

void ShoppingListContextMenuController::AddPriceTrackingItemForBookmark(
    ui::SimpleMenuModel* menu_model,
    const bookmarks::BookmarkNode* bookmark_node) {
  if (commerce::IsBookmarkPriceTrackedFromCache(
          shopping_service_, bookmark_model_, bookmark_node)) {
    menu_model->AddItem(
        IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_UNTRACK_BUTTON));
  } else {
    menu_model->AddItem(IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK,
                        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TRACK_BUTTON));
  }
}

bool ShoppingListContextMenuController::ExecuteCommand(
    int command_id,
    const bookmarks::BookmarkNode* bookmark_node) {
  switch (command_id) {
    // Use APIs from ShoppingServiceHandler for price tracking and untracking
    // because these APIs already have subscription error handling so we don't
    // need to handle it here.
    case IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK:
      shopping_list_hander_->TrackPriceForBookmark(bookmark_node->id());
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.SidePanel.Track.ContextMenu"));
      return true;
    case IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK:
      shopping_list_hander_->UntrackPriceForBookmark(bookmark_node->id());
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.SidePanel.Untrack.ContextMenu"));
      return true;
    default:
      return false;
  }
}
}  // namespace commerce
