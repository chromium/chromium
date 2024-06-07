// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_util.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/manta/proto/sparky.pb.h"
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

static constexpr auto diagnostic_map =
    base::MakeFixedFlatMap<manta::proto::Diagnostics, Diagnostics>(
        {{manta::proto::Diagnostics::DIAGNOSTICS_BATTERY,
          Diagnostics::kBattery},
         {manta::proto::Diagnostics::DIAGNOSTICS_CPU, Diagnostics::kCpu},
         {manta::proto::Diagnostics::DIAGNOSTICS_STORAGE,
          Diagnostics::kStorage},
         {manta::proto::Diagnostics::DIAGNOSTICS_MEMORY,
          Diagnostics::kMemory}});

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

std::vector<Diagnostics> COMPONENT_EXPORT(MANTA)
    ObtainDiagnosticsVectorFromProto(
        const ::manta::proto::DiagnosticsRequest& diagnostics_request) {
  int number_of_diagnostics_requested = diagnostics_request.diagnostics_size();
  std::vector<Diagnostics> diagnostics_vector;
  for (int index = 0; index < number_of_diagnostics_requested; index++) {
    const auto iter =
        diagnostic_map.find(diagnostics_request.diagnostics(index));

    auto type = iter != diagnostic_map.end()
                    ? std::optional<Diagnostics>(iter->second)
                    : std::nullopt;
    if (type == std::nullopt) {
      DVLOG(1) << "Invalid diagnostics type";
      continue;
    } else {
      diagnostics_vector.emplace_back(type.value());
    }
  }
  return diagnostics_vector;
}

void COMPONENT_EXPORT(MANTA)
    AddDiagnosticsProto(std::unique_ptr<DiagnosticsData> diagnostics_data,
                        proto::DiagnosticsData* diagnostics_proto) {
  if (diagnostics_data) {
    if (diagnostics_data->cpu_data) {
      auto* cpu_proto = diagnostics_proto->mutable_cpu();
      cpu_proto->set_temperature(
          diagnostics_data->cpu_data->average_cpu_temp_celsius);
      cpu_proto->set_clock_speed_ghz(
          diagnostics_data->cpu_data->scaling_current_frequency_ghz);
      cpu_proto->set_cpu_usage_snapshot(
          diagnostics_data->cpu_data->cpu_usage_percentage_snapshot);
    }
    if (diagnostics_data->memory_data) {
      auto* memory_proto = diagnostics_proto->mutable_memory();
      memory_proto->set_free_ram_gb(
          diagnostics_data->memory_data->available_memory_gb);
      memory_proto->set_total_ram_gb(
          diagnostics_data->memory_data->total_memory_gb);
    }
    if (diagnostics_data->battery_data) {
      auto* battery_proto = diagnostics_proto->mutable_battery();
      battery_proto->set_battery_health(
          diagnostics_data->battery_data->battery_wear_percentage);
      battery_proto->set_battery_charge_percentage(
          diagnostics_data->battery_data->battery_percentage);
      battery_proto->set_cycle_count(
          diagnostics_data->battery_data->cycle_count);
      battery_proto->set_battery_time(
          diagnostics_data->battery_data->power_time);
    }
  }
}

}  // namespace manta
