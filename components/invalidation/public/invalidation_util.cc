// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_util.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";

const int kDeprecatedSource = 2000;

}  // namespace

namespace syncer {

bool ObjectIdLessThan::operator()(const invalidation::ObjectId& lhs,
                                  const invalidation::ObjectId& rhs) const {
  return (lhs.source() < rhs.source()) ||
         (lhs.source() == rhs.source() && lhs.name() < rhs.name());
}

bool InvalidationVersionLessThan::operator()(const Invalidation& a,
                                             const Invalidation& b) const {
  DCHECK(a.object_id() == b.object_id())
      << "a: " << ObjectIdToString(a.object_id()) << ", "
      << "b: " << ObjectIdToString(a.object_id());

  if (a.is_unknown_version() && !b.is_unknown_version())
    return true;

  if (!a.is_unknown_version() && b.is_unknown_version())
    return false;

  if (a.is_unknown_version() && b.is_unknown_version())
    return false;

  return a.version() < b.version();
}

std::unique_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("source", object_id.source());
  value->SetString("name", object_id.name());
  return value;
}

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out) {
  *out = invalidation::ObjectId();
  std::string name;
  int source = 0;
  if (!value.GetInteger("source", &source) || !value.GetString("name", &name)) {
    return false;
  }
  *out = invalidation::ObjectId(source, name);
  return true;
}

std::string ObjectIdToString(const invalidation::ObjectId& object_id) {
  std::string str;
  base::JSONWriter::Write(*ObjectIdToValue(object_id), &str);
  return str;
}

bool InvalidationObjectIdLessThan::operator()(
    const invalidation::InvalidationObjectId& lhs,
    const invalidation::InvalidationObjectId& rhs) const {
  return (lhs.source() < rhs.source()) ||
         (lhs.source() == rhs.source() && lhs.name() < rhs.name());
}

std::unique_ptr<base::DictionaryValue> InvalidationObjectIdToValue(
    const invalidation::InvalidationObjectId& object_id) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(kNameKey, object_id.name());
  value->SetInteger(kSourceKey, object_id.source());
  return value;
}

std::string SerializeInvalidationObjectId(
    const invalidation::InvalidationObjectId& object_id) {
  std::unique_ptr<base::DictionaryValue> value =
      InvalidationObjectIdToValue(object_id);
  std::string serialized_id;
  JSONStringValueSerializer serializer(&serialized_id);
  serializer.Serialize(*value);
  return serialized_id;
}

bool DeserializeInvalidationObjectId(const std::string& serialized_id,
                                     invalidation::InvalidationObjectId* id) {
  JSONStringValueDeserializer deserializer(serialized_id);
  int error_code;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  base::DictionaryValue* dict = nullptr;

  if (!value->GetAsDictionary(&dict)) {
    DLOG(WARNING) << "Unable to get dictionary";
    return false;
  }

  int source = 0;
  if (!dict->GetInteger(kSourceKey, &source)) {
    DLOG(WARNING) << "Unable to deserialize source";
    return false;
  }

  std::string name;
  if (!dict->GetString(kNameKey, &name)) {
    DLOG(WARNING) << "Unable to deserialize name";
    return false;
  }
  id->Init(source, name);
  return true;
}

std::string InvalidationObjectIdToString(
    const invalidation::InvalidationObjectId& object_id) {
  std::string str;
  base::JSONWriter::Write(*InvalidationObjectIdToValue(object_id), &str);
  return str;
}

TopicSet ConvertIdsToTopics(ObjectIdSet ids) {
  TopicSet topics;
  for (const auto& id : ids)
    topics.insert(id.name());
  return topics;
}

ObjectIdSet ConvertTopicsToIds(TopicSet topics) {
  ObjectIdSet ids;
  for (const auto& topic : topics)
    ids.insert(invalidation::ObjectId(kDeprecatedSource, topic));
  return ids;
}

invalidation::ObjectId ConvertTopicToId(const Topic& topic) {
  return invalidation::ObjectId(kDeprecatedSource, topic);
}

HandlerOwnerType OwnerNameToHandlerType(const std::string& owner_name) {
  if (owner_name == "Cloud")
    return HandlerOwnerType::kCloud;
  if (owner_name == "Fake")
    return HandlerOwnerType::kFake;
  if (owner_name == "RemoteCommands")
    return HandlerOwnerType::kRemoteCommands;
  if (owner_name == "Drive")
    return HandlerOwnerType::kDrive;
  if (owner_name == "Sync")
    return HandlerOwnerType::kSync;
  if (owner_name == "TICL")
    return HandlerOwnerType::kTicl;
  if (owner_name == "ChildAccountInfoFetcherImpl")
    return HandlerOwnerType::kChildAccount;
  if (owner_name == "NotificationPrinter")
    return HandlerOwnerType::kNotificationPrinter;
  if (owner_name == "InvalidatorShim")
    return HandlerOwnerType::kInvalidatorShim;
  if (owner_name == "SyncBackendHostImpl")
    return HandlerOwnerType::kSyncBackendHostImpl;
  return HandlerOwnerType::kUnknown;
}

}  // namespace syncer
