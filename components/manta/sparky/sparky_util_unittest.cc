// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_util.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

class SparkyUtilTest : public testing::Test {
 public:
  SparkyUtilTest() = default;

  SparkyUtilTest(const SparkyUtilTest&) = delete;
  SparkyUtilTest& operator=(const SparkyUtilTest&) = delete;

  ~SparkyUtilTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  bool IsSameSetting(const proto::Setting& proto_setting,
                     const SettingsData* settings_data) {
    if (!settings_data->val_set) {
      return false;
    }
    if (proto_setting.settings_id() == settings_data->pref_name &&
        proto_setting.has_value()) {
      if (proto_setting.type() == proto::SETTING_TYPE_BOOL) {
        return (proto_setting.value().has_bool_val() &&
                proto_setting.value().bool_val() == settings_data->bool_val);
      } else if (proto_setting.type() == proto::SETTING_TYPE_STRING) {
        return (
            proto_setting.value().has_text_val() &&
            (proto_setting.value().text_val() == settings_data->string_val));
      } else if (proto_setting.type() == proto::SETTING_TYPE_INTEGER) {
        return (proto_setting.value().has_int_val() &&
                (proto_setting.value().int_val() == settings_data->int_val));
      } else if (proto_setting.type() == proto::SETTING_TYPE_DOUBLE) {
        EXPECT_DOUBLE_EQ(proto_setting.value().double_val(),
                         settings_data->double_val);
        return true;
      }
    }
    return false;  // Settings do not match.
  }

  bool ContainsSetting(
      const google::protobuf::RepeatedPtrField<proto::Setting>& repeatedField,
      SettingsData* settings_data) {
    for (const proto::Setting& proto_setting : repeatedField) {
      if (IsSameSetting(proto_setting, settings_data)) {
        return true;
      }
    }
    return false;  // Did not find the setting.
  }

  bool ContainsApp(
      const google::protobuf::RepeatedPtrField<proto::App>& repeated_field,
      std::string_view name,
      std::string_view id) {
    for (const proto::App& proto_app : repeated_field) {
      if (proto_app.id() == id && proto_app.name() == name) {
        return true;
      }
    }
    return false;
  }

  bool ContainsAction(const proto::Action& action_proto,
                      std::vector<Action>* actions) {
    for (const Action& action : *actions) {
      if (action.updated_setting.has_value() &&
          action_proto.has_update_setting()) {
        if (IsSameSetting(action_proto.update_setting(),
                          action.updated_setting.has_value()
                              ? &action.updated_setting.value()
                              : nullptr)) {
          return true;
        }
      } else if (action.type == ActionType::kLaunchApp &&
                 action_proto.has_launch_app_id()) {
        if (action.launched_app == action_proto.launch_app_id()) {
          return true;
        }
      } else if (action_proto.has_all_done() &&
                 action.type == ActionType::kAllDone) {
        return action_proto.all_done() == action.all_done;
      } else if (action_proto.has_click() && action_proto.click().has_x_pos() &&
                 action_proto.click().has_y_pos() &&
                 action.type == ActionType::kClick) {
        return action_proto.click().x_pos() == action.click->x_pos &&
               action_proto.click().y_pos() == action.click->y_pos;
      } else if (action_proto.has_text_entry() &&
                 action_proto.text_entry().has_text() &&
                 action.type == ActionType::kTextEntry) {
        return action_proto.text_entry().text() == action.text_entry;
      } else if (action_proto.has_launch_file() &&
                 action_proto.launch_file().has_launch_file_path() &&
                 action.type == ActionType::kLaunchFile) {
        return action_proto.launch_file().launch_file_path() ==
               action.launch_file->launch_file_path;
      }
    }
    return false;
  }

