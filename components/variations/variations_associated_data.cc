// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "components/variations/variations_http_header_provider.h"

namespace variations {

namespace {

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  typedef std::map<ActiveGroupId, VariationID, ActiveGroupIdCompare>
      GroupToIDMap;

  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return base::Singleton<GroupMapAccessor>::get();
  }

  // Note that this normally only sets the ID for a group the first time, unless
  // |force| is set to true, in which case it will always override it.
  void AssociateID(IDCollectionKey key,
                   const ActiveGroupId& group_identifier,
                   const VariationID id,
                   const bool force) {
    static_assert(3 == ID_COLLECTION_COUNT,
                  "If you add a new collection key, add handling code here!");
#if DCHECK_IS_ON()
    // Ensure that at most one of the trigger/non-trigger/signed-in web property
    // IDs are set.
    if (key == GOOGLE_WEB_PROPERTIES || key == GOOGLE_WEB_PROPERTIES_TRIGGER ||
        key == GOOGLE_WEB_PROPERTIES_SIGNED_IN) {
      if (key != GOOGLE_WEB_PROPERTIES)
        DCHECK_EQ(EMPTY_ID, GetID(GOOGLE_WEB_PROPERTIES, group_identifier));
      if (key != GOOGLE_WEB_PROPERTIES_TRIGGER) {
        DCHECK_EQ(EMPTY_ID,
                  GetID(GOOGLE_WEB_PROPERTIES_TRIGGER, group_identifier));
      }
      if (key != GOOGLE_WEB_PROPERTIES_SIGNED_IN) {
        DCHECK_EQ(EMPTY_ID,
                  GetID(GOOGLE_WEB_PROPERTIES_SIGNED_IN, group_identifier));
      }
    }

    // Validate that all collections with this |group_identifier| have the same
    // associated ID.
    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      IDCollectionKey other_key = static_cast<IDCollectionKey>(i);
      if (other_key == key)
        continue;
      VariationID other_id = GetID(other_key, group_identifier);
      DCHECK(other_id == EMPTY_ID || other_id == id);
    }
#endif

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
  ~GroupMapAccessor() {}

  base::Lock lock_;
  std::vector<GroupToIDMap> group_to_id_maps_;

  DISALLOW_COPY_AND_ASSIGN(GroupMapAccessor);
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

bool AssociateVariationParams(
    const std::string& trial_name,
    const std::string& group_name,
    const std::map<std::string, std::string>& params) {
  return base::AssociateFieldTrialParams(trial_name, group_name, params);
}

bool GetVariationParams(const std::string& trial_name,
                        std::map<std::string, std::string>* params) {
  return base::GetFieldTrialParams(trial_name, params);
}

bool GetVariationParamsByFeature(const base::Feature& feature,
                                 std::map<std::string, std::string>* params) {
  return base::GetFieldTrialParamsByFeature(feature, params);
}

std::string GetVariationParamValue(const std::string& trial_name,
                                   const std::string& param_name) {
  return base::GetFieldTrialParamValue(trial_name, param_name);
}

std::string GetVariationParamValueByFeature(const base::Feature& feature,
                                            const std::string& param_name) {
  return base::GetFieldTrialParamValueByFeature(feature, param_name);
}

int GetVariationParamByFeatureAsInt(const base::Feature& feature,
                                    const std::string& param_name,
                                    int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(feature, param_name,
                                                default_value);
}

double GetVariationParamByFeatureAsDouble(const base::Feature& feature,
                                          const std::string& param_name,
                                          double default_value) {
  return base::GetFieldTrialParamByFeatureAsDouble(feature, param_name,
                                                   default_value);
}

bool GetVariationParamByFeatureAsBool(const base::Feature& feature,
                                      const std::string& param_name,
                                      bool default_value) {
  return base::GetFieldTrialParamByFeatureAsBool(feature, param_name,
                                                 default_value);
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
