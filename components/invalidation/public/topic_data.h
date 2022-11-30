// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_TOPIC_DATA_H_
#define COMPONENTS_INVALIDATION_PUBLIC_TOPIC_DATA_H_

#include <string>

#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

// Represents invalidation topic concept: A "namespace" or "channel" of
// messages that clients can subscribe to. For Sync, they correspond to data
// types.
// TODO(crbug.com/1029698): rename to Topic.
class INVALIDATION_EXPORT TopicData {
 public:
  TopicData(const std::string& name, bool is_public);
  TopicData(const TopicData& other);
  TopicData(TopicData&& other);

  TopicData& operator=(const TopicData& other);
  TopicData& operator=(TopicData&& other);

  // A public name of the topic (i.e. it's always not GAIA-keyed).
  std::string name;

  // A topic can be either private (i.e. GAIA-keyed) or public. For private
  // topics, a unique ID derived from the user's GAIA ID is appended to the
  // topic name to make it unique (though this is an implementation detail
  // which is hidden from clients).
  bool is_public;
};

INVALIDATION_EXPORT bool operator==(const TopicData& lhs, const TopicData& rhs);
INVALIDATION_EXPORT bool operator!=(const TopicData& lhs, const TopicData& rhs);
INVALIDATION_EXPORT bool operator<(const TopicData& lhs, const TopicData& rhs);

INVALIDATION_EXPORT std::string ToString(const TopicData& topic_data);

// TDOO(crbug.com/1029698): delete this function together with legacy topic
// datatypes.
Topics ConvertTopicSetToLegacyTopicMap(const std::set<TopicData>& topics);

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_TOPIC_DATA_H_