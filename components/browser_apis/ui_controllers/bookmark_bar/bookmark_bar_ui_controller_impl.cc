// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_impl.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/action_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/model_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/platform_adapters_provider.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/preferences_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace bookmark_bar_api {

namespace {

mojo_base::mojom::ErrorPtr MakeError(mojo_base::mojom::Code code,
                                     std::string_view message) {
  return mojo_base::mojom::Error::New(code, std::string(message));
}

// Returns the nodes that `id` names, or an empty vector if it doesn't name a
// valid folder or bookmark. A permanent folder may be made of several nodes
// (e.g. local and account).
std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
    const ModelAdapter& model_adapter,
    const mojom::NodeId& id) {
  if (id.is_permanent_folder()) {
    if (id.get_permanent_folder() ==
        bookmarks_api::mojom::PermanentFolderType::kUnknown) {
      return {};
    }
    return model_adapter.GetUnderlyingNodes(id.get_permanent_folder());
  }
  const bookmarks::BookmarkNode* const node =
      model_adapter.GetNodeById(id.get_node_id());
  if (!node || node->is_root() || node->is_permanent_node()) {
    return {};
  }
  return {node};
}

}  // namespace

BookmarkBarUIControllerImpl::BookmarkBarUIControllerImpl(
    std::unique_ptr<PlatformAdaptersProvider> adapters)
    : adapters_(std::move(adapters)) {
  CHECK(adapters_);
  adapters_->preferences_adapter().SetOnChangedCallback(base::BindRepeating(
      &BookmarkBarUIControllerImpl::NotifyDisplayOptionsChanged,
      base::Unretained(this)));
}

BookmarkBarUIControllerImpl::~BookmarkBarUIControllerImpl() {
  adapters_->preferences_adapter().SetOnChangedCallback(
      base::RepeatingClosure());
}

mojom::DisplayOptionsPtr
BookmarkBarUIControllerImpl::GetDisplayOptionsWithoutObservation() {
  const PreferencesAdapter& preferences_adapter =
      adapters_->preferences_adapter();
  auto options = mojom::DisplayOptions::New();
  options->apps_shortcut_enabled = preferences_adapter.IsAppsShortcutEnabled();
  options->tab_groups_enabled = preferences_adapter.IsTabGroupsEnabled();
  options->managed_bookmarks_enabled =
      preferences_adapter.IsManagedBookmarksEnabled();
  return options;
}

void BookmarkBarUIControllerImpl::AddObserver(
    BookmarkBarUIControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkBarUIControllerImpl::RemoveObserver(
    BookmarkBarUIControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkBarUIControllerImpl::Accept(
    mojo::PendingReceiver<mojom::BookmarkBarUIController> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

BookmarkBarUIControllerImpl::GetDisplayOptionsResult
BookmarkBarUIControllerImpl::GetDisplayOptions() {
  auto snapshot = mojom::DisplayOptionsSnapshot::New();
  snapshot->options = GetDisplayOptionsWithoutObservation();

  mojo::AssociatedRemote<mojom::BookmarkBarObserver> stream;
  snapshot->stream = stream.BindNewEndpointAndPassReceiver();
  remote_observers_.Add(std::move(stream));

  return snapshot;
}

BookmarkBarUIControllerImpl::OpenAppsPageResult
BookmarkBarUIControllerImpl::OpenAppsPage(
    ui::mojom::WindowOpenDisposition disposition) {
  adapters_->action_adapter().OpenAppsPage(disposition);
  return std::monostate();
}

BookmarkBarUIControllerImpl::OpenBookmarkResult
BookmarkBarUIControllerImpl::OpenBookmark(
    int64_t node_id,
    ui::mojom::WindowOpenDisposition disposition) {
  const bookmarks::BookmarkNode* const node =
      adapters_->model_adapter().GetNodeById(node_id);
  if (!node) {
    return base::unexpected(
        MakeError(mojo_base::mojom::Code::kNotFound, "Bookmark not found"));
  }
  adapters_->action_adapter().OpenBookmark(node, disposition);
  return std::monostate();
}

BookmarkBarUIControllerImpl::OpenAllInFolderResult
BookmarkBarUIControllerImpl::OpenAllInFolder(
    mojom::NodeIdPtr folder,
    ui::mojom::WindowOpenDisposition disposition) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GetUnderlyingNodes(adapters_->model_adapter(), *folder);
  if (nodes.empty()) {
    return base::unexpected(
        MakeError(mojo_base::mojom::Code::kNotFound, "Folder not found"));
  }
  adapters_->action_adapter().OpenFolderNodes(nodes, disposition);
  return std::monostate();
}

BookmarkBarUIControllerImpl::RecordFolderOpenedResult
BookmarkBarUIControllerImpl::RecordFolderOpened() {
  adapters_->action_adapter().RecordFolderOpened();
  return std::monostate();
}

BookmarkBarUIControllerImpl::ShowContextMenuResult
BookmarkBarUIControllerImpl::ShowContextMenu(
    mojom::NodeIdPtr target,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  ModelAdapter& model_adapter = adapters_->model_adapter();
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GetUnderlyingNodes(model_adapter, *target);
  if (nodes.empty()) {
    return base::unexpected(
        MakeError(mojo_base::mojom::Code::kNotFound, "Target not found"));
  }

  // The paste check may finish later, so the menu is shown from the callback.
  // The caller doesn't wait for it.
  model_adapter.CanPasteFromClipboard(
      nodes, base::BindOnce(&BookmarkBarUIControllerImpl::OnPasteCheckComplete,
                            weak_ptr_factory_.GetWeakPtr(), std::move(target),
                            point, source_type));
  return std::monostate();
}

void BookmarkBarUIControllerImpl::NotifyDisplayOptionsChanged() {
  mojom::DisplayOptionsPtr options = GetDisplayOptionsWithoutObservation();
  for (BookmarkBarUIControllerObserver& observer : observers_) {
    observer.OnDisplayOptionsChanged(*options);
  }
  for (auto& remote : remote_observers_) {
    remote->OnDisplayOptionsChanged(options.Clone());
  }
}

void BookmarkBarUIControllerImpl::OnPasteCheckComplete(
    mojom::NodeIdPtr target,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    bool can_paste) {
  // The nodes may have been removed while the check ran, so look them up again.
  std::vector<const bookmarks::BookmarkNode*> selection =
      GetUnderlyingNodes(adapters_->model_adapter(), *target);
  if (selection.empty()) {
    NotifyContextMenuClosed();
    return;
  }
  adapters_->action_adapter().ShowContextMenu(
      selection, point, source_type, can_paste,
      base::BindOnce(&BookmarkBarUIControllerImpl::NotifyContextMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BookmarkBarUIControllerImpl::NotifyContextMenuClosed() {
  for (BookmarkBarUIControllerObserver& observer : observers_) {
    observer.OnContextMenuClosed();
  }
  for (auto& remote : remote_observers_) {
    remote->OnContextMenuClosed();
  }
}

}  // namespace bookmark_bar_api
