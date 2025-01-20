// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/price_tracking_handler.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/webui/webui_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "content/public/browser/web_contents.h"

namespace commerce {

PriceTrackingHandler::PriceTrackingHandler(
    mojo::PendingRemote<price_tracking::mojom::Page> remote_page,
    mojo::PendingReceiver<price_tracking::mojom::PriceTrackingHandler> receiver,
    content::WebUI* web_ui,
    ShoppingService* shopping_service,
    feature_engagement::Tracker* tracker,
    bookmarks::BookmarkModel* bookmark_model)
    : remote_page_(std::move(remote_page)),
      receiver_(this, std::move(receiver)),
      web_ui_(web_ui),
      tracker_(tracker),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service) {
  scoped_subscriptions_observation_.Observe(shopping_service_);
  scoped_bookmark_model_observation_.Observe(bookmark_model_);

  // It is safe to schedule updates and observe bookmarks. If the feature is
  // disabled, no new information will be fetched or provided to the frontend.
  shopping_service_->ScheduleSavedProductUpdate();

  if (shopping_service_->GetAccountChecker()) {
    locale_ = shopping_service_->GetAccountChecker()->GetLocale();
  }
}

PriceTrackingHandler::~PriceTrackingHandler() = default;

void PriceTrackingHandler::TrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), true,
      base::BindOnce(&PriceTrackingHandler::OnPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_, true));
}

void PriceTrackingHandler::UntrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), false,
      base::BindOnce(&PriceTrackingHandler::OnPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_, false));
}