  bool ContainsDialog(const ::google::protobuf::RepeatedPtrField<
                          ::manta::proto::Turn>& dialog_repeated,
                      const std::string& dialog,
                      Role role,
                      std::vector<Action>* actions) {
    for (const proto::Turn& proto_dialog : dialog_repeated) {
      if (proto_dialog.message() == dialog &&
          proto_dialog.role() == GetRole(role)) {
        if (actions) {
          if ((int)actions->size() != proto_dialog.action_size()) {
            return false;
          }
          auto actions_proto = proto_dialog.action();
          for (const proto::Action& action_proto : actions_proto) {
            if (!ContainsAction(action_proto, actions)) {
              return false;
            }
          }
        }
        return true;
      }
    }
    return false;
  }

  std::optional<proto::File> ObtainFileProto(
      const google::protobuf::RepeatedPtrField<proto::File>& repeated_field,
      std::string file_path) {
    for (const proto::File& proto_file : repeated_field) {
      if (proto_file.path() == file_path) {
        return std::make_optional(proto_file);
      }
    }
    return std::nullopt;
  }
};

TEST_F(SparkyUtilTest, AddSettingsProto) {
  auto current_prefs = SparkyDelegate::SettingsDataList();
  current_prefs["ash.dark_mode.enabled"] = std::make_unique<SettingsData>(
      "ash.dark_mode.enabled", PrefType::kBoolean,
      std::make_optional<base::Value>(true));
  current_prefs["string_pref"] = std::make_unique<SettingsData>(
      "string_pref", PrefType::kString,
      std::make_optional<base::Value>("my string"));
  current_prefs["int_pref"] = std::make_unique<SettingsData>(
      "int_pref", PrefType::kInt, std::make_optional<base::Value>(1));
  current_prefs["ash.night_light.enabled"] = std::make_unique<SettingsData>(
      "ash.night_light.enabled", PrefType::kBoolean,
      std::make_optional<base::Value>(false));
  current_prefs["ash.night_light.color_temperature"] =
      std::make_unique<SettingsData>("ash.night_light.color_temperature",
                                     PrefType::kDouble,
                                     std::make_optional<base::Value>(0.1));
  proto::SparkyContextData sparky_context_data;
  manta::proto::SettingsData* settings_data =
      sparky_context_data.mutable_settings_data();
  AddSettingsProto(current_prefs, settings_data);
  auto settings = settings_data->setting();
  ASSERT_EQ(settings_data->setting_size(), 5);
  ASSERT_TRUE(
      ContainsSetting(settings, current_prefs["ash.dark_mode.enabled"].get()));
  ASSERT_TRUE(ContainsSetting(settings, current_prefs["string_pref"].get()));
  ASSERT_TRUE(ContainsSetting(settings,
                              current_prefs["ash.night_light.enabled"].get()));
  ASSERT_TRUE(ContainsSetting(
      settings, current_prefs["ash.night_light.color_temperature"].get()));
}

