// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace commerce {

class ShoppingService;

class ShoppingListHandler : public shopping_list::mojom::ShoppingListHandler,
                            public bookmarks::BaseBookmarkModelObserver {
 public:
  ShoppingListHandler(
      mojo::PendingRemote<shopping_list::mojom::Page> page,
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
      bookmarks::BookmarkModel* bookmark_model,
      ShoppingService* shopping_service,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      const std::string& locale);
  ShoppingListHandler(const ShoppingListHandler&) = delete;
  ShoppingListHandler& operator=(const ShoppingListHandler&) = delete;
  ~ShoppingListHandler() override;

  // shopping_list::mojom::ShoppingListHandler:
  void GetAllPriceTrackedBookmarkProductInfo(
      GetAllPriceTrackedBookmarkProductInfoCallback callback) override;
  void TrackPriceForBookmark(int64_t bookmark_id) override;
  void UntrackPriceForBookmark(int64_t bookmark_id) override;

  // bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;
  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;

  static std::vector<shopping_list::mojom::BookmarkProductInfoPtr>
  BookmarkListToMojoList(
      bookmarks::BookmarkModel& model,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      const std::string& locale);

 private:
  void onPriceTrackResult(int64_t bookmark_id,
                          bookmarks::BookmarkModel* model,
                          bool success);

  mojo::Remote<shopping_list::mojom::Page> remote_page_;
  mojo::Receiver<shopping_list::mojom::ShoppingListHandler> receiver_;
  // The bookmark model, shopping service and tracker will outlive this
  // implementation since it is a keyed service bound to the browser context
  // (which in turn has the same lifecycle as the browser). The web UI that
  // hosts this will be shut down prior to the rest of the browser.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  const std::string locale_;
  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<ShoppingListHandler> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
