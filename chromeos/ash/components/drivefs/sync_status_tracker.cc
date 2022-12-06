// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace drivefs {

SyncStatusTracker::SyncStatusTracker()
    : root_(std::make_unique<TrieNode>(SyncStatus::kNotFound,
                                       /*path_path=*/"",
                                       /*parent=*/nullptr)) {}
SyncStatusTracker::~SyncStatusTracker() = default;

// TODO(msalomao): add count of kError and kInProgress descendant nodes to each
// node and update them whenever the trie changes to avoid a recursive lookup
// on query.
void SyncStatusTracker::AddSyncStatusForPath(const int64_t id,
                                             const base::FilePath& path,
                                             SyncStatus status,
                                             float progress) {
  if (path.empty() || !path.IsAbsolute()) {
    return;
  }
  std::vector<base::FilePath::StringType> path_parts = path.GetComponents();
  TrieNode* current_node = root_.get();
  for (const auto& path_part : path_parts) {
    std::unique_ptr<TrieNode>& matching_node =
        current_node->children[path_part];
    if (!matching_node) {
      matching_node = std::make_unique<TrieNode>(SyncStatus::kNotFound,
                                                 path_part, current_node);
    }
    current_node = matching_node.get();
  }
  current_node->status = status;
  current_node->progress = progress;

  // If the entry with the given id has changed its path, this means it has been
  // moved/renamed. Let's delete its old path before proceeding.
  if (auto it = id_to_node_.find(id);
      it != id_to_node_.end() && it->second != current_node) {
    RemoveNode(it->second);
  }
  id_to_node_[id] = current_node;
}

SyncStatusAndProgress SyncStatusTracker::GetSyncStatusForPath(
    const base::FilePath& path) {
  if (path.empty() || !path.IsAbsolute()) {
    return SyncStatusAndProgress::kNotFound;
  }
  std::vector<base::FilePath::StringType> path_parts = path.GetComponents();
  TrieNode* current_node = root_.get();
  for (const auto& path_part : path_parts) {
    auto it = current_node->children.find(path_part);
    if (it == current_node->children.end()) {
      return SyncStatusAndProgress::kNotFound;
    }
    current_node = it->second.get();
  }
  if (current_node->status != SyncStatus::kNotFound) {
    return {current_node->status, current_node->progress};
  }
  auto [status, progress] = SyncStatusAndProgress::kNotFound;
  std::deque<TrieNode*> queue = {current_node};
  while (!queue.empty()) {
    TrieNode* node = queue.front();
    queue.pop_front();
    if (node->status == SyncStatus::kError) {
      return SyncStatusAndProgress::kError;
    }
    if (node->status > status) {
      status = node->status;
    }
    // TODO(b/256931969): Optimize SyncStatusTracker to make reads O(1).
    for (const auto& child : node->children) {
      queue.emplace_back(child.second.get());
    }
  }
  return {status, progress};
}

void SyncStatusTracker::RemovePath(const int64_t id,
                                   const base::FilePath& path) {
  if (path.empty() || !path.IsAbsolute()) {
    return;
  }
  std::vector<base::FilePath::StringType> path_parts = path.GetComponents();
  TrieNode* current_node = root_.get();
  std::vector<std::pair<TrieNode*, TrieNode::PathToChildMap::iterator>>
      ancestors;
  for (const auto& path_part : path_parts) {
    auto it = current_node->children.find(path_part);
    if (it == current_node->children.end()) {
      return;
    }
    ancestors.emplace_back(current_node, it);
    current_node = it->second.get();
  }
  if (current_node->children.size() > 0) {
    return;
  }
  id_to_node_.erase(id);
  while (!ancestors.empty()) {
    auto& ancestor = ancestors.back();
    auto* node = ancestor.first;
    auto& it = ancestor.second;
    auto& child = it->second;
    if (!child->children.empty()) {
      break;
    }
    node->children.erase(it);
    ancestors.pop_back();
  }
}

void SyncStatusTracker::RemoveNode(const TrieNode* node) {
  if (!node) {
    return;
  }
  auto* parent = node->parent;
  if (!parent) {
    return;
  }
  parent->children.erase(node->path_part);
  auto* grandparent = parent->parent;
  while (grandparent && parent->children.empty()) {
    grandparent->children.erase(parent->path_part);
    parent = grandparent;
    grandparent = grandparent->parent;
  }
}

SyncStatusTracker::TrieNode::TrieNode(SyncStatus status,
                                      base::FilePath::StringType path_part,
                                      TrieNode* parent)
    : status(status), path_part(path_part), parent(parent) {}
SyncStatusTracker::TrieNode::~TrieNode() = default;

}  // namespace drivefs