TEST_F(SparkyUtilTest, AddDiagnosticsProto) {
  auto cpu_data = std::make_optional<CpuData>(40, 60, 5.0);
  auto memory_data = std::make_optional<MemoryData>(4.0, 8.0);
  auto battery_data =
      std::make_optional<BatteryData>(158, 76, "36 minutes until full", 80);
  auto storage_data = std::make_optional<manta::StorageData>("78 GB", "128 GB");
  std::optional<DiagnosticsData> diagnostics_data =
      std::make_optional<DiagnosticsData>(
          std::move(battery_data), std::move(cpu_data), std::move(memory_data),
          std::move(storage_data));
  proto::SparkyContextData sparky_context_data;
  auto* diagnostics_proto = sparky_context_data.mutable_diagnostics_data();
  AddDiagnosticsProto(std::move(diagnostics_data), diagnostics_proto);
  ASSERT_TRUE(diagnostics_proto->has_battery());
  ASSERT_TRUE(diagnostics_proto->has_cpu());
  ASSERT_TRUE(diagnostics_proto->has_memory());
  ASSERT_TRUE(diagnostics_proto->has_storage());

  ASSERT_DOUBLE_EQ(diagnostics_proto->cpu().clock_speed_ghz(), 5.0);
  ASSERT_EQ(diagnostics_proto->cpu().cpu_usage_snapshot(), 40);
  ASSERT_EQ(diagnostics_proto->cpu().temperature(), 60);
  ASSERT_DOUBLE_EQ(diagnostics_proto->memory().free_ram_gb(), 4.0);
  ASSERT_DOUBLE_EQ(diagnostics_proto->memory().total_ram_gb(), 8.0);
  ASSERT_EQ(diagnostics_proto->battery().battery_health(), 76);
  ASSERT_EQ(diagnostics_proto->battery().battery_charge_percentage(), 80);
  ASSERT_EQ(diagnostics_proto->battery().cycle_count(), 158);
  ASSERT_EQ(diagnostics_proto->battery().battery_time(),
            "36 minutes until full");
  ASSERT_EQ(diagnostics_proto->storage().free_storage(), "78 GB");
  ASSERT_EQ(diagnostics_proto->storage().total_storage(), "128 GB");
}

TEST_F(SparkyUtilTest, AddAppsData) {
  std::vector<manta::AppsData> apps_data;
  manta::AppsData app1 = AppsData("name1", "id1");
  apps_data.emplace_back(std::move(app1));
  manta::AppsData app2 = AppsData("name2", "id2");
  app2.AddSearchableText("search_term1");
  app2.AddSearchableText("search_term2");
  apps_data.emplace_back(std::move(app2));

  proto::SparkyContextData sparky_context_data;
  manta::proto::AppsData* apps_proto = sparky_context_data.mutable_apps_data();
  AddAppsData(std::move(apps_data), apps_proto);
  auto apps = apps_proto->app();
  ASSERT_EQ(apps_proto->app_size(), 2);
  ASSERT_TRUE(ContainsApp(apps, "name2", "id2"));
  ASSERT_TRUE(ContainsApp(apps, "name1", "id1"));
}

TEST_F(SparkyUtilTest, ObtainSettingFromProto) {
  proto::Setting bool_setting_proto;
  bool_setting_proto.set_type(proto::SETTING_TYPE_BOOL);
  bool_setting_proto.set_settings_id("power.adaptive_charging_enabled");
  auto* bool_settings_value = bool_setting_proto.mutable_value();
  bool_settings_value->set_bool_val(true);
  std::unique_ptr<SettingsData> bool_settings_data =
      ObtainSettingFromProto(bool_setting_proto);
  ASSERT_TRUE(IsSameSetting(bool_setting_proto, bool_settings_data.get()));

  proto::Setting int_setting_proto;
  int_setting_proto.set_type(proto::SETTING_TYPE_INTEGER);
  int_setting_proto.set_settings_id("ash.int.setting");
  auto* int_settings_value = int_setting_proto.mutable_value();
  int_settings_value->set_int_val(2);
  std::unique_ptr<SettingsData> int_settings_data =
      ObtainSettingFromProto(int_setting_proto);
  ASSERT_TRUE(IsSameSetting(int_setting_proto, int_settings_data.get()));

  proto::Setting double_setting_proto;
  double_setting_proto.set_type(proto::SETTING_TYPE_DOUBLE);
  double_setting_proto.set_settings_id("ash.night_light.color_temperature");
  auto* double_settings_value = double_setting_proto.mutable_value();
  double_settings_value->set_double_val(0.5);
  std::unique_ptr<SettingsData> double_settings_data =
      ObtainSettingFromProto(double_setting_proto);
  ASSERT_TRUE(IsSameSetting(double_setting_proto, double_settings_data.get()));

  proto::Setting string_setting_proto;
  string_setting_proto.set_type(proto::SETTING_TYPE_STRING);
  string_setting_proto.set_settings_id("ash.string.setting");
  auto* string_settings_value = string_setting_proto.mutable_value();
  string_settings_value->set_text_val("my string");
  std::unique_ptr<SettingsData> string_settings_data =
      ObtainSettingFromProto(string_setting_proto);
  ASSERT_TRUE(IsSameSetting(string_setting_proto, string_settings_data.get()));
}

