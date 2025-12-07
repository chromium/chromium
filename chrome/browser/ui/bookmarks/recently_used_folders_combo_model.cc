// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"

#include <stddef.h>

#include <algorithm>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

struct RecentlyUsedFoldersComboModel::Item {
  enum Type {
    TYPE_NODE,
    TYPE_ALL_BOOKMARKS_NODE,
    TYPE_SEPARATOR,
    TYPE_ACCOUNT_BOOKMARK_HEADING,
    TYPE_DEVICE_BOOKMARK_HEADING,
    TYPE_CHOOSE_ANOTHER_FOLDER
  };

  Item(const BookmarkNode* node, Type type);
  ~Item();

  bool operator==(const Item& item) const;

  raw_ptr<const BookmarkNode> node;
  Type type;
};

RecentlyUsedFoldersComboModel::Item::Item(const BookmarkNode* node, Type type)
    : node(node), type(type) {}

RecentlyUsedFoldersComboModel::Item::~Item() = default;

bool RecentlyUsedFoldersComboModel::Item::operator==(const Item& item) const {
  return item.node == node && item.type == type;
}

RecentlyUsedFoldersComboModel::RecentlyUsedFoldersComboModel(
    BookmarkModel* model,
    const BookmarkNode* node)
    : bookmark_model_(model), parent_node_(node->parent()) {
  bookmark_model_->AddObserver(this);

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model, node);

  if (!mru_bookmarks.account_nodes.empty()) {
    const bool show_labels = !mru_bookmarks.local_nodes.empty();

    // Add account nodes to `items_`.
    if (show_labels) {
      items_.emplace_back(nullptr, Item::TYPE_ACCOUNT_BOOKMARK_HEADING);
    }

    for (const BookmarkNode* mru_node : mru_bookmarks.account_nodes) {
      items_.emplace_back(mru_node, mru_node == model->account_other_node()
                                        ? Item::TYPE_ALL_BOOKMARKS_NODE
                                        : Item::TYPE_NODE);
    }

    // Add local nodes to `items_`.
    if (show_labels) {
      items_.emplace_back(nullptr, Item::TYPE_DEVICE_BOOKMARK_HEADING);
    }

    for (const BookmarkNode* mru_node : mru_bookmarks.local_nodes) {
      items_.emplace_back(mru_node, mru_node == model->other_node()
                                        ? Item::TYPE_ALL_BOOKMARKS_NODE
                                        : Item::TYPE_NODE);
    }
  } else {
    for (const BookmarkNode* mru_node : mru_bookmarks.local_nodes) {
      items_.emplace_back(mru_node, Item::TYPE_NODE);
    }
  }

  // Add a separator + choose another folder last regardless of account/local.
  items_.emplace_back(nullptr, Item::TYPE_SEPARATOR);
  items_.emplace_back(nullptr, Item::TYPE_CHOOSE_ANOTHER_FOLDER);
}

RecentlyUsedFoldersComboModel::~RecentlyUsedFoldersComboModel() {
  bookmark_model_->RemoveObserver(this);
}

size_t RecentlyUsedFoldersComboModel::GetItemCount() const {
  return items_.size();
}

std::u16string RecentlyUsedFoldersComboModel::GetItemAt(size_t index) const {
  switch (items_[index].type) {
    case Item::TYPE_NODE:
      return items_[index].node->GetTitle();
    case Item::TYPE_ACCOUNT_BOOKMARK_HEADING:
      return l10n_util::GetStringUTF16(IDS_BOOKMARKS_ACCOUNT_BOOKMARKS);
    case Item::TYPE_DEVICE_BOOKMARK_HEADING:
      return l10n_util::GetStringUTF16(IDS_BOOKMARKS_DEVICE_BOOKMARKS);
    case Item::TYPE_ALL_BOOKMARKS_NODE:
      return l10n_util::GetStringUTF16(IDS_BOOKMARKS_ALL_BOOKMARKS);
    case Item::TYPE_SEPARATOR:
      // This function should not be called for separators.
      NOTREACHED();
    case Item::TYPE_CHOOSE_ANOTHER_FOLDER:
      return l10n_util::GetStringUTF16(
          IDS_BOOKMARK_BUBBLE_CHOOSER_ANOTHER_FOLDER);
  }
  NOTREACHED();
}

bool RecentlyUsedFoldersComboModel::IsItemSeparatorAt(size_t index) const {
  return items_[index].type == Item::TYPE_SEPARATOR;
}

bool RecentlyUsedFoldersComboModel::IsItemTitleAt(size_t index) const {
  switch (items_[index].type) {
    case Item::TYPE_ACCOUNT_BOOKMARK_HEADING:
    case Item::TYPE_DEVICE_BOOKMARK_HEADING:
      return true;
    case Item::TYPE_NODE:
    case Item::TYPE_SEPARATOR:
    case Item::TYPE_ALL_BOOKMARKS_NODE:
    case Item::TYPE_CHOOSE_ANOTHER_FOLDER:
      return false;
  }
  NOTREACHED();
}

