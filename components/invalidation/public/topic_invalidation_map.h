// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_TOPIC_INVALIDATION_MAP_H_
#define COMPONENTS_INVALIDATION_PUBLIC_TOPIC_INVALIDATION_MAP_H_

#include <map>
#include <memory>
#include <vector>

#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/single_topic_invalidation_set.h"

namespace invalidation {

// A set of notifications with some helper methods to organize them by Topic
// and version number.
class INVALIDATION_EXPORT TopicInvalidationMap {
 public:
  TopicInvalidationMap();
  TopicInvalidationMap(const TopicInvalidationMap& other);
  TopicInvalidationMap& operator=(const TopicInvalidationMap& other);
  ~TopicInvalidationMap();

  // Returns set of Topics for which at least one invalidation is present.
  TopicSet GetTopics() const;

  // Returns true if this map contains no invalidations.
  bool Empty() const;

  // Returns true if both maps contain the same set of invalidations.
  bool operator==(const TopicInvalidationMap& other) const;

  // Inserts a new invalidation into this map.
  void Insert(const Invalidation& invalidation);

  // Returns a new map containing the subset of invaliations from this map
  // whose topic were in the specified |topics|.
  // TODO(crbug.com/1029698): replace all usages with the version below and
  // remove this method.
  TopicInvalidationMap GetSubsetWithTopics(const Topics& topics) const;

  // Returns a new map containing the subset of invaliations from this map
  // whose topic were in the specified |topics|.
  TopicInvalidationMap GetSubsetWithTopics(const TopicSet& topics) const;

  // Returns the subset of invalidations with Topic matching |topic|.
  const SingleTopicInvalidationSet& ForTopic(Topic topic) const;

  // Returns the contents of this map in a single vector.
  void GetAllInvalidations(std::vector<Invalidation>* out) const;

  // Call Acknowledge() on all contained Invalidations.
  void AcknowledgeAll() const;

  // Serialize this map to a value. Used to expose value on
  // chrome://invalidations page.
  base::Value::List ToValue() const;

 private:
  explicit TopicInvalidationMap(
      const std::map<Topic, SingleTopicInvalidationSet>& map);

  std::map<Topic, SingleTopicInvalidationSet> map_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_TOPIC_INVALIDATION_MAP_H_