TEST_F(SparkyUtilTest, AddFilesData) {
  std::vector<manta::FileData> files_data;
  auto file_1 = FileData("path1", "name1", "2024");
  file_1.summary = "file 1 summary";
  file_1.bytes =
      std::make_optional(std::vector<uint8_t>({2, 4, 6, 7, 4, 7, 2, 8}));
  file_1.size_in_bytes = 8L;

  files_data.emplace_back(file_1);
  files_data.emplace_back("path2", "name2", "2023");

  proto::SparkyContextData sparky_context_data;
  manta::proto::FilesData* files_proto =
      sparky_context_data.mutable_files_data();
  AddFilesData(std::move(files_data), files_proto);
  auto files = files_proto->files();
  ASSERT_EQ(files_proto->files_size(), 2);
  std::optional<proto::File> proto_file_1 = ObtainFileProto(files, "path1");
  ASSERT_TRUE(proto_file_1.has_value());
  ASSERT_EQ(proto_file_1->name(), "name1");
  ASSERT_EQ(proto_file_1->date_modified(), "2024");
  ASSERT_EQ(proto_file_1->serialized_bytes(), "\x2\x4\x6\a\x4\a\x2\b");
  ASSERT_EQ(proto_file_1->summary(), "file 1 summary");
  std::optional<proto::File> proto_file_2 = ObtainFileProto(files, "path2");
  ASSERT_TRUE(proto_file_2.has_value());
  ASSERT_EQ(proto_file_2->name(), "name2");
  ASSERT_EQ(proto_file_2->date_modified(), "2023");
}

TEST_F(SparkyUtilTest, GetSelectedFilePaths) {
  proto::FileRequest file_request;

  std::set<std::string> empty_set = GetSelectedFilePaths(file_request);
  ASSERT_TRUE(empty_set.empty());

  file_request.add_paths("my/file/path");
  file_request.add_paths("my/second/file/path/");
  std::set<std::string> file_set = GetSelectedFilePaths(file_request);
  ASSERT_EQ(file_set.size(), 2u);
  ASSERT_TRUE(file_set.contains("my/file/path"));
  ASSERT_TRUE(file_set.contains("my/second/file/path/"));
}

TEST_F(SparkyUtilTest, GetFileDataFromProto) {
  manta::proto::FilesData files_proto;
  std::vector<FileData> empty_files_data = GetFileDataFromProto(files_proto);
  EXPECT_TRUE(empty_files_data.empty());

  manta::proto::File* file1 = files_proto.add_files();
  file1->set_name("name1");
  file1->set_path("my/file/path");
  file1->set_summary("this file is a picture of a cat");
  file1->set_date_modified("2024");
  file1->set_size_in_bytes(823);

  manta::proto::File* file2 = files_proto.add_files();
  file2->set_name("name2");
  file2->set_path("my/second/file/path/");
  file2->set_summary("this file is a poem about a cat");
  file2->set_date_modified("2023");
  file2->set_size_in_bytes(94);

  std::vector<FileData> files_data = GetFileDataFromProto(files_proto);
  ASSERT_EQ(files_data.size(), 2u);
  auto file1_data = files_data.at(0);
  ASSERT_EQ(file1_data.name, "name1");
  ASSERT_EQ(file1_data.path, "my/file/path");
  ASSERT_EQ(file1_data.summary, "this file is a picture of a cat");
  ASSERT_EQ(file1_data.date_modified, "2024");
  ASSERT_EQ(file1_data.size_in_bytes, 823);
}

}  // namespace manta