std::optional<size_t> RecentlyUsedFoldersComboModel::GetDefaultIndex() const {
  // TODO(pbos): Ideally we shouldn't have to handle `parent_node_` removal
  // here, the dialog should instead close immediately (and destroy `this`).
  // If that can be resolved, this should DCHECK that it != items_.end() and
  // a DCHECK should be added in the BookmarkModel observer methods to ensure
  // that we don't remove `parent_node_`.
  // TODO(pbos): Look at returning -1 here if there's no default index. Right
  // now a lot of code in Combobox assumes an index within `items_` bounds.
  auto it = std::ranges::find(items_, Item(parent_node_, Item::TYPE_NODE));
  if (it == items_.end()) {
    it = std::ranges::find(items_,
                           Item(parent_node_, Item::TYPE_ALL_BOOKMARKS_NODE));
  }
  return it == items_.end() ? 0 : static_cast<int>(it - items_.begin());
}

std::optional<ui::ColorId>
RecentlyUsedFoldersComboModel::GetDropdownForegroundColorIdAt(
    size_t index) const {
  switch (items_[index].type) {
    case Item::TYPE_ACCOUNT_BOOKMARK_HEADING:
    case Item::TYPE_DEVICE_BOOKMARK_HEADING:
      return ui::kColorDisabledForeground;
    case Item::TYPE_NODE:
    case Item::TYPE_SEPARATOR:
    case Item::TYPE_ALL_BOOKMARKS_NODE:
    case Item::TYPE_CHOOSE_ANOTHER_FOLDER:
      return std::nullopt;
  }
  NOTREACHED();
}

ui::ComboboxModel::ItemCheckmarkConfig
RecentlyUsedFoldersComboModel::GetCheckmarkConfig() const {
  if (base::FeatureList::IsEnabled(
          switches::kSyncEnableBookmarksInTransportMode)) {
    // Explicitly enable checkmarks for all folder entries to visually
    // distinguish them from titles.
    return ItemCheckmarkConfig::kEnabled;
  }
  return ItemCheckmarkConfig::kDefault;
}

void RecentlyUsedFoldersComboModel::BookmarkModelLoaded(bool ids_reassigned) {}

void RecentlyUsedFoldersComboModel::BookmarkModelBeingDeleted() {}

void RecentlyUsedFoldersComboModel::BookmarkNodeMoved(
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeAdded(
    const BookmarkNode* parent,
    size_t index,
    bool added_by_user) {}

void RecentlyUsedFoldersComboModel::OnWillRemoveBookmarks(
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const base::Location& location) {
  // Changing is rare enough that we don't attempt to readjust the contents.
  // Update |items_| so we aren't left pointing to a deleted node.
  bool changed = false;
  for (auto i = items_.begin(); i != items_.end();) {
    if (i->type == Item::TYPE_NODE && i->node->HasAncestor(node)) {
      i = items_.erase(i);
      changed = true;
    } else {
      ++i;
    }
  }
  if (changed) {
    for (ui::ComboboxModelObserver& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

void RecentlyUsedFoldersComboModel::BookmarkNodeRemoved(
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeChanged(
    const BookmarkNode* node) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeFaviconChanged(
    const BookmarkNode* node) {}

void RecentlyUsedFoldersComboModel::BookmarkNodeChildrenReordered(
    const BookmarkNode* node) {}

void RecentlyUsedFoldersComboModel::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // Changing is rare enough that we don't attempt to readjust the contents.
  // Update |items_| so we aren't left pointing to a deleted node.
  bool changed = false;
  for (auto i = items_.begin(); i != items_.end();) {
    if (i->type == Item::TYPE_NODE &&
        !bookmark_model_->is_permanent_node(i->node)) {
      i = items_.erase(i);
      changed = true;
    } else {
      ++i;
    }
  }
  if (changed) {
    for (ui::ComboboxModelObserver& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

void RecentlyUsedFoldersComboModel::MaybeChangeParent(const BookmarkNode* node,
                                                      size_t selected_index) {
  DCHECK_LT(selected_index, items_.size());
  if (items_[selected_index].type != Item::TYPE_NODE &&
      items_[selected_index].type != Item::TYPE_ALL_BOOKMARKS_NODE) {
    return;
  }

  const BookmarkNode* new_parent = GetNodeAt(selected_index);
  if (new_parent != node->parent()) {
    base::RecordAction(base::UserMetricsAction("BookmarkBubble_ChangeParent"));
    bookmark_model_->Move(node, new_parent, new_parent->children().size());
  }
}

const BookmarkNode* RecentlyUsedFoldersComboModel::GetNodeAt(size_t index) {
  return (index < items_.size()) ? items_[index].node.get() : nullptr;
}