void PriceTrackingHandler::SetPriceTrackingStatusForCurrentUrl(bool track) {
  if (track) {
    // If the product on the page isn't already tracked, create a bookmark for
    // it and start tracking.
    TrackPriceForBookmark(GetOrAddBookmarkForCurrentUrl()->id());
    commerce::metrics::RecordShoppingActionUKM(
        GetCurrentTabUkmSourceId(),
        commerce::metrics::ShoppingAction::kPriceTracked);
    return;
  }

  // If the product is already tracked, there must be a bookmark, but it's not
  // necessarily the page the user is currently on (i.e. multi-merchant
  // tracking). Prioritize accessing the product info for the URL before
  // attempting to access the bookmark.
  base::OnceCallback<void(uint64_t)> unsubscribe = base::BindOnce(
      [](base::WeakPtr<PriceTrackingHandler> handler, uint64_t id) {
        if (!handler) {
          return;
        }

        commerce::SetPriceTrackingStateForClusterId(
            handler->shopping_service_, handler->bookmark_model_, id, false,
            base::BindOnce([](bool success) {}));
      },
      weak_ptr_factory_.GetWeakPtr());

  shopping_service_->GetProductInfoForUrl(
      GetCurrentTabUrl().value(),
      base::BindOnce(
          [](base::WeakPtr<PriceTrackingHandler> handler,
             base::OnceCallback<void(uint64_t)> unsubscribe, const GURL& url,
             const std::optional<const ProductInfo>& info) {
            if (!handler) {
              return;
            }

            if (!info.has_value() || !info->product_cluster_id.has_value()) {
              std::optional<uint64_t> cluster_id =
                  GetProductClusterIdFromBookmark(url,
                                                  handler->bookmark_model_);

              if (cluster_id.has_value()) {
                std::move(unsubscribe).Run(cluster_id.value());
              }

              return;
            }

            std::move(unsubscribe).Run(info->product_cluster_id.value());
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(unsubscribe)));
}

void PriceTrackingHandler::GetAllPriceTrackedBookmarkProductInfo(
    GetAllPriceTrackedBookmarkProductInfoCallback callback) {
  shopping_service_->WaitForReady(base::BindOnce(
      [](base::WeakPtr<PriceTrackingHandler> handler,
         GetAllPriceTrackedBookmarkProductInfoCallback callback,
         ShoppingService* service) {
        if (!service || !service->IsShoppingListEligible() ||
            handler.WasInvalidated()) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  std::move(callback),
                  std::vector<shared::mojom::BookmarkProductInfoPtr>()));
          return;
        }

        service->GetAllPriceTrackedBookmarks(
            base::BindOnce(&PriceTrackingHandler::OnFetchPriceTrackedBookmarks,
                           handler, std::move(callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PriceTrackingHandler::GetAllShoppingBookmarkProductInfo(
    GetAllShoppingBookmarkProductInfoCallback callback) {
  shopping_service_->WaitForReady(base::BindOnce(
      [](base::WeakPtr<PriceTrackingHandler> handler,
         GetAllShoppingBookmarkProductInfoCallback callback,
         ShoppingService* service) {
        if (!service || !service->IsShoppingListEligible() ||
            handler.WasInvalidated()) {
          std::move(callback).Run({});
          return;
        }

        std::vector<const bookmarks::BookmarkNode*> bookmarks =
            service->GetAllShoppingBookmarks();

        std::vector<shared::mojom::BookmarkProductInfoPtr> info_list =
            handler->BookmarkListToMojoList(*(handler->bookmark_model_),
                                            bookmarks, handler->locale_);

        std::move(callback).Run(std::move(info_list));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PriceTrackingHandler::GetShoppingCollectionBookmarkFolderId(
    GetShoppingCollectionBookmarkFolderIdCallback callback) {
  const bookmarks::BookmarkNode* collection =
      commerce::GetShoppingCollectionBookmarkFolder(bookmark_model_);
  std::move(callback).Run(collection ? collection->id() : -1);
}

void PriceTrackingHandler::GetParentBookmarkFolderNameForCurrentUrl(
    GetParentBookmarkFolderNameForCurrentUrlCallback callback) {
  auto current_url = GetCurrentTabUrl();
  if (current_url.has_value()) {
    std::move(callback).Run(
        commerce::GetBookmarkParentName(bookmark_model_, current_url.value())
            .value_or(std::u16string()));
  } else {
    std::move(callback).Run(std::u16string());
  }
}

void PriceTrackingHandler::ShowBookmarkEditorForCurrentUrl() {
  auto current_url = GetCurrentTabUrl();
  if (!current_url.has_value()) {
    return;
  }

  auto* profile = Profile::FromWebUI(web_ui_);
  auto* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    return;
  }

  const bookmarks::BookmarkNode* existing_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(current_url.value());
  if (!existing_node) {
    return;
  }

  BookmarkEditor::Show(browser->window()->GetNativeWindow(), profile,
                       BookmarkEditor::EditDetails::EditNode(existing_node),
                       BookmarkEditor::SHOW_TREE);
}

std::vector<shared::mojom::BookmarkProductInfoPtr>
PriceTrackingHandler::BookmarkListToMojoList(
    bookmarks::BookmarkModel& model,
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    const std::string& locale) {
  std::vector<shared::mojom::BookmarkProductInfoPtr> info_list;

  for (const bookmarks::BookmarkNode* node : bookmarks) {
    info_list.push_back(BookmarkNodeToMojoProduct(model, node, locale));
  }

  return info_list;
}

void PriceTrackingHandler::BookmarkModelChanged() {}

void PriceTrackingHandler::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  if (!node) {
    return;
  }
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_, node);
  if (!meta || !meta->has_shopping_specifics() ||
      !meta->shopping_specifics().has_product_cluster_id()) {
    return;
  }
  remote_page_->OnProductBookmarkMoved(
      BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_));
}

void PriceTrackingHandler::OnSubscribe(const CommerceSubscription& subscription,
                                       bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscription, true);
  }
}

void PriceTrackingHandler::OnUnsubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscription, false);
  }
}

