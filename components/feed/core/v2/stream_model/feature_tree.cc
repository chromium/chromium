// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model/feature_tree.h"

#include <algorithm>
#include <sstream>

#include "base/check.h"

namespace feed {
namespace stream_model {
namespace {
std::string ToAsciiForTesting(const std::string& s) {
  std::string result = s;
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] < 32 || result[i] > 126) {
      result[i] = '?';
    }
  }
  return result;
}
}  // namespace

ContentMap::ContentMap(ContentRevision::Generator* revision_generator)
    : revision_generator_(revision_generator) {}

ContentMap::~ContentMap() = default;

ContentTag ContentMap::GetContentTag(const feedwire::ContentId& id) {
  auto iter = mapping_.find(id);
  if (iter != mapping_.end())
    return iter->second;
  ContentTag tag = tag_generator_.GenerateNextId();
  mapping_[id] = tag;
  return tag;
}

const feedstore::Content* ContentMap::FindContent(
    ContentRevision content_revision) {
  const size_t index = content_revision.GetUnsafeValue();
  if (revision_to_content_.size() <= index) {
    return nullptr;
  }
  return revision_to_content_[index];
}

ContentRevision ContentMap::LookupContentRevision(
    const feedstore::Content& content) {
  auto iter = content_.find(content);
  return (iter != content_.end()) ? iter->second : ContentRevision();
}

ContentRevision ContentMap::AddContent(feedstore::Content content) {
  auto result = content_.emplace(std::move(content), ContentRevision());
  // Already exists
  if (!result.second)
    return result.first->second;

  // Newly inserted.
  const ContentRevision new_revision = revision_generator_->GenerateNextId();
  result.first->second = new_revision;
  if (revision_to_content_.size() <= new_revision.GetUnsafeValue()) {
    revision_to_content_.resize(new_revision.GetUnsafeValue() + 1);
  }
  revision_to_content_[new_revision.GetUnsafeValue()] = &result.first->first;
  return new_revision;
}

void ContentMap::Clear() {
  // We don't clear the ID generators, so no IDs are re-used.
  mapping_.clear();
  content_.clear();
  revision_to_content_.clear();
}

StreamNode::StreamNode() = default;
StreamNode::~StreamNode() = default;
StreamNode::StreamNode(const StreamNode&) = default;
StreamNode& StreamNode::operator=(const StreamNode&) = default;

FeatureTree::FeatureTree(ContentMap* content_map) : content_map_(content_map) {}

FeatureTree::FeatureTree(const FeatureTree* base)
    : base_(base),
      content_map_(base->content_map_),
      computed_root_(base->computed_root_),
      root_tag_(base->root_tag_),
      nodes_(base->nodes_) {}
FeatureTree::~FeatureTree() = default;

StreamNode* FeatureTree::GetOrMakeNode(ContentTag id) {
  ResizeNodesIfNeeded(id);
  return &nodes_[id.value()];
}

const StreamNode* FeatureTree::FindNode(ContentTag id) const {
  return const_cast<FeatureTree*>(this)->FindNode(id);
}

StreamNode* FeatureTree::FindNode(ContentTag id) {
  if (!id.is_null() && nodes_.size() > id.value())
    return &nodes_[id.value()];
  return nullptr;
}

const feedstore::Content* FeatureTree::FindContent(ContentRevision id) const {
  return content_map_->FindContent(id);
}

void FeatureTree::ApplyStreamStructure(
    const feedstore::StreamStructure& structure) {
  switch (structure.operation()) {
    case feedstore::StreamStructure::CLEAR_ALL:
      nodes_.clear();
      // Clearing content is not required for correctness, but we can do it to
      // free memory as long as there's no base feature tree that can reference
      // the content.
      if (!base_)
        content_map_->Clear();
      computed_root_ = false;
      break;
    case feedstore::StreamStructure::UPDATE_OR_APPEND: {
      const ContentTag child_id = GetContentTag(structure.content_id());
      const bool is_root = structure.is_root();
      ContentTag parent_id;
      if (structure.has_parent_id()) {
        parent_id = GetContentTag(structure.parent_id());
      }
      ResizeNodesIfNeeded(std::max(child_id, parent_id));
      StreamNode& child = nodes_[child_id.value()];
      StreamNode* parent = FindNode(parent_id);

      // If a node already has a parent, treat this as an update, not an append
      // operation.
      child.tombstoned = false;
      if (root_tag_ == child_id) {
        computed_root_ = false;
      }

      if (parent && !child.has_parent) {
        // The child doesn't yet have a parent, but it should. Link to the
        // parent now. If the child already has a parent, we will never change
        // the parent even if requested by UPDATE_OR_APPEND.
        child.has_parent = true;
        child.previous_sibling = parent->last_child;
        parent->last_child = child_id;
      } else if (is_root || ((!computed_root_ || root_tag_.is_null()) &&
                             !parent && structure.parent_id().id() == 0 &&
                             structure.parent_id().type() == 0)) {
        // For recently produced stream data, there should be a node with
        // 'is_root' set. However, for older cached stream data, we weren't
        // storing this information. In that case, we fallback to pick the first
        // node which doesn't have a parentID as root.
        // TODO(crbug.com/40755948): simplify this once we can depend on
        // receiving is_root.
        computed_root_ = true;
        root_tag_ = child_id;
      }
    } break;
    case feedstore::StreamStructure::REMOVE: {
      // Removal is just unlinking the node from the tree.
      // If it's added back again later, it retains its old children.
      ContentTag tag = GetContentTag(structure.content_id());
      if (root_tag_ == tag) {
        computed_root_ = false;
      }
      GetOrMakeNode(tag)->tombstoned = true;
    } break;
    default:
      break;
  }
}  // namespace stream_model

