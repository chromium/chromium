// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_api.mojom.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace bookmark_bar_api {

class PlatformAdaptersProvider;

class BookmarkBarUIControllerImpl : public BookmarkBarUIController {
 public:
  explicit BookmarkBarUIControllerImpl(
      std::unique_ptr<PlatformAdaptersProvider> adapters);
  BookmarkBarUIControllerImpl(const BookmarkBarUIControllerImpl&) = delete;
  BookmarkBarUIControllerImpl& operator=(const BookmarkBarUIControllerImpl&) =
      delete;
  ~BookmarkBarUIControllerImpl() override;

  // BookmarkBarUIController:
  mojom::DisplayOptionsPtr GetDisplayOptionsWithoutObservation() override;
  void AddObserver(BookmarkBarUIControllerObserver* observer) override;
  void RemoveObserver(BookmarkBarUIControllerObserver* observer) override;
  void Accept(
      mojo::PendingReceiver<mojom::BookmarkBarUIController> receiver) override;

  // mojom::BookmarkBarUIControllerDirectReturnStub:
  GetDisplayOptionsResult GetDisplayOptions() override;
  OnAppsShortcutPressedResult OnAppsShortcutPressed(
      int32_t event_flags) override;
  OnBookmarkPressedResult OnBookmarkPressed(int64_t node_id,
                                            int32_t event_flags) override;
  OnFolderPressedResult OnFolderPressed(mojom::NodeIdPtr folder,
                                        int32_t event_flags) override;
  ShowContextMenuResult ShowContextMenu(
      mojom::NodeIdPtr target,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

 private:
  void NotifyDisplayOptionsChanged();
  void NotifyShowFolderMenu(const mojom::NodeId& folder);
  void OnPasteCheckComplete(mojom::NodeIdPtr target,
                            const gfx::Point& point,
                            ui::mojom::MenuSourceType source_type,
                            bool can_paste);
  void NotifyContextMenuClosed();

  std::unique_ptr<PlatformAdaptersProvider> adapters_;

  mojom::BookmarkBarUIControllerBridge bridge_{this};
  mojo::ReceiverSet<mojom::BookmarkBarUIController> receivers_;
  mojo::AssociatedRemoteSet<mojom::BookmarkBarObserver> remote_observers_;
  base::ObserverList<BookmarkBarUIControllerObserver> observers_;

  base::WeakPtrFactory<BookmarkBarUIControllerImpl> weak_ptr_factory_{this};
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_BOOKMARK_BAR_UI_CONTROLLER_IMPL_H_
