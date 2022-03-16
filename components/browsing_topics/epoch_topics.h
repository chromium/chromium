// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_
#define COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/browsing_topics/topic_and_domains.h"
#include "components/browsing_topics/util.h"
#include "url/origin.h"

namespace browsing_topics {

// Contains the epoch's top topics. This is the necessary data to calculate the
// browsing topic for one epoch when a context requests it via
// document.browsingTopics().
class EpochTopics {
 public:
  EpochTopics();

  EpochTopics(std::vector<TopicAndDomains> top_topics_and_observing_domains,
              size_t padded_top_topics_start_index,
              size_t taxonomy_size,
              int taxonomy_version,
              int model_version,
              base::Time calculation_time);

  EpochTopics(const EpochTopics&) = delete;
  EpochTopics& operator=(const EpochTopics&) = delete;

  EpochTopics(EpochTopics&&);
  EpochTopics& operator=(EpochTopics&&);

  ~EpochTopics();

  // Serialization functions for storing in prefs.
  static EpochTopics FromDictValue(const base::Value::Dict& dict_value);
  base::Value::Dict ToDictValue() const;

  // Calculate the topic to expose on `top_domain` when requested by a context
  // where the domain hash is `hashed_context_domain`. Return absl::nullopt when
  // there are no topics (i.e. calculation failed, or the topics were cleared),
  // or when the candidate topic is filtered due to the context has not observed
  // the topic before. The `hmac_key` is the one used to hash the domains inside
  // `top_topics_and_observing_domains_` and `hashed_context_domain`.
  absl::optional<Topic> TopicForSite(const std::string& top_domain,
                                     const HashedDomain& hashed_context_domain,
                                     ReadOnlyHmacKey hmac_key) const;

  bool HasValidTopics() const {
    return !top_topics_and_observing_domains_.empty();
  }

  // Clear `top_topics_and_observing_domains_` and
  // reset `padded_top_topics_start_index_` to 0.
  void ClearTopics();

  int taxonomy_version() const { return taxonomy_version_; }

  int model_version() const { return model_version_; }

  base::Time calculation_time() const { return calculation_time_; }

 private:
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
  int model_version_ = 0;

  // The calculation start time. This also determines the time range of the
  // underlying topics data.
  base::Time calculation_time_;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_EPOCH_TOPICS_H_
