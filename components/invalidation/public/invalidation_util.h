// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation_export.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace invalidation {
class ObjectId;
class InvalidationObjectId;
}  // namespace invalidation

namespace syncer {

// FCMInvalidationService and deprecated TiclInvalidationService uses ObjectId
// to keep track of objects to invalidate. There are 2 fields in ObjectId:
// source and name. TiclInvalidationService expects both of them, while
// FCMInvalidationService only works with the name. So InvalidationService
// assigns the value of source to kDeprecatedSourceForFCM when FCM (Firebase
// Cloud Messaging) is enabled.
extern const int kDeprecatedSourceForFCM;

// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class HandlerOwnerType {
  kCloud = 0,
  kFake = 1,
  kRemoteCommands = 2,
  kDrive = 3,
  kSync = 4,
  kTicl = 5,
  kChildAccount = 6,
  kNotificationPrinter = 7,
  kInvalidatorShim = 8,
  kSyncEngineImpl = 9,
  kUnknown = 10,
  kMaxValue = kUnknown,
};

class Invalidation;
class InvalidationHandler;

struct INVALIDATION_EXPORT ObjectIdLessThan {
  bool operator()(const invalidation::ObjectId& lhs,
                  const invalidation::ObjectId& rhs) const;
};

struct INVALIDATION_EXPORT InvalidationVersionLessThan {
  bool operator()(const Invalidation& a, const Invalidation& b) const;
};

typedef std::set<invalidation::ObjectId, ObjectIdLessThan> ObjectIdSet;

typedef std::map<invalidation::ObjectId, int, ObjectIdLessThan>
    ObjectIdCountMap;

using Topic = std::string;
// It should be std::set, since std::set_difference is used for it.
using TopicSet = std::set<std::string>;

INVALIDATION_EXPORT struct TopicMetadata {
  // Whether the topic is public.
  bool is_public;
};

INVALIDATION_EXPORT bool operator==(const TopicMetadata&, const TopicMetadata&);

using Topics = std::map<std::string, TopicMetadata>;

// Caller owns the returned DictionaryValue.
std::unique_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id);

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out);

INVALIDATION_EXPORT std::string ObjectIdToString(
    const invalidation::ObjectId& object_id);

// Same set of utils as above but for the InvalidationObjectId.

struct INVALIDATION_EXPORT InvalidationObjectIdLessThan {
  bool operator()(const invalidation::InvalidationObjectId& lhs,
                  const invalidation::InvalidationObjectId& rhs) const;
};

typedef std::set<invalidation::InvalidationObjectId,
                 InvalidationObjectIdLessThan>
    InvalidationObjectIdSet;

typedef std::
    map<invalidation::InvalidationObjectId, int, InvalidationObjectIdLessThan>
        InvalidationObjectIdCountMap;

std::unique_ptr<base::DictionaryValue> InvalidationObjectIdToValue(
    const invalidation::InvalidationObjectId& object_id);

// TODO(melandory): figure out the security implications for such serialization.
std::string SerializeInvalidationObjectId(
    const invalidation::InvalidationObjectId& object_id);
bool DeserializeInvalidationObjectId(const std::string& serialized,
                                     invalidation::InvalidationObjectId* id);

INVALIDATION_EXPORT std::string InvalidationObjectIdToString(
    const invalidation::InvalidationObjectId& object_id);

ObjectIdSet ConvertTopicsToIds(TopicSet topics);
ObjectIdSet ConvertTopicsToIds(Topics topics);
invalidation::ObjectId ConvertTopicToId(const Topic& topic);
Topics ConvertIdsToTopics(ObjectIdSet ids, InvalidationHandler* handler);

HandlerOwnerType OwnerNameToHandlerType(const std::string& owner_name);

// Returns a |Topic| contained within both |lhs| and |rhs| or null if |lhs| and
// |rhs| are disjoint.
const Topic* FindMatchingTopic(const Topics& lhs, const Topics& rhs);

// Returns a vector of Topics in |lhs| but not |rhs|.
std::vector<Topic> FindRemovedTopics(const Topics& lhs, const Topics& rhs);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
