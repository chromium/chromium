// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_TOPIC_AND_DOMAINS_H_
#define COMPONENTS_BROWSING_TOPICS_TOPIC_AND_DOMAINS_H_

#include <set>

#include "base/types/strong_alias.h"
#include "base/values.h"
#include "components/browsing_topics/common/common_types.h"

namespace browsing_topics {

// Contains a topic and a set of hashed domains that has observed the associated
// topic (i.e. the Topics API was used within the domains's context, and the
// page is related to that topic.)
class TopicAndDomains {
 public:
  TopicAndDomains();
  TopicAndDomains(Topic topic, std::set<HashedDomain> hashed_domains);

  TopicAndDomains(const TopicAndDomains&) = delete;
  TopicAndDomains& operator=(const TopicAndDomains&) = delete;

  TopicAndDomains(TopicAndDomains&&);
  TopicAndDomains& operator=(TopicAndDomains&&);

  ~TopicAndDomains();

  // Serialization functions for storing in prefs.
  static TopicAndDomains FromDictValue(const base::Value::Dict& dict_value);
  base::Value::Dict ToDictValue() const;

  void ClearDomain(const HashedDomain& domain);

  bool IsValid() const { return topic_ != Topic(0); }

  const Topic& topic() const { return topic_; }

  const std::set<HashedDomain>& hashed_domains() const {
    return hashed_domains_;
  }

 private:
  Topic topic_{0};
  std::set<HashedDomain> hashed_domains_;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_TOPIC_AND_DOMAINS_H_
