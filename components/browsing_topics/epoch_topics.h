// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_
#define COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/browsing_topics/candidate_topic.h"
#include "components/browsing_topics/topic_and_domains.h"
#include "components/browsing_topics/util.h"
#include "url/origin.h"

namespace browsing_topics {

// Contains the epoch's top topics. This is the necessary data to calculate the
// browsing topic for one epoch when a context requests it via
// document.browsingTopics().
class EpochTopics {
 public:
  explicit EpochTopics(base::Time calculation_time);

  EpochTopics(std::vector<TopicAndDomains> top_topics_and_observing_domains,
              size_t padded_top_topics_start_index,
              size_t taxonomy_size,
              int taxonomy_version,
              int64_t model_version,
              base::Time calculation_time);

  EpochTopics(const EpochTopics&) = delete;
  EpochTopics& operator=(const EpochTopics&) = delete;

  EpochTopics(EpochTopics&&);
  EpochTopics& operator=(EpochTopics&&);

  ~EpochTopics();

  // Serialization functions for storing in prefs.
  static EpochTopics FromDictValue(const base::Value::Dict& dict_value);
  base::Value::Dict ToDictValue() const;

  // Calculate the candidate topic to expose on `top_domain` when requested by a
  // context where the domain hash is `hashed_context_domain`. The candidate
  // topic will be annotated with `is_true_topic` and `should_be_filtered` based
  // on its type and/or whether `hashed_context_domain` has observed the topic.
  // Returns an invalid `CandidateTopic` when there is no topic (e.g.
  // failed epoch topics calculation, cleared history, or cleared/blocked
  // individual topics). The `hmac_key` is the one used to hash the domains
  // inside `top_topics_and_observing_domains_` and `hashed_context_domain`.
  CandidateTopic CandidateTopicForSite(
      const std::string& top_domain,
      const HashedDomain& hashed_context_domain,
      ReadOnlyHmacKey hmac_key) const;

  // Whether `top_topics_and_observing_domains_` is empty.
  bool empty() const { return top_topics_and_observing_domains_.empty(); }

  // Clear `top_topics_and_observing_domains_` and
  // reset `padded_top_topics_start_index_` to 0.
  void ClearTopics();

  // Clear an entry in `top_topics_and_observing_domains_` that matches `topic`
  // and any entry in `top_topics_and_observing_domains_` that is a topic
  // descended from `topic`.
  void ClearTopic(Topic topic);

  // Clear the domains in `top_topics_and_observing_domains_`  that match
  // `hashed_context_domain`.
  void ClearContextDomain(const HashedDomain& hashed_context_domain);

  bool HasValidVersions() const {
    return taxonomy_version_ > 0 && model_version_ > 0;
  }

  const std::vector<TopicAndDomains>& top_topics_and_observing_domains() const {
    return top_topics_and_observing_domains_;
  }

  size_t padded_top_topics_start_index() const {
    return padded_top_topics_start_index_;
  }

  size_t taxonomy_size() const { return taxonomy_size_; }

  int taxonomy_version() const { return taxonomy_version_; }

  int64_t model_version() const { return model_version_; }

  base::Time calculation_time() const { return calculation_time_; }

 private:
  absl::optional<Topic> TopicForSiteHelper(
      const std::string& top_domain,
      bool need_filtering,
      bool allow_random_or_padded_topic,
      const HashedDomain& hashed_context_domain,
      ReadOnlyHmacKey hmac_key,
      bool& output_is_true_topic,
      bool& candidate_topic_filtered) const;

  // The top topics for this epoch, and the context domains that observed each
  // topic across
  // `kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering` epochs.
  // Its length should be either equal to the configuration parameter
  // `kBrowsingTopicsNumberOfTopTopicsPerEpoch`, or 0, which may be due to not
  // enough history entries, permission denial for calculating, or history
  // deletion.
  std::vector<TopicAndDomains> top_topics_and_observing_domains_;

  // Some topics in `top_topics_and_observing_domains_` may be randomly padded
  // at the end. `padded_top_topics_start_index_` is the starting index of
  // those randomly padded topics. If all topics in
  // `top_topics_and_observing_domains_` are real, then
  // `padded_top_topics_start_index_` will equal
  // `top_topics_and_observing_domains_.size()`.
  size_t padded_top_topics_start_index_ = 0;

  // The size of the taxonomy applicable to this epoch's topics.
  size_t taxonomy_size_ = 0;

  // The version of the taxonomy applicable to this epoch's topics.
  int taxonomy_version_ = 0;

  // The version of the model used to calculate this epoch's topics.
  int64_t model_version_ = 0;

  // The calculation start time. This determines the end time of this epoch's
  // underlying topics data, and may determine the start time of future epochs'
  // underlying topics data. It's only best effort to read this field from a
  // failed calculation, as historically this field is only set for successful
  // calculations.
  base::Time calculation_time_;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_
