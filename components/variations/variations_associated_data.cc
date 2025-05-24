// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include <map>
#include <utility>
#include <vector>

#include "base/memory/singleton.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"

namespace variations {

namespace {

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  typedef std::map<ActiveGroupId, VariationID, ActiveGroupIdCompare>
      GroupToIDMap;

  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return base::Singleton<GroupMapAccessor,
                           base::LeakySingletonTraits<GroupMapAccessor>>::get();
  }

  GroupMapAccessor(const GroupMapAccessor&) = delete;
  GroupMapAccessor& operator=(const GroupMapAccessor&) = delete;

  // Ensures that |group_identifier| is associated with only one non-trigger,
  // trigger, or signed-in key.
  void ValidateID(IDCollectionKey key,
                  const ActiveGroupId& group_identifier,
                  const VariationID id) {
    static_assert(ID_COLLECTION_COUNT == 6,
                  "If you add a new collection key, add handling code here!");
#if DCHECK_IS_ON()
    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      IDCollectionKey other_key = static_cast<IDCollectionKey>(i);
      if (key == other_key)
        continue;

      VariationID other_id = GetID(other_key, group_identifier);

      // For a GOOGLE_APP key, validate that all other collections with this
      // |group_identifier| have the same associated ID.
      if (key == GOOGLE_APP) {
        DCHECK(other_id == EMPTY_ID || other_id == id);
        continue;
      }

      // The ID should not be registered under a different non-GOOGLE_APP
      // IDCollectionKey.
      if (other_key != GOOGLE_APP)
        DCHECK_EQ(EMPTY_ID, other_id);
    }
#endif  // DCHECK_IS_ON()
  }

  // Note that this normally only sets the ID for a group the first time, unless
  // |force| is set to true, in which case it will always override it.
  void AssociateID(IDCollectionKey key,
                   const ActiveGroupId& group_identifier,
                   const VariationID id,
                   const bool force) {
    ValidateID(key, group_identifier, id);

    base::AutoLock scoped_lock(lock_);

    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    if (force ||
        group_to_id_map->find(group_identifier) == group_to_id_map->end())
      (*group_to_id_map)[group_identifier] = id;
  }

  VariationID GetID(IDCollectionKey key,
                    const ActiveGroupId& group_identifier) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    GroupToIDMap::const_iterator it = group_to_id_map->find(group_identifier);
    if (it == group_to_id_map->end())
      return EMPTY_ID;
    return it->second;
  }

  void ClearAllMapsForTesting() {
    base::AutoLock scoped_lock(lock_);

    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      GroupToIDMap* map = GetGroupToIDMap(static_cast<IDCollectionKey>(i));
      DCHECK(map);
      map->clear();
    }
  }

 private:
  friend struct base::DefaultSingletonTraits<GroupMapAccessor>;

  // Retrieves the GroupToIDMap for |key|.
  GroupToIDMap* GetGroupToIDMap(IDCollectionKey key) {
    return &group_to_id_maps_[key];
  }

  GroupMapAccessor() {
    group_to_id_maps_.resize(ID_COLLECTION_COUNT);
  }
  ~GroupMapAccessor() = default;

  base::Lock lock_;
  std::vector<GroupToIDMap> group_to_id_maps_;
};
}  // namespace

void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), id, false);
}

void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     VariationID id) {
  AssociateGoogleVariationIDForceHashes(
      key, MakeActiveGroupId(trial_name, group_name), id);
}

void AssociateGoogleVariationIDForceHashes(IDCollectionKey key,
                                           const ActiveGroupId& active_group,
                                           VariationID id) {
  GroupMapAccessor::GetInstance()->AssociateID(key, active_group, id, true);
}

VariationID GetGoogleVariationID(IDCollectionKey key,
                                 const std::string& trial_name,
                                 const std::string& group_name) {
  return GetGoogleVariationIDFromHashes(
      key, MakeActiveGroupId(trial_name, group_name));
}

VariationID GetGoogleVariationIDFromHashes(
    IDCollectionKey key,
    const ActiveGroupId& active_group) {
  return GroupMapAccessor::GetInstance()->GetID(key, active_group);
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void ClearAllVariationIDs() {
  GroupMapAccessor::GetInstance()->ClearAllMapsForTesting();
}

void ClearAllVariationParams() {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

}  // namespace testing

}  // namespace variations
