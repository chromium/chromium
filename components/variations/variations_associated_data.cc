// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"

namespace variations {
namespace {

// The internal singleton accessor for the map, used to keep it thread-safe.
class GroupMapAccessor {
 public:
  struct VariationEntry {
    VariationID id;
    TimeWindow time_window;
  };

  typedef std::map<ActiveGroupId, VariationEntry, ActiveGroupIdCompare>
      GroupToIDMap;

  // Retrieve the singleton.
  static GroupMapAccessor* GetInstance() {
    return base::Singleton<GroupMapAccessor,
                           base::LeakySingletonTraits<GroupMapAccessor>>::get();
  }

  GroupMapAccessor(const GroupMapAccessor&) = delete;
  GroupMapAccessor& operator=(const GroupMapAccessor&) = delete;

  // Validates the specified mapping. A `group_identifier` is allowed to only
  // have one `key`, except in the case of `GOOGLE_APP`. A `GOOGLE_APP` key may
  // coexist with another provided they have the same `id` value.
  bool ValidateID(IDCollectionKey key,
                  ActiveGroupId group_identifier,
                  VariationID id) {
    static_assert(ID_COLLECTION_COUNT == 6,
                  "If you add a new collection key, add handling code here!");
    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      IDCollectionKey other_key = static_cast<IDCollectionKey>(i);
      if (key == other_key) {
        continue;
      }

      VariationID other_id = GetID(other_key, group_identifier);
      if (other_id == EMPTY_ID) {
        continue;
      }

      // In the case of a GOOGLE_APP key, another entry is allowed provided that
      // the IDs match.
      if (key == GOOGLE_APP || other_key == GOOGLE_APP) {
        if (other_id != id) {
          return false;
        }
        continue;
      }

      // `group_identifier` may not be registered under multiple non-GOOGLE_APP
      // IDCollectionKey values.
      return false;
    }
    return true;
  }

  void AssociateID(IDCollectionKey key,
                   ActiveGroupId group_identifier,
                   VariationID id,
                   TimeWindow time_window) {
    if (!ValidateID(key, group_identifier, id)) {
      // In the case of a validation failure, omit the association and instead
      // report the issue via a crash dump, to notify us of a problem with the
      // server-side configs.
      SCOPED_CRASH_KEY_NUMBER("InvalidVariationsID", "id", id);
      base::debug::DumpWithoutCrashing();
      return;
    }

    base::AutoLock scoped_lock(lock_);

    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    (*group_to_id_map)[group_identifier] = {id, time_window};
  }

  VariationID GetID(IDCollectionKey key,
                    ActiveGroupId group_identifier,
                    std::optional<base::Time> current_time = std::nullopt) {
    base::AutoLock scoped_lock(lock_);
    GroupToIDMap* group_to_id_map = GetGroupToIDMap(key);
    GroupToIDMap::const_iterator it = group_to_id_map->find(group_identifier);
    if (it == group_to_id_map->end() ||
        (current_time.has_value() &&
         !it->second.time_window.Contains(*current_time))) {
      return EMPTY_ID;
    }
    return it->second.id;
  }

  void ClearAllMapsForTesting() {
    base::AutoLock scoped_lock(lock_);

    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      GroupToIDMap* map = GetGroupToIDMap(static_cast<IDCollectionKey>(i));
      DCHECK(map);
      map->clear();
    }
  }

  base::Time GetNextTimeWindowEvent(base::Time time) const {
    base::AutoLock scoped_lock(lock_);
    base::Time next_event = base::Time::Max();
    // This double loop is O(N) where N is the number of field trials having an
    // associated variations ID, which should be in the order of 10s at most.
    for (const auto& id_map : group_to_id_maps_) {
      for (const auto& [id, entry] : id_map) {
        // Update the next time window event if the start or end time is after
        // `time`  but also before `next_event`.
        if (entry.time_window.start() > time &&
            entry.time_window.start() < next_event) {
          next_event = entry.time_window.start();
        }
        if (entry.time_window.end() > time &&
            entry.time_window.end() < next_event) {
          next_event = entry.time_window.end();
        }
      }
    }
    return next_event;
  }

 private:
  friend struct base::DefaultSingletonTraits<GroupMapAccessor>;

  // Retrieves the GroupToIDMap for `key`.
  GroupToIDMap* GetGroupToIDMap(IDCollectionKey key) {
    return &group_to_id_maps_[key];
  }

  GroupMapAccessor() { group_to_id_maps_.resize(ID_COLLECTION_COUNT); }
  ~GroupMapAccessor() = default;

  mutable base::Lock lock_;
  std::vector<GroupToIDMap> group_to_id_maps_;
};

}  // namespace

void AssociateGoogleVariationID(base::PassKey<VariationsSeedProcessor> pass_key,
                                IDCollectionKey key,
                                ActiveGroupId active_group_id,
                                VariationID variation_id,
                                TimeWindow time_window) {
  GroupMapAccessor::GetInstance()->AssociateID(key, active_group_id,
                                               variation_id, time_window);
}

void AssociateGoogleVariationID(base::PassKey<SyntheticTrialRegistry> pass_key,
                                IDCollectionKey key,
                                ActiveGroupId active_group_id,
                                VariationID variation_id,
                                TimeWindow time_window) {
  GroupMapAccessor::GetInstance()->AssociateID(key, active_group_id,
                                               variation_id, time_window);
}

void AssociateGoogleVariationIDForTesting(IDCollectionKey key,
                                          std::string_view trial_name,
                                          std::string_view group_name,
                                          VariationID variation_id,
                                          TimeWindow time_window) {
  GroupMapAccessor::GetInstance()->AssociateID(
      key, MakeActiveGroupId(trial_name, group_name), variation_id,
      time_window);
}

VariationID GetGoogleVariationID(IDCollectionKey key,
                                 std::string_view trial_name,
                                 std::string_view group_name,
                                 std::optional<base::Time> current_time) {
  return GetGoogleVariationID(
      key, MakeActiveGroupId(trial_name, group_name), current_time);
}

VariationID GetGoogleVariationID(
    IDCollectionKey key,
    ActiveGroupId active_group_id,
    std::optional<base::Time> current_time) {
  return GroupMapAccessor::GetInstance()->GetID(key, active_group_id,
                                                current_time);
}

base::Time GetNextTimeWindowEvent(base::Time current_time) {
  return GroupMapAccessor::GetInstance()->GetNextTimeWindowEvent(current_time);
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace test {

void ClearAllVariationIDs() {
  GroupMapAccessor::GetInstance()->ClearAllMapsForTesting();
}

void ClearAllVariationParams() {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}
}  // namespace test
}  // namespace variations
