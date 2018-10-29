// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/object_id_invalidation_map.h"

#include <stddef.h>

#include "base/json/json_string_value_serializer.h"

namespace syncer {

// static
ObjectIdInvalidationMap ObjectIdInvalidationMap::InvalidateAll(
    const ObjectIdSet& ids) {
  ObjectIdInvalidationMap invalidate_all;
  for (auto it = ids.begin(); it != ids.end(); ++it) {
    invalidate_all.Insert(Invalidation::InitUnknownVersion(*it));
  }
  return invalidate_all;
}

ObjectIdInvalidationMap::ObjectIdInvalidationMap() {}

ObjectIdInvalidationMap::ObjectIdInvalidationMap(
    const ObjectIdInvalidationMap& other) = default;

ObjectIdInvalidationMap::~ObjectIdInvalidationMap() {}

ObjectIdSet ObjectIdInvalidationMap::GetObjectIds() const {
  ObjectIdSet ret;
  for (auto it = map_.begin(); it != map_.end(); ++it) {
    ret.insert(it->first);
  }
  return ret;
}

bool ObjectIdInvalidationMap::Empty() const {
  return map_.empty();
}

void ObjectIdInvalidationMap::Insert(const Invalidation& invalidation) {
  map_[invalidation.object_id()].Insert(invalidation);
}

ObjectIdInvalidationMap ObjectIdInvalidationMap::GetSubsetWithObjectIds(
    const ObjectIdSet& ids) const {
  IdToListMap new_map;
  for (auto it = ids.begin(); it != ids.end(); ++it) {
    auto lookup = map_.find(*it);
    if (lookup != map_.end()) {
      new_map[*it] = lookup->second;
    }
  }
  return ObjectIdInvalidationMap(new_map);
}

const SingleObjectInvalidationSet& ObjectIdInvalidationMap::ForObject(
    invalidation::ObjectId id) const {
  auto lookup = map_.find(id);
  DCHECK(lookup != map_.end());
  DCHECK(!lookup->second.IsEmpty());
  return lookup->second;
}

void ObjectIdInvalidationMap::GetAllInvalidations(
    std::vector<syncer::Invalidation>* out) const {
  for (auto it = map_.begin(); it != map_.end(); ++it) {
    out->insert(out->begin(), it->second.begin(), it->second.end());
  }
}
void ObjectIdInvalidationMap::AcknowledgeAll() const {
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      it2->Acknowledge();
    }
  }
}

bool ObjectIdInvalidationMap::operator==(
    const ObjectIdInvalidationMap& other) const {
  return map_ == other.map_;
}

std::unique_ptr<base::ListValue> ObjectIdInvalidationMap::ToValue() const {
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  for (auto it1 = map_.begin(); it1 != map_.end(); ++it1) {
    for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      value->Append(it2->ToValue());
    }
  }
  return value;
}

bool ObjectIdInvalidationMap::ResetFromValue(const base::ListValue& value) {
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

std::string ObjectIdInvalidationMap::ToString() const {
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(*ToValue());
  return output;
}

ObjectIdInvalidationMap::ObjectIdInvalidationMap(const IdToListMap& map)
  : map_(map) {}

}  // namespace syncer
