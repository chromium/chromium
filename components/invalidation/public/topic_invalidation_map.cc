// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/topic_invalidation_map.h"

#include <stddef.h>

#include "base/json/json_string_value_serializer.h"
#include "components/invalidation/public/object_id_invalidation_map.h"

namespace syncer {

TopicInvalidationMap::TopicInvalidationMap() {}

TopicInvalidationMap::TopicInvalidationMap(const TopicInvalidationMap& other) =
    default;

TopicInvalidationMap::~TopicInvalidationMap() {}

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
  map_[invalidation.object_id().name()].Insert(invalidation);
}

TopicInvalidationMap TopicInvalidationMap::GetSubsetWithTopics(
    const Topics& topics) const {
  TopicToListMap new_map;
  for (const auto& topic : topics) {
    auto lookup = map_.find(topic.first);
    if (lookup != map_.end()) {
      new_map[topic.first] = lookup->second;
    }
  }
  return TopicInvalidationMap(new_map);
}

const SingleObjectInvalidationSet& TopicInvalidationMap::ForTopic(
    Topic topic) const {
  auto lookup = map_.find(topic);
  DCHECK(lookup != map_.end());
  DCHECK(!lookup->second.IsEmpty());
  return lookup->second;
}

void TopicInvalidationMap::GetAllInvalidations(
    std::vector<syncer::Invalidation>* out) const {
  for (auto it = map_.begin(); it != map_.end(); ++it) {
    out->insert(out->begin(), it->second.begin(), it->second.end());
  }
}

void TopicInvalidationMap::AcknowledgeAll() const {
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      it2->Acknowledge();
    }
  }
}

bool TopicInvalidationMap::operator==(const TopicInvalidationMap& other) const {
  return map_ == other.map_;
}

std::unique_ptr<base::ListValue> TopicInvalidationMap::ToValue() const {
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      value->Append(it2->ToValue());
    }
  }
  return value;
}

bool TopicInvalidationMap::ResetFromValue(const base::ListValue& value) {
  map_.clear();
  for (size_t i = 0; i < value.GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (!value.GetDictionary(i, &dict)) {
      return false;
    }
    std::unique_ptr<Invalidation> invalidation =
        Invalidation::InitFromValue(*dict);
    if (!invalidation) {
      return false;
    }
    Insert(*invalidation);
  }
  return true;
}

std::string TopicInvalidationMap::ToString() const {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(*ToValue());
  return output;
}

TopicInvalidationMap::TopicInvalidationMap(const TopicToListMap& map)
    : map_(map) {}

TopicInvalidationMap ConvertObjectIdInvalidationMapToTopicInvalidationMap(
    ObjectIdInvalidationMap object_ids_map) {
  TopicInvalidationMap topics_map;
  std::vector<Invalidation> invalidations;
  object_ids_map.GetAllInvalidations(&invalidations);
  for (const auto& invalidation : invalidations) {
    topics_map.Insert(invalidation);
  }
  return topics_map;
}

ObjectIdInvalidationMap ConvertTopicInvalidationMapToObjectIdInvalidationMap(
    const TopicInvalidationMap& topics_map) {
  ObjectIdInvalidationMap object_ids_map;
  std::vector<Invalidation> invalidations;
  topics_map.GetAllInvalidations(&invalidations);
  for (const auto& invalidation : invalidations) {
    object_ids_map.Insert(invalidation);
  }
  return object_ids_map;
}

}  // namespace syncer
