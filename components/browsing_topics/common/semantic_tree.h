// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
#define COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_

#include <map>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "components/browsing_topics/common/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace browsing_topics {

// Stores all taxonomies, including how the topics within the taxonomies relate,
// which topics are available in which taxonomies, and the localized name
// message ids to use to get the topic names.
class COMPONENT_EXPORT(BROWSING_TOPICS_COMMON) SemanticTree {
 public:
  SemanticTree();
  SemanticTree(const SemanticTree& other) = delete;
  ~SemanticTree();
  std::set<Topic> GetDescendantTopics(const Topic& topic);
  absl::optional<int> GetLocalizedNameMessageId(const Topic& topic,
                                                int taxonomy_version);

 private:
  void InitializeParentToChildTopicMap();

  // `parent_to_child_topic_map_` is lazy initialized when GetDescendantTopics
  // is first used
  absl::optional<std::map<Topic, std::vector<Topic>>>
      parent_to_child_topic_map_;
};
}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