void FeatureTree::ResizeNodesIfNeeded(ContentTag id) {
  if (nodes_.size() <= id.value())
    nodes_.resize(id.value() + 1);
}

void FeatureTree::AddContent(feedstore::Content content) {
  const ContentTag tag = GetContentTag(content.content_id());
  const ContentRevision revision_id =
      content_map_->AddContent(std::move(content));
  GetOrMakeNode(tag)->content_revision = revision_id;
}

void FeatureTree::CopyAndAddContent(const feedstore::Content& content) {
  ContentRevision revision_id = content_map_->LookupContentRevision(content);
  if (revision_id.is_null()) {
    revision_id = content_map_->AddContent(content);
  }
  const ContentTag tag = GetContentTag(content.content_id());
  GetOrMakeNode(tag)->content_revision = revision_id;
}

void FeatureTree::ResolveRoot() {
  if (computed_root_) {
    DCHECK(!FindNode(root_tag_) || !FindNode(root_tag_)->tombstoned);
    DCHECK(!FindNode(root_tag_) || !FindNode(root_tag_)->has_parent);
    return;
  }
  root_tag_ = ContentTag();
  for (size_t i = 0; i < nodes_.size(); ++i) {
    const StreamNode& node = nodes_[i];
    if (!node.tombstoned && !node.has_parent) {
      root_tag_ = ContentTag(i);
    }
  }
  computed_root_ = true;
}

std::vector<ContentRevision> FeatureTree::GetVisibleContent() {
  ResolveRoot();
  std::vector<ContentRevision> result;
  std::vector<ContentTag> stack;

  // Node: Cycles are impossible here. The root node is guaranteed to
  // not be a child. All other nodes have exactly one parent.
  // It is possible for nodes to cycle, like A->B->A, but in this case there can
  // be no valid root because all nodes have a parent.
  stack.push_back(root_tag_);
  while (!stack.empty()) {
    const ContentTag tag = stack.back();
    stack.pop_back();
    const StreamNode* node = FindNode(tag);
    if (!node || node->tombstoned)
      continue;
    if (!node->last_child.is_null()) {
      for (ContentTag child_id = node->last_child; !child_id.is_null();
           child_id = nodes_[child_id.value()].previous_sibling) {
        stack.push_back(child_id);
      }
    }
    if (!node->content_revision.is_null()) {
      result.push_back(node->content_revision);
    }
  }
  return result;
}

std::string FeatureTree::DumpStateForTesting() {
  std::stringstream ss;
  ss << "FeatureTree{\n";
  ResolveRoot();
  std::vector<std::pair<int, ContentTag>> stack;

  stack.push_back({1, root_tag_});
  while (!stack.empty()) {
    const ContentTag tag = stack.back().second;
    const int depth = stack.back().first;
    stack.pop_back();
    const StreamNode* node = FindNode(tag);
    if (!node || node->tombstoned)
      continue;
    ss << std::string(depth, ' ') << "|-";
    ss << (node->has_parent ? "ROOT" : "node");
    if (!node->last_child.is_null()) {
      for (ContentTag child_id = node->last_child; !child_id.is_null();
           child_id = nodes_[child_id.value()].previous_sibling) {
        stack.push_back({depth + 1, child_id});
      }
    }
    if (!node->content_revision.is_null()) {
      const feedstore::Content* content = FindContent(node->content_revision);
      ss << " content.frame=" << ToAsciiForTesting(content->frame());
    }
    ss << '\n';
  }
  ss << "}FeatureTree\n";
  return ss.str();
}

}  // namespace stream_model
}  // namespace feed
