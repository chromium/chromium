// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data_prefs_delegate.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/key_util.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace metrics::structured {

KeyDataPrefsDelegate::KeyDataPrefsDelegate(PrefService* local_state,
                                           std::string_view pref_name)
    : local_state_(local_state), pref_name_(pref_name) {
  CHECK(local_state_);
  CHECK(!pref_name_.empty());

  LoadKeysFromPrefs();
}

KeyDataPrefsDelegate::~KeyDataPrefsDelegate() = default;

bool KeyDataPrefsDelegate::IsReady() const {
  return true;
}

const KeyProto* KeyDataPrefsDelegate::GetKey(uint64_t project_name_hash) const {
  CHECK(IsReady());
  auto it = proto_.keys().find(project_name_hash);
  if (it != proto_.keys().end()) {
    return &it->second;
  }
  return nullptr;
}

void KeyDataPrefsDelegate::UpsertKey(uint64_t project_name_hash,
                                     base::TimeDelta last_key_rotation,
                                     base::TimeDelta key_rotation_period) {
  KeyProto& key_proto = (*(proto_.mutable_keys()))[project_name_hash];
  key_proto.set_key(util::GenerateNewKey());
  key_proto.set_last_rotation(last_key_rotation.InDays());
  key_proto.set_rotation_period(key_rotation_period.InDays());

  UpdatePrefsByProject(project_name_hash, key_proto);
}

void KeyDataPrefsDelegate::Purge() {
  // Clears in-memory keys.
  proto_.mutable_keys()->clear();

  // Clears persisted keys.
  local_state_->ClearPref(pref_name_);
}

void KeyDataPrefsDelegate::LoadKeysFromPrefs() {
  const base::Value::Dict& keys_pref = local_state_->GetDict(pref_name_);

  // Use the validators to get the project name to project hash mapping.
  const validator::Validators* validators = validator::Validators::Get();

  auto* proto_keys = proto_.mutable_keys();

  for (const auto [project_name, project_keys] : keys_pref) {
    std::optional<const ProjectValidator*> project_validator =
        validators->GetProjectValidator(project_name);

    // Check if a project was found for the name.
    if (!project_validator.has_value()) {
      continue;
    }

    const uint64_t project_hash = (*project_validator)->project_hash();
    const base::Value::Dict* value_dict = project_keys.GetIfDict();
    if (!value_dict) {
      LOG(ERROR) << "Key Pref value was expected to be a dict.";
      continue;
    }

    std::optional<KeyProto> key_data =
        util::CreateKeyProtoFromValue(*value_dict);

    if (!key_data.has_value()) {
      LOG(ERROR) << "Failed to convert pref value into key data.";
      continue;
    }

    (*proto_keys)[project_hash] = *key_data;
  }
}

void KeyDataPrefsDelegate::UpdatePrefsByProject(uint64_t project_name_hash,
                                                const KeyProto& key_proto) {
  ScopedDictPrefUpdate pref_updater(local_state_, pref_name_);

  base::Value::Dict& dict = pref_updater.Get();

  // Get the name of the project for |project_name_hash| to be used to store the
  // keys in prefs.
  const validator::Validators* validators = validator::Validators::Get();
  std::optional<std::string_view> project_name =
      validators->GetProjectName(project_name_hash);

  if (!project_name.has_value()) {
    LOG(ERROR) << "Attempting to store key for invalid project: "
               << project_name_hash;
    return;
  }

  dict.Set(*project_name, util::CreateValueFromKeyProto(key_proto));
}

}  // namespace metrics::structured
