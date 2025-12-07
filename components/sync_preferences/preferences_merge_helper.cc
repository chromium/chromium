// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/preferences_merge_helper.h"

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/syncable_prefs_database.h"

namespace sync_preferences::helper {

namespace {

MergeBehavior GetMergeBehavior(const PrefModelAssociatorClient& client,
                               std::string_view pref_name) {
  std::optional<SyncablePrefMetadata> metadata =
      client.GetSyncablePrefsDatabase().GetSyncablePrefMetadata(pref_name);
  CHECK(metadata.has_value());
  return metadata->merge_behavior();
}

}  // namespace

base::Value::List MergeListValues(const base::Value::List& local_value,
                                  const base::Value::List& server_value) {
  base::Value::List result = server_value.Clone();
  for (const auto& value : local_value) {
    if (!base::Contains(result, value)) {
      result.Append(value.Clone());
    }
  }

  return result;
}

base::Value::Dict MergeDictionaryValues(const base::Value::Dict& local_value,
                                        const base::Value::Dict& server_value) {
  base::Value::Dict result = server_value.Clone();

  for (auto it : local_value) {
    // It's not clear whether using a C++17 structured binding here would cause
    // a copy of the value or not, so in doubt unpack the old way.
    const base::Value* local_key_value = &it.second;
    base::Value* server_key_value = result.Find(it.first);
    if (server_key_value) {
      if (local_key_value->is_dict() && server_key_value->is_dict()) {
        *server_key_value = base::Value(MergeDictionaryValues(
            local_key_value->GetDict(), server_key_value->GetDict()));
      }
      // Note that for all other types we want to preserve the "server"
      // values so we do nothing here.
    } else {
      result.Set(it.first, local_key_value->Clone());
    }
  }
  return result;
}

base::Value MergePreference(const PrefModelAssociatorClient* client,
                            std::string_view pref_name,
                            const base::Value& local_value,
                            const base::Value& server_value) {
  if (!client) {
    CHECK_IS_TEST();
    // No client was registered. Directly let server value win.
    return server_value.Clone();
  }

  switch (GetMergeBehavior(*client, pref_name)) {
    case MergeBehavior::kMergeableDict:
      if (!server_value.is_dict()) {
        // Server value is corrupt or missing, keep pref value unchanged.
        // TODO(crbug.com/40901973): Investigate in which scenarios can the
        // value be corrupt.
        return local_value.Clone();
      }
      // TODO(crbug.com/40933499): Investigate if this is valid or if this
      // should be a CHECK instead.
      if (!local_value.is_dict()) {
        return server_value.Clone();
      }
      return base::Value(
          MergeDictionaryValues(local_value.GetDict(), server_value.GetDict()));
    case MergeBehavior::kMergeableListWithRewriteOnUpdate:
      if (!server_value.is_list()) {
        // Server value is corrupt or missing, keep pref value unchanged.
        // TODO(crbug.com/40901973): Investigate in which scenarios can the
        // value be corrupt.
        return local_value.Clone();
      }
      // TODO(crbug.com/40933499): Investigate if this is valid or if this
      // should be a CHECK instead.
      if (!local_value.is_list()) {
        return server_value.Clone();
      }
      return base::Value(
          MergeListValues(local_value.GetList(), server_value.GetList()));
    case MergeBehavior::kCustom:
      if (base::Value merged_value = client->MaybeMergePreferenceValues(
              pref_name, local_value, server_value);
          !merged_value.is_none()) {
        return merged_value;
      }
      [[fallthrough]];
    case MergeBehavior::kNone:
      // If this is not a specially handled preference, server wins.
      return server_value.Clone();
  }
  NOTREACHED();
}

std::pair<base::Value::Dict, base::Value::Dict> UnmergeDictionaryValues(
    base::Value::Dict new_dict,
    const base::Value::Dict& original_local_dict,
    const base::Value::Dict& original_account_dict) {
  base::Value::Dict new_local_dict;
  base::Value::Dict new_account_dict;

  // Keep only keys that exist in the `new_dict`.
  for (auto [k, v] : original_local_dict) {
    if (new_dict.contains(k)) {
      new_local_dict.Set(k, v.Clone());
    }
  }
  for (auto [k, v] : original_account_dict) {
    if (new_dict.contains(k)) {
      new_account_dict.Set(k, v.Clone());
    }
  }

  // Add or update individual keys.
  for (auto [k, new_dict_value] : new_dict) {
    // If contained value is again a dict, recursively un-merge.
    if (new_dict_value.is_dict()) {
      base::Value local_dict_value(base::Value::Type::DICT);
      if (base::Value::Dict* local_dict_value_dict =
              new_local_dict.FindDict(k)) {
        local_dict_value = base::Value(std::move(*local_dict_value_dict));
      }
      base::Value account_dict_value(base::Value::Type::DICT);
      if (base::Value::Dict* account_dict_value_dict =
              new_account_dict.FindDict(k)) {
        account_dict_value = base::Value(std::move(*account_dict_value_dict));
      }
      // Note: Passing empty dict values in case the key does not exist or has a
      // different type.
      auto [local_v, account_v] = UnmergeDictionaryValues(
          std::move(new_dict_value).TakeDict(), local_dict_value.GetDict(),
          account_dict_value.GetDict());
      // If the new unmerged dict value is empty, remove the key.
      if (!local_v.empty()) {
        new_local_dict.Set(k, std::move(local_v));
      } else {
        new_local_dict.Remove(k);
      }
      // If the new unmerged dict value is empty, remove the key.
      if (!account_v.empty()) {
        new_account_dict.Set(k, std::move(account_v));
      } else {
        new_account_dict.Remove(k);
      }
    } else if (base::Value* account_dict_value = new_account_dict.Find(k)) {
      // The key already existed in the account store. Copy to the local store
      // if its value changed.
      if (*account_dict_value != new_dict_value) {
        new_local_dict.Set(k, new_dict_value.Clone());
        *account_dict_value = std::move(new_dict_value);
      }
    } else if (base::Value* local_dict_value = new_local_dict.Find(k)) {
      // The key already existed in the local store. Copy to the account store
      // if its value changed.
      if (*local_dict_value != new_dict_value) {
        new_account_dict.Set(k, new_dict_value.Clone());
        *local_dict_value = std::move(new_dict_value);
      }
    } else {
      // New dict entry - the key didn't exist before. Add it to both the
      // stores.
      new_account_dict.Set(k, new_dict_value.Clone());
      new_local_dict.Set(k, std::move(new_dict_value));
    }
  }
  return {std::move(new_local_dict), std::move(new_account_dict)};
}

}  // namespace sync_preferences::helper