void PriceTrackingHandler::HandleSubscriptionChange(
    const CommerceSubscription& sub,
    bool is_tracking) {
  if (sub.id_type != IdentifierType::kProductClusterId) {
    return;
  }

  uint64_t cluster_id;
  if (!base::StringToUint64(sub.id, &cluster_id)) {
    return;
  }

  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      GetBookmarksWithClusterId(bookmark_model_, cluster_id);
  // Special handling when the unsubscription is caused by bookmark deletion and
  // therefore the bookmark can no longer be retrieved.
  // TODO(crbug.com/40066977): Update mojo call to pass cluster ID and make
  // BookmarkProductInfo a nullable parameter.
  if (!bookmarks.size()) {
    auto bookmark_info = shared::mojom::BookmarkProductInfo::New();
    bookmark_info->info = shared::mojom::ProductInfo::New();
    bookmark_info->info->cluster_id = cluster_id;
    remote_page_->PriceUntrackedForBookmark(std::move(bookmark_info));
    return;
  }
  for (auto* node : bookmarks) {
    auto product = BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_);
    if (is_tracking) {
      remote_page_->PriceTrackedForBookmark(std::move(product));
    } else {
      remote_page_->PriceUntrackedForBookmark(std::move(product));
    }
  }
}

std::optional<GURL> PriceTrackingHandler::GetCurrentTabUrl() {
  auto* profile = Profile::FromWebUI(web_ui_);
  auto* browser = chrome::FindTabbedBrowser(profile, false);
  if (!browser) {
    return std::nullopt;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::nullopt;
  }

  return std::make_optional<GURL>(web_contents->GetLastCommittedURL());
}

ukm::SourceId PriceTrackingHandler::GetCurrentTabUkmSourceId() {
  auto* browser = chrome::FindTabbedBrowser(Profile::FromWebUI(web_ui_), false);
  if (!browser) {
    return ukm::kInvalidSourceId;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return ukm::kInvalidSourceId;
  }
  return web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

const bookmarks::BookmarkNode*
PriceTrackingHandler::GetOrAddBookmarkForCurrentUrl() {
  auto* browser =
      chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui_));
  if (!browser) {
    return nullptr;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }

  const bookmarks::BookmarkNode* existing_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(
          web_contents->GetLastCommittedURL());
  if (existing_node != nullptr) {
    return existing_node;
  }
  GURL url;
  std::u16string title;
  if (chrome::GetURLAndTitleToBookmark(web_contents, &url, &title)) {
    const bookmarks::BookmarkNode* parent =
        commerce::GetShoppingCollectionBookmarkFolder(bookmark_model_, true);

    return bookmark_model_->AddNewURL(parent, parent->children().size(), title,
                                      url);
  }
  return nullptr;
}

void PriceTrackingHandler::OnPriceTrackResult(int64_t bookmark_id,
                                              bookmarks::BookmarkModel* model,
                                              bool is_tracking,
                                              bool success) {
  if (success) {
    return;
  }

  // We only do work here if price tracking failed. When the UI is interacted
  // with, we assume success. In the event it failed, we switch things back.
  // So in this case, if we were trying to untrack and that action failed, set
  // the UI back to "tracking".
  auto* node = bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
  auto product = BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_);

  if (!is_tracking) {
    remote_page_->PriceTrackedForBookmark(std::move(product));
  } else {
    remote_page_->PriceUntrackedForBookmark(std::move(product));
  }
  // Pass in whether the failed operation was to track or untrack price. It
  // should be the reverse of the current tracking status since the operation
  // failed.
  remote_page_->OperationFailedForBookmark(
      BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_), is_tracking);
}

void PriceTrackingHandler::OnFetchPriceTrackedBookmarks(
    GetAllPriceTrackedBookmarkProductInfoCallback callback,
    std::vector<const bookmarks::BookmarkNode*> bookmarks) {
  std::vector<shared::mojom::BookmarkProductInfoPtr> info_list =
      BookmarkListToMojoList(*bookmark_model_, bookmarks, locale_);

  if (!info_list.empty()) {
    // Record usage for price tracking promo.
    tracker_->NotifyEvent("price_tracking_side_panel_shown");
  }

  std::move(callback).Run(std::move(info_list));
}

}  // namespace commerce
