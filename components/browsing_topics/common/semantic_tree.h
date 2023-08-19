// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
#define COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_

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
  static constexpr int kNumTopics = 629;

  SemanticTree();
  SemanticTree(const SemanticTree& other) = delete;
  ~SemanticTree();

  // Get a topic in taxonomy `taxonomy_version`. The result is deterministic.
  // `random_topic_index_decision` % the taxonomy size is used to select
  // the index of the topic in the taxonomy.
  Topic GetRandomTopic(int taxonomy_version,
                       uint64_t random_topic_index_decision);

  // Get whether the `taxonomy_version` is supported by the semantic tree.
  bool IsTaxonomySupported(int taxonomy_version);

  std::vector<Topic> GetDescendantTopics(const Topic& topic);
  std::vector<Topic> GetAncestorTopics(const Topic& topic);
  // Get the most recent localized name message id as of the version in
  // `blink::features::kBrowsingTopicsTaxonomyVersion.Get()`.
  absl::optional<int> GetLatestLocalizedNameMessageId(const Topic& topic);

 private:
  // Get the localized name message id for a topic in taxonomy
  // `taxonomy_version.` If the topic is not in taxonomy `taxonomy_version,` try
  // to get the most recent name for a prior taxonomy. If the topic was not in
  // any taxonomy, return an empty result.
  absl::optional<int> GetLocalizedNameMessageId(const Topic& topic,
                                                int taxonomy_version);
};
}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
