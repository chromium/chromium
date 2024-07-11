// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_util.h"

#include <memory>

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
                     SettingsData* settings_data) {
    if (proto_setting.settings_id() == settings_data->pref_name &&
        proto_setting.has_value()) {
      if (proto_setting.type() == proto::SETTING_TYPE_BOOL) {
        return (proto_setting.value().has_bool_val() &&
                proto_setting.value().bool_val() ==
                    settings_data->value->GetBool());
      } else if (proto_setting.type() == proto::SETTING_TYPE_STRING) {
        return (proto_setting.value().has_text_val() &&
                (proto_setting.value().text_val() ==
                 settings_data->value->GetString()));
      } else if (proto_setting.type() == proto::SETTING_TYPE_INTEGER) {
        return (proto_setting.value().has_int_val() &&
                (proto_setting.value().int_val() ==
                 settings_data->value->GetInt()));
      } else if (proto_setting.type() == proto::SETTING_TYPE_DOUBLE) {
        EXPECT_DOUBLE_EQ(proto_setting.value().double_val(),
                         settings_data->value->GetDouble());
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
  auto cpu_data = std::make_unique<CpuData>(40, 60, 5.0);
  auto memory_data = std::make_unique<MemoryData>(4.0, 8.0);
  auto battery_data =
      std::make_unique<BatteryData>(158, 76, "36 minutes until full", 80);
  std::unique_ptr<DiagnosticsData> diagnostics_data =
      std::make_unique<DiagnosticsData>(
          std::move(battery_data), std::move(cpu_data), std::move(memory_data));
  proto::SparkyContextData sparky_context_data;
  auto* diagnostics_proto = sparky_context_data.mutable_diagnostics_data();
  AddDiagnosticsProto(std::move(diagnostics_data), diagnostics_proto);
  ASSERT_TRUE(diagnostics_proto->has_battery());
  ASSERT_TRUE(diagnostics_proto->has_cpu());
  ASSERT_TRUE(diagnostics_proto->has_memory());
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

}  // namespace manta
