// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_MODEL_FEATURE_TREE_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_MODEL_FEATURE_TREE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/id_type.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/types.h"

namespace feed {
namespace stream_model {

// Uniquely identifies a feedwire::ContentId. Provided by |ContentMap|.
using ContentTag = base::IdTypeU32<class ContentTagClass>;
using ContentRevision = feed::ContentRevision;

// Owns instances of feedstore::Content pointed to by the feature tree, and
// maps ContentId into ContentTag.
class ContentMap {
 public:
  explicit ContentMap(ContentRevision::Generator* revision_generator);
  ~ContentMap();
  ContentMap(const ContentMap&) = delete;
  ContentMap& operator=(const ContentMap&) = delete;

  ContentTag GetContentTag(const feedwire::ContentId& id);

  const feedstore::Content* FindContent(ContentRevision content_revision);
  ContentRevision LookupContentRevision(const feedstore::Content& content);
  ContentRevision AddContent(feedstore::Content content);

  void Clear();

 private:
  ContentTag::Generator tag_generator_;
  raw_ptr<ContentRevision::Generator> revision_generator_;
  std::map<feedwire::ContentId, ContentTag, ContentIdCompareFunctor> mapping_;

  // These two containers work together to store and index content.
  // Each unique piece of content is stored once, and never removed.
  std::map<feedstore::Content, ContentRevision, ContentCompareFunctor> content_;
  std::vector<raw_ptr<const feedstore::Content, VectorExperimental>>
      revision_to_content_;
};

// A node in FeatureTree.
struct StreamNode {
  StreamNode();
  ~StreamNode();
  StreamNode(const StreamNode&);
  StreamNode& operator=(const StreamNode&);
  // If true, this nodes has been removed and should be ignored.
  bool tombstoned = false;
  // Whether this node has a parent.
  bool has_parent = false;
  // If this node has content, this identifies it.
  ContentRevision content_revision;
  // Child relations are stored in linked-list fashion.
  // The ID of the last child, or null.
  ContentTag last_child;
  // The ID of the sibling before this one.
  ContentTag previous_sibling;
};

// The feature tree which underlies StreamModel.
// This tree is different than most, the rules are as follows:
// * A node may or may not have a parent, so this is more of a forest than a
//   tree.
// * When nodes are removed, their set of children are remembered. If the node
//   is added again, it retains its old children.
// * A node can be added multiple times, but subsequent adds will not change
//   the node's parent.
// * There is only one 'stream root' acknowledged, even though there can be many
//   roots. The stream root is the last root node added of type STREAM. The
//   stream root identifies the tree whose nodes are used to compute
//   |GetVisibleContent()|.
// * A tree can be constructed with a base tree. This copies features from base,
//   but refers to content stored in base by reference.
class FeatureTree {
 public:
  // Constructor. |id_map| is retained by FeatureTree, and must have a greater
  // scope than FeatureTree.
  explicit FeatureTree(ContentMap* id_map);
  // Create a |FeatureTree| which starts as a copy of |base|.
  // Copies structure from |base|, and keeps a reference for content access.
  explicit FeatureTree(const FeatureTree* base);
  ~FeatureTree();

  FeatureTree(const FeatureTree& src) = delete;
  FeatureTree& operator=(const FeatureTree& src) = delete;

  // Mutations.

  void ApplyStreamStructure(const feedstore::StreamStructure& structure);
  // Adds |content| to the tree.
  void AddContent(feedstore::Content content);
  // Same as |AddContent()|, but can avoid a copy of |content| if it already
  // exists.
  void CopyAndAddContent(const feedstore::Content& content);

  // Data access.

  const StreamNode* FindNode(ContentTag id) const;
  StreamNode* FindNode(ContentTag id);
  const feedstore::Content* FindContent(ContentRevision id) const;
  ContentTag GetContentTag(const feedwire::ContentId& id) {
    return content_map_->GetContentTag(id);
  }

  // Returns the list of content that should be visible.
  std::vector<ContentRevision> GetVisibleContent();

  std::string DumpStateForTesting();

 private:
  StreamNode* GetOrMakeNode(ContentTag id);
  void ResolveRoot();
  void ResizeNodesIfNeeded(ContentTag id);
  void RemoveFromParent(ContentTag node_id);
  bool RemoveFromParent(StreamNode* parent, ContentTag node_id);

  raw_ptr<const FeatureTree> base_ = nullptr;  // Unowned.
  raw_ptr<ContentMap> content_map_;            // Unowned.
  // Finding the root:
  // We pick the root node as the last STREAM node which has no parent.
  // In most cases, we can identify the root as the tree is built.
  // But in some cases, we need to search all nodes to find the root.
  // |computed_root_| is true if |root_tag_| is guaranteed to identify the root.
  bool computed_root_ = true;
  ContentTag root_tag_;
  // All nodes in the forest, included removed nodes.
  // This datastructure was selected to make copies efficient.
  std::vector<StreamNode> nodes_;
};

}  // namespace stream_model
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_MODEL_FEATURE_TREE_H_
