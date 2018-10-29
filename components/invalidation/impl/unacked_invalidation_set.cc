// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/unacked_invalidation_set.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/object_id_invalidation_map.h"

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";
const char kInvalidationListKey[] = "invalidation-list";

}  // namespace

namespace syncer {

const size_t UnackedInvalidationSet::kMaxBufferedInvalidations = 5;

// static
UnackedInvalidationSet::UnackedInvalidationSet(
    invalidation::ObjectId id)
    : registered_(false),
      object_id_(id) {}

UnackedInvalidationSet::UnackedInvalidationSet(
    const UnackedInvalidationSet& other)
    : registered_(other.registered_),
      object_id_(other.object_id_),
      invalidations_(other.invalidations_) {
}

UnackedInvalidationSet::~UnackedInvalidationSet() {}

const invalidation::ObjectId& UnackedInvalidationSet::object_id() const {
  return object_id_;
}

void UnackedInvalidationSet::Add(
    const Invalidation& invalidation) {
  SingleObjectInvalidationSet set;
  set.Insert(invalidation);
  AddSet(set);
  if (!registered_)
    Truncate(kMaxBufferedInvalidations);
}

void UnackedInvalidationSet::AddSet(
    const SingleObjectInvalidationSet& invalidations) {
  invalidations_.insert(invalidations.begin(), invalidations.end());
  if (!registered_)
    Truncate(kMaxBufferedInvalidations);
}

void UnackedInvalidationSet::ExportInvalidations(
    base::WeakPtr<AckHandler> ack_handler,
    scoped_refptr<base::SingleThreadTaskRunner> ack_handler_task_runner,
    ObjectIdInvalidationMap* out) const {
  for (auto it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    // Copy the invalidation and set the copy's ack_handler.
    Invalidation inv(*it);
    inv.SetAckHandler(ack_handler, ack_handler_task_runner);
    out->Insert(inv);
  }
}

void UnackedInvalidationSet::Clear() {
  invalidations_.clear();
}

void UnackedInvalidationSet::SetHandlerIsRegistered() {
  registered_ = true;
}

void UnackedInvalidationSet::SetHandlerIsUnregistered() {
  registered_ = false;
  Truncate(kMaxBufferedInvalidations);
}

// Removes the matching ack handle from the list.
void UnackedInvalidationSet::Acknowledge(const AckHandle& handle) {
  bool handle_found = false;
  for (auto it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    if (it->ack_handle().Equals(handle)) {
      invalidations_.erase(*it);
      handle_found = true;
      break;
    }
  }
  DLOG_IF(WARNING, !handle_found)
      << "Unrecognized to ack for object " << ObjectIdToString(object_id_);
  (void)handle_found;  // Silence unused variable warning in release builds.
}

// Erase the invalidation with matching ack handle from the list.  Also creates
// an 'UnknownVersion' invalidation with the same ack handle and places it at
// the beginning of the list.  If an unknown version invalidation currently
// exists, it is replaced.
void UnackedInvalidationSet::Drop(const AckHandle& handle) {
  SingleObjectInvalidationSet::const_iterator it;
  for (it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    if (it->ack_handle().Equals(handle)) {
      break;
    }
  }
  if (it == invalidations_.end()) {
    DLOG(WARNING) << "Unrecognized drop request for object "
                  << ObjectIdToString(object_id_);
    return;
  }

  Invalidation unknown_version = Invalidation::InitFromDroppedInvalidation(*it);
  invalidations_.erase(*it);

  // If an unknown version is in the list, we remove it so we can replace it.
  if (!invalidations_.empty() && invalidations_.begin()->is_unknown_version()) {
    invalidations_.erase(*invalidations_.begin());
  }

  invalidations_.insert(unknown_version);
}

// static
bool UnackedInvalidationSet::DeserializeSetIntoMap(
    const base::DictionaryValue& dict,
    UnackedInvalidationsMap* map) {
  std::string source_str;
  if (!dict.GetString(kSourceKey, &source_str)) {
    DLOG(WARNING) << "Unable to deserialize source";
    return false;
  }
  int source = 0;
  if (!base::StringToInt(source_str, &source)) {
    DLOG(WARNING) << "Invalid source: " << source_str;
    return false;
  }
  std::string name;
  if (!dict.GetString(kNameKey, &name)) {
    DLOG(WARNING) << "Unable to deserialize name";
    return false;
  }
  invalidation::ObjectId id(source, name);
  UnackedInvalidationSet storage(id);
  const base::ListValue* invalidation_list = nullptr;
  if (!dict.GetList(kInvalidationListKey, &invalidation_list) ||
      !storage.ResetListFromValue(*invalidation_list)) {
    // Earlier versions of this class did not set this field, so we don't treat
    // parsing errors here as a fatal failure.
    DLOG(WARNING) << "Unable to deserialize invalidation list.";
  }
  map->insert(std::make_pair(id, storage));
  return true;
}

std::unique_ptr<base::DictionaryValue> UnackedInvalidationSet::ToValue() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(kSourceKey, base::IntToString(object_id_.source()));
  value->SetString(kNameKey, object_id_.name());

