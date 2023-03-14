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
    const bookmarks::BookmarkNode* bookmark_node,
    ui::SimpleMenuModel* menu_model)
    : bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      bookmark_node_(bookmark_node),
      menu_model_(menu_model) {}

void ShoppingListContextMenuController::AddPriceTrackingItemForBookmark() {
  if (commerce::IsBookmarkPriceTrackedFromCache(
          shopping_service_, bookmark_model_, bookmark_node_)) {
    menu_model_->AddItem(
        IDC_BOOKMARK_BAR_UNTRACK_PRICE_FOR_SHOPPING_BOOKMARK,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_UNTRACK_BUTTON));
  } else {
    menu_model_->AddItem(
        IDC_BOOKMARK_BAR_TRACK_PRICE_FOR_SHOPPING_BOOKMARK,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TRACK_BUTTON));
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
