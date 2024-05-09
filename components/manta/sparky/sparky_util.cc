// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_util.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/manta/sparky/sparky_delegate.h"

namespace manta {
namespace {
using SettingType = proto::SettingType;

static constexpr auto setting_map =
    base::MakeFixedFlatMap<PrefType, SettingType>({
        {PrefType::kBoolean, SettingType::SETTING_TYPE_BOOL},
        {PrefType::kString, SettingType::SETTING_TYPE_STRING},
        {PrefType::kDouble, SettingType::SETTING_TYPE_DOUBLE},
        {PrefType::kInt, SettingType::SETTING_TYPE_INTEGER},
    });

// Gets the type of setting into the proto enum. Also verifies that the value is
// of the type specified.
std::optional<SettingType> VerifyValueAndConvertPrefTypeToSettingType(
    PrefType pref_type,
    base::Value* value) {
  if (!value) {
    return std::nullopt;
  }
  const auto iter = setting_map.find(pref_type);

  auto type = iter != setting_map.end()
                  ? std::optional<SettingType>(iter->second)
                  : std::nullopt;
  if (!type.has_value()) {
    return std::nullopt;
  }
  if ((pref_type == PrefType::kBoolean && value->is_bool()) ||
      (pref_type == PrefType::kDouble && value->is_double()) ||
      (pref_type == PrefType::kInt && value->is_int()) ||
      (pref_type == PrefType::kString && value->is_string())) {
    return type;
  }

  return std::nullopt;
}
}  // namespace

void AddSettingsProto(const SparkyDelegate::SettingsDataList& settings_list,
                      ::manta::proto::SettingsData* settings_data) {
  for (auto const& [pref_name, setting] : settings_list) {
    auto setting_type = VerifyValueAndConvertPrefTypeToSettingType(
        setting->pref_type,
        setting->value ? std::addressof(*setting->value) : nullptr);
    if (setting_type == std::nullopt) {
      DVLOG(1) << "Invalid setting type for" << pref_name;
      continue;
    }
    auto* setting_data = settings_data->add_setting();
    setting_data->set_type(setting_type.value());
    setting_data->set_settings_id(pref_name);
    auto* settings_value = setting_data->mutable_value();
    if (setting->pref_type == PrefType::kBoolean) {
      settings_value->set_bool_val(setting->value->GetBool());
    } else if (setting->pref_type == PrefType::kDouble) {
      settings_value->set_double_val(setting->value->GetDouble());
    } else if (setting->pref_type == PrefType::kInt) {
      settings_value->set_int_val(setting->value->GetInt());
    } else if (setting->pref_type == PrefType::kString) {
      settings_value->set_text_val(setting->value->GetString());
    }
  }
}

}  // namespace manta
