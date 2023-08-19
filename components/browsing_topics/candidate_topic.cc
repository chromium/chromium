// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/candidate_topic.h"

namespace browsing_topics {

// static
CandidateTopic CandidateTopic::Create(Topic topic,
                                      bool is_true_topic,
                                      bool should_be_filtered,
                                      int config_version,
                                      int taxonomy_version,
                                      int64_t model_version) {
  DCHECK_NE(topic, Topic(0));

  return CandidateTopic(topic, is_true_topic, should_be_filtered,
                        config_version, taxonomy_version, model_version);
}

// static
CandidateTopic CandidateTopic::CreateInvalid() {
  return CandidateTopic(Topic(0), /*is_true_topic=*/false,
                        /*should_be_filtered=*/false,
                        /*config_version=*/0,
                        /*taxonomy_version=*/0,
                        /*model_version=*/0);
}

CandidateTopic::CandidateTopic(Topic topic,
                               bool is_true_topic,
                               bool should_be_filtered,
                               int config_version,
                               int taxonomy_version,
                               int64_t model_version)
    : topic_(topic),
      is_true_topic_(is_true_topic),
      should_be_filtered_(should_be_filtered),
      config_version_(config_version),
      taxonomy_version_(taxonomy_version),
      model_version_(model_version) {}

}  // namespace browsing_topics
