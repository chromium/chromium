// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_CANDIDATE_TOPIC_H_
#define COMPONENTS_BROWSING_TOPICS_CANDIDATE_TOPIC_H_

#include "components/browsing_topics/common/common_types.h"

namespace browsing_topics {

// Contains a topic, the associated attributes, and flags that determine how
// the topic should be handled. This is used as the intermediate result type
// when calculating the topics for a site.
class CandidateTopic {
 public:
  static CandidateTopic Create(Topic topic,
                               bool is_true_topic,
                               bool should_be_filtered,
                               int config_version,
                               int taxonomy_version,
                               int64_t model_version);

  static CandidateTopic CreateInvalid();

  CandidateTopic(const CandidateTopic&) = delete;
  CandidateTopic& operator=(const CandidateTopic&) = delete;

  CandidateTopic(CandidateTopic&&) = default;
  CandidateTopic& operator=(CandidateTopic&&) = default;

  bool IsValid() const { return topic_ != Topic(0); }

  const Topic& topic() const { return topic_; }

  bool is_true_topic() const { return is_true_topic_; }

  bool should_be_filtered() const { return should_be_filtered_; }

  int config_version() const { return config_version_; }

  int taxonomy_version() const { return taxonomy_version_; }

  int64_t model_version() const { return model_version_; }

 private:
  CandidateTopic(Topic topic,
                 bool is_true_topic,
                 bool should_be_filtered,
                 int config_version,
                 int taxonomy_version,
                 int64_t model_version);

  Topic topic_;
  bool is_true_topic_;
  bool should_be_filtered_;
  int config_version_;
  int taxonomy_version_;
  int64_t model_version_;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_CANDIDATE_TOPIC_H_
