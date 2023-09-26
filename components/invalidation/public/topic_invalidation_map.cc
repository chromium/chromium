// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/topic_invalidation_map.h"

#include <stddef.h>

#include "base/values.h"

namespace invalidation {

TopicInvalidationMap::TopicInvalidationMap() = default;

TopicInvalidationMap::TopicInvalidationMap(const TopicInvalidationMap& other) =
    default;

TopicInvalidationMap& TopicInvalidationMap::operator=(
    const TopicInvalidationMap& other) = default;

TopicInvalidationMap::~TopicInvalidationMap() = default;

TopicSet TopicInvalidationMap::GetTopics() const {
  TopicSet ret;
  for (const auto& topic_and_invalidation_set : map_)
    ret.insert(topic_and_invalidation_set.first);
  return ret;
}

bool TopicInvalidationMap::Empty() const {
  return map_.empty();
}

void TopicInvalidationMap::Insert(const Invalidation& invalidation) {
  map_[invalidation.topic()].Insert(invalidation);
}

TopicInvalidationMap TopicInvalidationMap::GetSubsetWithTopics(
    const Topics& topics) const {
  std::map<Topic, SingleTopicInvalidationSet> new_map;
  for (const auto& topic : topics) {
    auto lookup = map_.find(topic.first);
    if (lookup != map_.end()) {
      new_map[topic.first] = lookup->second;
    }
  }
  return TopicInvalidationMap(new_map);
}

TopicInvalidationMap TopicInvalidationMap::GetSubsetWithTopics(
    const TopicSet& topics) const {
  std::map<Topic, SingleTopicInvalidationSet> new_map;
  for (const auto& topic : topics) {
    auto lookup = map_.find(topic);
    if (lookup != map_.end()) {
      new_map[topic] = lookup->second;
    }
  }
  return TopicInvalidationMap(new_map);
}

const SingleTopicInvalidationSet& TopicInvalidationMap::ForTopic(
    Topic topic) const {
  auto lookup = map_.find(topic);
  DCHECK(lookup != map_.end());
  DCHECK(!lookup->second.IsEmpty());
  return lookup->second;
}

void TopicInvalidationMap::GetAllInvalidations(
    std::vector<Invalidation>* out) const {
  for (const auto& topic_to_invalidations : map_) {
    out->insert(out->begin(), topic_to_invalidations.second.begin(),
                topic_to_invalidations.second.end());
  }
}

void TopicInvalidationMap::AcknowledgeAll() const {
  for (const auto& topic_to_invalidations : map_) {
    for (const Invalidation& invalidation : topic_to_invalidations.second) {
      invalidation.Acknowledge();
    }
  }
}

bool TopicInvalidationMap::operator==(const TopicInvalidationMap& other) const {
  return map_ == other.map_;
}

TopicInvalidationMap::TopicInvalidationMap(
    const std::map<Topic, SingleTopicInvalidationSet>& map)
    : map_(map) {}

}  // namespace invalidation
