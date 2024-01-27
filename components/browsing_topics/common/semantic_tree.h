// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
#define COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_

#include <optional>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "components/browsing_topics/common/common_types.h"

namespace browsing_topics {

// Stores all taxonomies, including how the topics within the taxonomies relate,
// which topics are available in which taxonomies, and the localized name
// message ids to use to get the topic names.
class COMPONENT_EXPORT(BROWSING_TOPICS_COMMON) SemanticTree {
 public:
  static constexpr int kNumTopics = 629;
  static constexpr int kMaxTaxonomyVersion = 2;

  SemanticTree();
  SemanticTree(const SemanticTree& other) = delete;
  ~SemanticTree();

  // Get a topic in taxonomy `taxonomy_version`. The result is deterministic.
  // `random_topic_index_decision` % the taxonomy size is used to select
  // the index of the topic in the taxonomy.
  Topic GetRandomTopic(int taxonomy_version,
                       uint64_t random_topic_index_decision);

  // Returns all first level topics (aka Top Level topics, topics without
  // parents).
  std::vector<Topic> GetFirstLevelTopicsInCurrentTaxonomy();

  // Returns at most 2 representative topics for a given topic. A representative
  // is not necessarily a descendant, it's just a topic example that represents
  // well the passed topic.
  std::vector<Topic> GetAtMostTwoRepresentativesInCurrentTaxonomy(
      const Topic& topic);

  // Get whether the `taxonomy_version` is supported by the semantic tree.
  bool IsTaxonomySupported(int taxonomy_version);

  // Returns the list of all the descendant topics for a given `topic`. When
  // `only_direct` is set to true it returns only the direct descendants.
  std::vector<Topic> GetDescendantTopics(const Topic& topic,
                                         bool only_direct = false);
  std::vector<Topic> GetAncestorTopics(const Topic& topic);
  // Get the most recent localized name message id as of the version in
  // `blink::features::kBrowsingTopicsTaxonomyVersion.Get()`.
  std::optional<int> GetLatestLocalizedNameMessageId(const Topic& topic);

 private:
  // Get the localized name message id for a topic in taxonomy
  // `taxonomy_version.` If the topic is not in taxonomy `taxonomy_version,` try
  // to get the most recent name for a prior taxonomy. If the topic was not in
  // any taxonomy, return an empty result.
  std::optional<int> GetLocalizedNameMessageId(const Topic& topic,
                                               int taxonomy_version);
  FRIEND_TEST_ALL_PREFIXES(SemanticTreeUnittest,
                           RepresentativesNeverEmptyForFirstLevelTopics);
  FRIEND_TEST_ALL_PREFIXES(SemanticTreeUnittest,
                           RepresentativesAreTopicsInTheCurrentTaxonomy);
  // Returns all first level topics (aka Top Level topics, topics without
  // parents).
  std::vector<Topic> GetFirstLevelTopicsInCurrentTaxonomyInternal();

  FRIEND_TEST_ALL_PREFIXES(SemanticTreeUnittest,
                           RepresentativesAreTopicsInTheCurrentTaxonomy);
  // Returns a set containing all the topics values in
  // the current taxonomy.
  std::set<int> GetTopicsInCurrentTaxonomyInternal();
};
}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_COMMON_SEMANTIC_TREE_H_
