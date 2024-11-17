// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_util.h"

#include <memory>
#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"

namespace manta {
namespace {
using SettingType = proto::SettingType;

static constexpr auto pref_to_setting_map =
    base::MakeFixedFlatMap<PrefType, SettingType>({
        {PrefType::kBoolean, SettingType::SETTING_TYPE_BOOL},
        {PrefType::kString, SettingType::SETTING_TYPE_STRING},
        {PrefType::kDouble, SettingType::SETTING_TYPE_DOUBLE},
        {PrefType::kInt, SettingType::SETTING_TYPE_INTEGER},
    });

static constexpr auto setting_to_pref_map =
    base::MakeFixedFlatMap<SettingType, PrefType>({
        {SettingType::SETTING_TYPE_BOOL, PrefType::kBoolean},
        {SettingType::SETTING_TYPE_STRING, PrefType::kString},
        {SettingType::SETTING_TYPE_DOUBLE, PrefType::kDouble},
        {SettingType::SETTING_TYPE_INTEGER, PrefType::kInt},
    });

static constexpr auto role_to_proto_map =
    base::MakeFixedFlatMap<Role, proto::Role>({
        {Role::kAssistant, proto::Role::ROLE_ASSISTANT},
        {Role::kUser, proto::Role::ROLE_USER},
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
    std::optional<base::Value> value) {
  if (!value) {
    return std::nullopt;
  }
  const auto iter = pref_to_setting_map.find(pref_type);

  auto type = iter != pref_to_setting_map.end()
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

// Converts the setting proto into the pref type. Also verifies that the value
// is of the type specified.
std::optional<PrefType> VerifyValueAndConvertSettingTypeToPrefType(
    SettingType setting_type,
    const proto::SettingsValue& value) {
  const auto iter = setting_to_pref_map.find(setting_type);

  auto type = iter != setting_to_pref_map.end()
                  ? std::optional<PrefType>(iter->second)
                  : std::nullopt;
  if (!type.has_value()) {
    return std::nullopt;
  }
  if ((type.value() == PrefType::kBoolean && value.has_bool_val()) ||
      (type.value() == PrefType::kDouble && value.has_double_val()) ||
      (type.value() == PrefType::kInt && value.has_int_val()) ||
      (type.value() == PrefType::kString && value.has_text_val())) {
    return type;
  }

  return std::nullopt;
}

std::optional<base::Value> GetSettingsValue(proto::SettingsValue value,
                                            const PrefType& pref_type) {
  if (pref_type == PrefType::kBoolean) {
    return std::make_optional(base::Value(value.bool_val()));
  } else if (pref_type == PrefType::kInt) {
    return std::make_optional(base::Value(value.int_val()));
  } else if (pref_type == PrefType::kDouble) {
    return std::make_optional(base::Value(value.double_val()));
  } else if (pref_type == PrefType::kString) {
    return std::make_optional(base::Value(value.text_val()));
  } else {
    return std::nullopt;
  }
}

}  // namespace

LaunchFile::LaunchFile(std::string launch_file_path)
    : launch_file_path(launch_file_path) {}
LaunchFile::~LaunchFile() = default;

LaunchFile::LaunchFile(const LaunchFile&) = default;
LaunchFile& LaunchFile::operator=(const LaunchFile&) = default;

ClickAction::ClickAction(int x_pos, int y_pos) : x_pos(x_pos), y_pos(y_pos) {}
ClickAction::~ClickAction() = default;

ClickAction::ClickAction(const ClickAction&) = default;
ClickAction& ClickAction::operator=(const ClickAction&) = default;

Action::Action(SettingsData updated_setting)
    : updated_setting(std::make_optional(updated_setting)),
      type(ActionType::kSetting) {}
Action::Action(bool all_done)
    : type(ActionType::kAllDone), all_done(all_done) {}
Action::Action(ClickAction click)
    : click(std::make_optional(click)), type(ActionType::kClick) {}
Action::Action(ActionType type) : type(type) {}
Action::Action(LaunchFile launch_file, ActionType type)
    : launch_file(std::make_optional(launch_file)), type(type) {}
Action::~Action() = default;

Action::Action(const Action&) = default;
Action& Action::operator=(const Action&) = default;

proto::Role GetRole(Role role) {
  const auto iter = role_to_proto_map.find(role);
  return iter != role_to_proto_map.end() ? iter->second
                                         : proto::ROLE_UNSPECIFIED;
}

void AddSettingProto(const SettingsData& setting,
                     ::manta::proto::Setting* setting_proto,
                     SettingType setting_type) {
  setting_proto->set_type(setting_type);
  setting_proto->set_settings_id(setting.pref_name);
  auto* settings_value = setting_proto->mutable_value();
  if (setting.pref_type == PrefType::kBoolean) {
    settings_value->set_bool_val(setting.bool_val);
  } else if (setting.pref_type == PrefType::kDouble) {
    settings_value->set_double_val(setting.double_val);
  } else if (setting.pref_type == PrefType::kInt) {
    settings_value->set_int_val(setting.int_val);
  } else if (setting.pref_type == PrefType::kString) {
    settings_value->set_text_val(setting.string_val);
  }
}

void AddSettingsProto(const SparkyDelegate::SettingsDataList& settings_list,
                      ::manta::proto::SettingsData* settings_data) {
  for (auto const& [pref_name, setting] : settings_list) {
    auto setting_type = VerifyValueAndConvertPrefTypeToSettingType(
        setting->pref_type, setting->GetValue());
    if (setting_type == std::nullopt) {
      DVLOG(1) << "Invalid setting type for" << setting->pref_name;
      continue;
    }
    auto* setting_data = settings_data->add_setting();
    AddSettingProto(*setting, setting_data, setting_type.value());
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
    AddDiagnosticsProto(std::optional<DiagnosticsData> diagnostics_data,
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
    if (diagnostics_data->storage_data) {
      auto* storage_proto = diagnostics_proto->mutable_storage();
      storage_proto->set_free_storage(
          diagnostics_data->storage_data->free_bytes);
      storage_proto->set_total_storage(
          diagnostics_data->storage_data->total_bytes);
    }
  }
}

void AddAppsData(base::span<const AppsData> apps_data,
                 proto::AppsData* apps_proto) {
  for (const manta::AppsData& app : apps_data) {
    proto::App* app_proto = apps_proto->add_app();
    app_proto->set_id(app.id);
    app_proto->set_name(app.name);
    app_proto->mutable_searchable_term()->Add(app.searchable_text.begin(),
                                              app.searchable_text.end());
  }
}

void AddFilesData(base::span<const FileData> files_data,
                  proto::FilesData* files_proto) {
  for (const manta::FileData& file : files_data) {
    proto::File* file_proto = files_proto->add_files();
    file_proto->set_name(file.name);
    file_proto->set_path(file.path);
    file_proto->set_date_modified(file.date_modified);
    file_proto->set_size_in_bytes(file.size_in_bytes);
    if (file.bytes.has_value()) {
      file_proto->set_serialized_bytes(
          std::string(file.bytes->begin(), file.bytes->end()));
    }
    if (!file.summary.empty()) {
      file_proto->set_summary(file.summary);
    }
  }
}

std::unique_ptr<SettingsData> ObtainSettingFromProto(
    proto::Setting setting_proto) {
  auto pref_type = VerifyValueAndConvertSettingTypeToPrefType(
      setting_proto.type(), setting_proto.value());
  if (pref_type == std::nullopt) {
    return nullptr;
  }
  return std::make_unique<SettingsData>(
      setting_proto.settings_id(), *pref_type,
      GetSettingsValue(setting_proto.value(), *pref_type));
}

std::set<std::string> COMPONENT_EXPORT(MANTA)
    GetSelectedFilePaths(const proto::FileRequest& file_request) {
  std::set<std::string> set_file_paths;
  int file_size = file_request.paths_size();
  for (int index = 0; index < file_size; index++) {
    set_file_paths.emplace(file_request.paths(index));
  }
  return set_file_paths;
}

std::optional<FileData> GetFileFromProto(const proto::File& file_proto) {
  if (!file_proto.has_name() || !file_proto.has_path() ||
      !file_proto.has_date_modified() || !file_proto.has_size_in_bytes() ||
      !file_proto.has_summary()) {
    return std::nullopt;
  }
  auto file = std::make_optional<FileData>(file_proto.path(), file_proto.name(),
                                           file_proto.date_modified());
  file->summary = file_proto.summary();
  file->size_in_bytes = file_proto.size_in_bytes();
  if (file_proto.has_serialized_bytes()) {
    file->bytes = std::vector<uint8_t>(file_proto.serialized_bytes().begin(),
                                       file_proto.serialized_bytes().end());
  }
  return file;
}

std::vector<FileData> GetFileDataFromProto(
    const proto::FilesData& files_proto) {
  std::vector<FileData> files_data;
  int proto_file_count = files_proto.files_size();
  for (int index = 0; index < proto_file_count; ++index) {
    auto file = GetFileFromProto(files_proto.files(index));
    if (file.has_value()) {
      files_data.emplace_back(std::move(file.value()));
    }
  }
  return files_data;
}

proto::Turn CreateTurn(const std::string& message, manta::proto::Role role) {
  manta::proto::Turn turn;
  turn.set_message(message);
  turn.set_role(role);
  return turn;
}

}  // namespace manta
