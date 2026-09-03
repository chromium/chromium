// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_model_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_injector.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace {

std::vector<const bookmarks::BookmarkNode*> ResolveTargetNodes(
    BookmarkBarModelAdapter* model_adapter,
    const bookmarks::BookmarkNodeId& target) {
  if (std::holds_alternative<int64_t>(target)) {
    const bookmarks::BookmarkNode* const node =
        model_adapter->GetNodeById(std::get<int64_t>(target));
    return node ? std::vector{node}
                : std::vector<const bookmarks::BookmarkNode*>{};
  }
  return model_adapter->GetUnderlyingNodes(target);
}

BookmarkParentFolder GetParentFolderForNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes) {
  CHECK(!nodes.empty());
  const bookmarks::BookmarkNode* parent =
      (nodes[0]->is_permanent_node() ||
       (nodes.size() == 1 && nodes[0]->is_folder()))
          ? nodes[0]
          : nodes[0]->parent();
  CHECK(parent && !parent->is_root());
  return BookmarkParentFolder::FromFolderNode(parent);
}

}  // namespace

BookmarkBarUIControllerImpl::BookmarkBarUIControllerImpl(
    std::unique_ptr<BookmarkBarUIControllerInjector> injector)
    : injector_(std::move(injector)) {}

BookmarkBarUIControllerImpl::~BookmarkBarUIControllerImpl() = default;

void BookmarkBarUIControllerImpl::Bind(BookmarkBarUIClient* client) {
  CHECK(!client_) << "BookmarkBarUIController is already bound to a client.";
  client_ = client;

  BookmarkBarPrefsAdapter* prefs_adapter = injector_->GetPrefsAdapter();
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnAppsPageShortcutVisibilityPrefChanged,
          base::Unretained(this)));
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnTabGroupsVisibilityPrefChanged,
          base::Unretained(this)));
  prefs_adapter->AddObserver(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarUIControllerImpl::OnShowManagedBookmarksPrefChanged,
          base::Unretained(this)));

  // Push initial states.
  OnAppsPageShortcutVisibilityPrefChanged();
  OnTabGroupsVisibilityPrefChanged();
  OnShowManagedBookmarksPrefChanged();
}

void BookmarkBarUIControllerImpl::OnAppsPageShortcutVisibilityPrefChanged() {
  if (client_) {
    client_->SetAppsPageShortcutVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  }
}

void BookmarkBarUIControllerImpl::OnTabGroupsVisibilityPrefChanged() {
  if (client_) {
    client_->SetSavedTabGroupsVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowTabGroupsInBookmarkBar));
  }
}

void BookmarkBarUIControllerImpl::OnShowManagedBookmarksPrefChanged() {
  if (client_) {
    client_->SetManagedBookmarksFolderVisibility(
        injector_->GetPrefsAdapter()->GetBoolean(
            bookmarks::prefs::kShowManagedBookmarksInBookmarkBar));
  }
}

void BookmarkBarUIControllerImpl::OpenAppsPage(
    WindowOpenDisposition disposition) {
  injector_->GetActionAdapter()->OpenAppsPage(disposition);
}

void BookmarkBarUIControllerImpl::OpenBookmark(
    int64_t node_id,
    WindowOpenDisposition disposition) {
  BookmarkBarModelAdapter* const model_adapter = injector_->GetModelAdapter();
  const bookmarks::BookmarkNode* const node =
      model_adapter->GetNodeById(node_id);
  if (!node) {
    return;
  }
  injector_->GetActionAdapter()->OpenBookmark(node, disposition);
}

void BookmarkBarUIControllerImpl::OpenFolder(
    const bookmarks::BookmarkNodeId& folder,
    WindowOpenDisposition disposition) {
  // Clicking the middle mouse button or clicking with Control/Command key down
  // opens all bookmarks in the folder in new tabs.
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    injector_->GetActionAdapter()->NotifyFolderOpened();
    if (client_) {
      client_->ShowFolderMenu(folder);
    }
  } else {
    BookmarkBarModelAdapter* const model_adapter = injector_->GetModelAdapter();
    std::vector<const bookmarks::BookmarkNode*> nodes =
        model_adapter->GetUnderlyingNodes(folder);
    if (nodes.empty()) {
      return;
    }
    injector_->GetActionAdapter()->OpenFolderNodes(nodes, disposition);
  }
}

void BookmarkBarUIControllerImpl::ShowContextMenu(
    const bookmarks::BookmarkNodeId& target,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    base::OnceClosure on_close) {
  base::ScopedClosureRunner auto_close(std::move(on_close));
  BookmarkBarModelAdapter* const model_adapter = injector_->GetModelAdapter();

  std::vector<const bookmarks::BookmarkNode*> nodes =
      ResolveTargetNodes(model_adapter, target);
  if (nodes.empty()) {
    return;
  }

  BookmarkParentFolder parent_folder = GetParentFolderForNodes(nodes);

  model_adapter->CanPasteFromClipboard(
      &parent_folder,
      base::BindOnce(&BookmarkBarUIControllerImpl::OnPasteCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(nodes), point,
                     source_type, auto_close.Release()));
}

void BookmarkBarUIControllerImpl::OnPasteCheckComplete(
    std::vector<const bookmarks::BookmarkNode*> selection,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    base::OnceClosure on_close,
    bool can_paste) {
  injector_->GetActionAdapter()->ShowContextMenu(
      selection, point, source_type, can_paste, std::move(on_close));
}
