// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/preferences_merge_helper.h"

#include "base/containers/contains.h"
#include "components/sync_preferences/pref_model_associator_client.h"

namespace sync_preferences::helper {

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
                            const std::string& pref_name,
                            const base::Value& local_value,
                            const base::Value& server_value) {
  if (client) {
    if (client->IsMergeableListPreference(pref_name)) {
      if (local_value.is_none()) {
        return server_value.Clone();
      }
      if (server_value.is_none()) {
        return local_value.Clone();
      }
      return base::Value(
          MergeListValues(local_value.GetList(), server_value.GetList()));
    }
    if (client->IsMergeableDictionaryPreference(pref_name)) {
      if (local_value.is_none()) {
        return server_value.Clone();
      }
      if (server_value.is_none()) {
        return local_value.Clone();
      }
      return base::Value(
          MergeDictionaryValues(local_value.GetDict(), server_value.GetDict()));
    }
    base::Value merged_value = client->MaybeMergePreferenceValues(
        pref_name, local_value, server_value);
    if (!merged_value.is_none()) {
      return merged_value;
    }
  }

  // If this is not a specially handled preference, server wins.
  return server_value.Clone();
}

}  // namespace sync_preferences::helper
