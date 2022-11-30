// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/topic_data.h"

namespace invalidation {

TopicData::TopicData(const std::string& name, bool is_public)
    : name(name), is_public(is_public) {}

TopicData::TopicData(const TopicData& other) = default;

TopicData::TopicData(TopicData&& other) = default;

TopicData& TopicData::operator=(const TopicData& other) = default;
TopicData& TopicData::operator=(TopicData&& other) = default;

bool operator==(const TopicData& lhs, const TopicData& rhs) {
  return lhs.name == rhs.name && lhs.is_public == rhs.is_public;
}

bool operator!=(const TopicData& lhs, const TopicData& rhs) {
  return !(lhs == rhs);
}

bool operator<(const TopicData& lhs, const TopicData& rhs) {
  if (lhs.name != rhs.name) {
    return lhs.name < rhs.name;
  }
  return lhs.is_public < rhs.is_public;
}

Topics ConvertTopicSetToLegacyTopicMap(const std::set<TopicData>& topics) {
  Topics result;
  for (const TopicData& topic : topics) {
    result.emplace(topic.name, TopicMetadata{/*is_public=*/topic.is_public});
  }
  return result;
}

}  // namespace invalidation