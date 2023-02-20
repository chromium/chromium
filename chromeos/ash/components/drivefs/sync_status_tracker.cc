// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"

#include <cstdint>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"

namespace drivefs {

constexpr auto kNotFound = SyncStatus::kNotFound;
constexpr auto kMoved = SyncStatus::kMoved;
constexpr auto kCompleted = SyncStatus::kCompleted;
constexpr auto kQueued = SyncStatus::kQueued;
constexpr auto kInProgress = SyncStatus::kInProgress;
constexpr auto kError = SyncStatus::kError;

std::ostream& operator<<(std::ostream& os, const SyncStatus& status) {
  switch (status) {
    case kNotFound:
      return os << "not_found";
    case kMoved:
      return os << "moved";
    case kCompleted:
      return os << "completed";
    case kQueued:
      return os << "queued";
    case kInProgress:
      return os << "in_progress";
    case kError:
      return os << "error";
    default:
      return os << "unknown";
  }
}

SyncStatusTracker::SyncStatusTracker() = default;
SyncStatusTracker::~SyncStatusTracker() = default;

SyncState SyncStatusTracker::GetSyncState(const base::FilePath path) const {
  if (path.empty() || !path.IsAbsolute()) {
    return SyncState::CreateNotFound(path);
  }

  const Node* node = FindNode(path);

  return node ? GetNodeState(node, std::move(path))
              : SyncState::CreateNotFound(std::move(path));
}

const std::vector<const SyncState> SyncStatusTracker::GetChangesAndClean() {
  std::vector<const SyncState> updated_sync_states;

  // Traverse trie.
  std::vector<Node*> stack = {root_.get()};
  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();

    for (auto& child : node->children) {
      stack.emplace_back(child.second.get());
    }

    // Collect dirty nodes and flip them back to pristine.
    if (node->state.IsDirty()) {
      updated_sync_states.emplace_back(GetNodeState(node));
      node->state.MarkAsPristine();
    }

    if (ShouldRemoveNode(node)) {
      RemoveNode(node);
    }
  }

  // Reset root node if it's childless.
  if (root_->children.empty()) {
    root_->state.Set(kNotFound, 0, 0);
  }

  return updated_sync_states;
}

SyncStatusTracker::Node* SyncStatusTracker::FindNode(
    const base::FilePath& path) const {
  const auto components = path.GetComponents();
  DCHECK(!components.empty() && components.front() == "/");
  const base::span<const base::FilePath::StringType> path_parts(
      components.begin() + 1, components.end());

  Node* node = root_.get();
  for (const auto& path_part : path_parts) {
    auto it = node->children.find(path_part);
    if (it == node->children.end()) {
      return nullptr;
    }
    node = it->second.get();
  }
  return node;
}

void SyncStatusTracker::SetSyncState(const int64_t id,
                                     const base::FilePath& path,
                                     const SyncStatus status,
                                     const int64_t transferred,
                                     const int64_t total) {
  if (path.empty() || !path.IsAbsolute()) {
    return;
  }

  const auto components = path.GetComponents();
  DCHECK(!components.empty() && components.front() == "/");
  const base::span<const base::FilePath::StringType> path_parts(
      components.begin() + 1, components.end());

  Node* node = root_.get();
  for (const auto& path_part : path_parts) {
    std::unique_ptr<Node>& matching_node = node->children[path_part];
    if (!matching_node) {
      matching_node = std::make_unique<Node>();
      matching_node->path_part = path_part;
      matching_node->parent = node;
      matching_node->id = id;
    }
    node = matching_node.get();
  }
  SetNodeState(node, status, transferred, total);

  // If the entry with the given id has changed its path, this means it has been
  // moved/renamed. Mark it as "moved" and remove its status/progress changes
  // from its current ancestors so they are not duplicated with its new
  // ancestors in the trie.
  if (auto it = id_to_node_.find(id);
      it != id_to_node_.end() && it->second != node) {
    SetNodeState(it->second, kMoved, 0, 0);
  }
  id_to_node_[id] = node;
}

bool SyncStatusTracker::ShouldRemoveNode(const Node* node) const {
  DCHECK(node);
  const auto status = node->state.GetStatus();
  return node->children.empty() &&
         (status == SyncStatus::kCompleted || status == SyncStatus::kMoved);
}

void SyncStatusTracker::RemoveNode(const Node* node) {
  DCHECK(node);
  Node* parent = node->parent;
  if (!parent) {
    return;
  }
  if (node->id) {
    id_to_node_.erase(node->id);
  }
  parent->children.erase(node->path_part);
  Node* grandparent = parent->parent;
  while (grandparent && parent->children.empty()) {
    grandparent->children.erase(parent->path_part);
    parent = grandparent;
    grandparent = grandparent->parent;
  }
}

