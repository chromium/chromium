// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_expanded_state_tracker.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

namespace bookmarks {

BookmarkExpandedStateTracker::BookmarkExpandedStateTracker(
    BookmarkModel* bookmark_model,
    PrefService* pref_service)
    : bookmark_model_(bookmark_model),
      pref_service_(pref_service) {
  bookmark_model->AddObserver(this);
}

BookmarkExpandedStateTracker::~BookmarkExpandedStateTracker() {
}

void BookmarkExpandedStateTracker::SetExpandedNodes(const Nodes& nodes) {
  UpdatePrefs(nodes);
}

BookmarkExpandedStateTracker::Nodes
BookmarkExpandedStateTracker::GetExpandedNodes() {
  Nodes nodes;
  if (!bookmark_model_->loaded())
    return nodes;

  if (!pref_service_)
    return nodes;

  const base::ListValue* value =
      pref_service_->GetList(prefs::kBookmarkEditorExpandedNodes);
  if (!value)
    return nodes;

  bool changed = false;
  for (auto i = value->begin(); i != value->end(); ++i) {
    std::string value;
    int64_t node_id;
    const BookmarkNode* node;
    if (i->GetAsString(&value) && base::StringToInt64(value, &node_id) &&
        (node = GetBookmarkNodeByID(bookmark_model_, node_id)) != nullptr &&
        node->is_folder()) {
      nodes.insert(node);
    } else {
      changed = true;
    }
  }
  if (changed)
    UpdatePrefs(nodes);
  return nodes;
}

void BookmarkExpandedStateTracker::BookmarkModelLoaded(BookmarkModel* model,
                                                       bool ids_reassigned) {
  if (ids_reassigned) {
    // If the ids change we can't trust the value in preferences and need to
    // reset it.
    SetExpandedNodes(Nodes());
  }
}

void BookmarkExpandedStateTracker::BookmarkModelChanged() {
}

void BookmarkExpandedStateTracker::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  model->RemoveObserver(this);
}

void BookmarkExpandedStateTracker::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  if (!node->is_folder())
    return;  // Only care about folders.

  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::UpdatePrefs(const Nodes& nodes) {
  if (!pref_service_)
    return;

  std::vector<base::Value> values;
  values.reserve(nodes.size());
  for (const auto* node : nodes) {
    values.emplace_back(base::NumberToString(node->id()));
  }

  pref_service_->Set(prefs::kBookmarkEditorExpandedNodes,
                     base::Value(std::move(values)));
}

}  // namespace bookmarks