  std::unique_ptr<base::ListValue> list_value(new base::ListValue);
  for (auto it = invalidations_.begin(); it != invalidations_.end(); ++it) {
    list_value->Append(it->ToValue());
  }
  value->Set(kInvalidationListKey, std::move(list_value));

  return value;
}

bool UnackedInvalidationSet::ResetFromValue(
    const base::DictionaryValue& value) {
  std::string source_str;
  if (!value.GetString(kSourceKey, &source_str)) {
    DLOG(WARNING) << "Unable to deserialize source";
    return false;
  }
  int source = 0;
  if (!base::StringToInt(source_str, &source)) {
    DLOG(WARNING) << "Invalid source: " << source_str;
    return false;
  }
  std::string name;
  if (!value.GetString(kNameKey, &name)) {
    DLOG(WARNING) << "Unable to deserialize name";
    return false;
  }
  object_id_ = invalidation::ObjectId(source, name);
  const base::ListValue* invalidation_list = nullptr;
  if (!value.GetList(kInvalidationListKey, &invalidation_list)
      || !ResetListFromValue(*invalidation_list)) {
    // Earlier versions of this class did not set this field, so we don't treat
    // parsing errors here as a fatal failure.
    DLOG(WARNING) << "Unable to deserialize invalidation list.";
  }
  return true;
}

bool UnackedInvalidationSet::ResetListFromValue(
    const base::ListValue& list) {
  for (size_t i = 0; i < list.GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (!list.GetDictionary(i, &dict)) {
      DLOG(WARNING) << "Failed to get invalidation dictionary at index " << i;
      return false;
    }
    std::unique_ptr<Invalidation> invalidation =
        Invalidation::InitFromValue(*dict);
    if (!invalidation) {
      DLOG(WARNING) << "Failed to parse invalidation at index " << i;
      return false;
    }
    invalidations_.insert(*invalidation);
  }
  return true;
}

void UnackedInvalidationSet::Truncate(size_t max_size) {
  DCHECK_GT(max_size, 0U);

  if (invalidations_.size() <= max_size) {
    return;
  }

  while (invalidations_.size() > max_size) {
    invalidations_.erase(*invalidations_.begin());
  }

  // We dropped some invalidations.  We remember the fact that an unknown
  // amount of information has been lost by ensuring this list begins with
  // an UnknownVersion invalidation.  We remove the oldest remaining
  // invalidation to make room for it.
  invalidation::ObjectId id = invalidations_.begin()->object_id();
  invalidations_.erase(*invalidations_.begin());

  Invalidation unknown_version = Invalidation::InitUnknownVersion(id);
  invalidations_.insert(unknown_version);
}

}  // namespace syncer