const SyncState SyncStatusTracker::GetNodeState(
    const Node* node,
    const base::FilePath path) const {
  const NodeState& state = node->state;
  // Save some computation if the caller already knows the node's path.
  return {state.GetStatus(), state.GetProgress(),
          path.empty() ? GetNodePath(node) : std::move(path)};
}

void SyncStatusTracker::SetNodeState(Node* const node,
                                     const SyncStatus status,
                                     const int64_t transferred = 0,
                                     const int64_t total = 0) {
  const NodeState& delta = node->state.Set(status, transferred, total);

  // Nothing to do if there were no changes.
  if (delta.IsPristine()) {
    return;
  }

  // Update ancestors.
  Node* p = node->parent;
  while (p) {
    p->state.ApplyDelta(delta);
    p = p->parent;
  }
}

const base::FilePath SyncStatusTracker::GetNodePath(const Node* node) const {
  if (!node->parent) {
    return base::FilePath("/");
  }

  std::vector<base::FilePath::StringPieceType> path_parts = {node->path_part};
  Node* p = node->parent;
  while (p) {
    path_parts.emplace_back(p->path_part);
    p = p->parent;
  }
  // Reverse the vector of parts before using it to build the path.
  std::reverse(path_parts.begin(), path_parts.end());
  return base::FilePath(base::JoinString(std::move(path_parts), "/"));
}

SyncStatusTracker::NodeState SyncStatusTracker::NodeState::Set(
    const SyncStatus status,
    int64_t transferred,
    int64_t total) {
  NodeState delta;

  // If the node's status has changed, mark as dirty (both the node and the
  // delta object representing the changes applied to it).
  if (status != GetStatus()) {
    MarkAsDirty();
    delta.MarkAsDirty();
    int32_t old_queued_count = queued_count_;
    int32_t old_in_progress_count = in_progress_count_;
    int32_t old_error_count = error_count_;
    SetStatus(status);
    delta.queued_count_ = queued_count_ - old_queued_count;
    delta.in_progress_count_ = in_progress_count_ - old_in_progress_count;
    delta.error_count_ = error_count_ - old_error_count;
  }

  // This step is required because completed and error status don't carry their
  // actual progress data. Keeping their "total" values fixed ensures their
  // ancestors' aggregate progresses don't suddenly fluctuate up or down.
  if (status == kCompleted) {
    // Increase transferred to 100% of the current total.
    transferred = total_;
    total = total_;
  } else if (status == kError) {
    // Lower transferred to 0% of the current total.
    transferred = 0;
    total = total_;
  }

  delta.transferred_ = transferred - transferred_;
  delta.total_ = total - total_;

  // If the total or transferred number of bytes has changed, mark as dirty
  // (both the node and the delta object representing the changes applied to
  // it).
  if (delta.transferred_ != 0 || delta.total_ != 0) {
    MarkAsDirty();
    delta.MarkAsDirty();
  }

  transferred_ = transferred;
  total_ = total;

  return delta;
}

void SyncStatusTracker::NodeState::ApplyDelta(const NodeState& delta) {
  MarkAsDirty();
  queued_count_ += delta.queued_count_;
  in_progress_count_ += delta.in_progress_count_;
  error_count_ += delta.error_count_;
  transferred_ += delta.transferred_;
  total_ += delta.total_;
}

SyncStatus SyncStatusTracker::NodeState::GetStatus() const {
  if (error_count_ > 0) {
    return kError;
  }
  if (in_progress_count_ > 0) {
    return kInProgress;
  }
  if (queued_count_ > 0) {
    return kQueued;
  }

  // If the node's status isn't "error", "in_progress", or "queued", it will be
  // "completed" if it has transferred some bytes. If it hasn't transferred
  // any bytes but is pristine, it will also be reported as "completed" - this
  // is a special case for the root node: the root node resets to zero progress
  // once all it's children are "completed" but it will be "pristine" at that
  // moment, therefore correctly report as "completed". Finally, if the node's
  // state:
  // * isn't "error", "in_progress", nor "queued",
  // * has transferred no bytes, and
  // * is not dirty,
  // Then it must have been set as dirty because it was detected to be have been
  // "moved" (once nodes are moved, their progress and total resets to 0 and
  // they are marked as dirty);
  return transferred_ || IsPristine() ? kCompleted : kMoved;
}

void SyncStatusTracker::NodeState::SetStatus(const SyncStatus status) {
  switch (status) {
    case kQueued:
      queued_count_ = 1;
      break;
    case kInProgress:
      in_progress_count_ = 1;
      break;
    case kError:
      error_count_ = 1;
      break;
    default:
      queued_count_ = 0;
      in_progress_count_ = 0;
      error_count_ = 0;
      break;
  }
}

SyncStatusTracker::Node::Node() = default;
SyncStatusTracker::Node::~Node() = default;

}  // namespace drivefs
