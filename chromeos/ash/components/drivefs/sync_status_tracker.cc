// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include <algorithm>
#include <deque>
#include <utility>
#include <vector>

namespace drivefs {

SyncStatusTracker::SyncStatusTracker()
    : root_(std::make_unique<TrieNode>(SyncStatus::kNotFound)) {}
SyncStatusTracker::~SyncStatusTracker() = default;

// TODO(msalomao): add count of kError and kInProgress descendant nodes to each
// node and update them whenever the trie changes to avoid a recursive lookup
// on query.
void SyncStatusTracker::AddSyncStatusForPath(const base::FilePath& path,
                                             SyncStatus status) {
  if (path.empty() || !path.IsAbsolute()) {
    return;
  }
  std::vector<base::FilePath::StringType> path_parts = path.GetComponents();
  TrieNode* current_node = root_.get();
  for (const auto& path_part : path_parts) {
    std::unique_ptr<TrieNode>& matchingNode = current_node->children[path_part];
    if (matchingNode == nullptr) {
      matchingNode = std::make_unique<TrieNode>(SyncStatus::kNotFound);
    }
    current_node = matchingNode.get();
  }
  current_node->status = status;
}

SyncStatus SyncStatusTracker::GetSyncStatusForPath(const base::FilePath& path) {
  if (path.empty() || !path.IsAbsolute()) {
    return SyncStatus::kNotFound;
  }
  std::vector<base::FilePath::StringType> path_parts = path.GetComponents();
  TrieNode* current_node = root_.get();
  for (const auto& path_part : path_parts) {
    auto it = current_node->children.find(path_part);
    if (it == current_node->children.end()) {
      return SyncStatus::kNotFound;
    }
    current_node = it->second.get();
  }
  if (current_node->status != SyncStatus::kNotFound) {
    return current_node->status;
  }
  SyncStatus status = SyncStatus::kNotFound;
  std::deque<TrieNode*> queue = {current_node};
  while (!queue.empty()) {
    TrieNode* node = queue.front();
    queue.pop_front();
    if (node->status == SyncStatus::kError) {
      return SyncStatus::kError;
    }
    if (node->status > status) {
      status = node->status;
    }
    for (const auto& child : node->children) {
      queue.emplace_back(child.second.get());
    }
  }
  return status;
}

void SyncStatusTracker::RemovePath(const base::FilePath& path) {
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
  while (!ancestors.empty()) {
    auto& ancestor = ancestors.back();
    auto* node = ancestor.first;
    auto& it = ancestor.second;
    auto& child = it->second;
    if (child->children.empty()) {
      node->children.erase(it);
    } else {
      return;
    }
    ancestors.pop_back();
  }
}

SyncStatusTracker::TrieNode::TrieNode(SyncStatus status) : status(status) {}
SyncStatusTracker::TrieNode::~TrieNode() = default;

}  // namespace drivefs
