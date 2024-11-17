// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/manta/base_provider.h"
#include "components/manta/base_provider_test_helper.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_context.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/sparky_util.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

namespace {
constexpr char kMockEndpoint[] = "https://my-endpoint.com";
}  // namespace

class FakeSparkyDelegate : public SparkyDelegate {
 public:
  FakeSparkyDelegate() {
    current_prefs_ = SettingsDataList();
    current_prefs_["ash.dark_mode.enabled"] = std::make_unique<SettingsData>(
        "ash.dark_mode.enabled", PrefType::kBoolean,
        std::make_optional<base::Value>(true));
    current_prefs_["power.adaptive_charging_enabled"] =
        std::make_unique<SettingsData>("power.adaptive_charging_enabled",
                                       PrefType::kBoolean,
                                       std::make_optional<base::Value>(false));
    current_prefs_["ash.night_light.enabled"] = std::make_unique<SettingsData>(
        "ash.night_light.enabled", PrefType::kBoolean,
        std::make_optional<base::Value>(false));
    current_prefs_["ash.night_light.color_temperature"] =
        std::make_unique<SettingsData>("ash.night_light.color_temperature",
                                       PrefType::kDouble,
                                       std::make_optional<base::Value>(0.1));
  }

  // manta::SparkyDelegate
  bool SetSettings(std::unique_ptr<SettingsData> settings_data) override {
    current_prefs_[settings_data->pref_name] = std::make_unique<SettingsData>(
        settings_data->pref_name, settings_data->pref_type,
        settings_data->GetValue());
    return true;
  }
  SettingsDataList* GetSettingsList() override {
    if (current_prefs_.empty()) {
      return nullptr;
    } else {
      return &current_prefs_;
    }
  }
  std::optional<base::Value> GetSettingValue(
      const std::string& setting_id) override {
    if (current_prefs_.contains(setting_id)) {
      return current_prefs_[setting_id]->GetValue();
    } else {
      return std::nullopt;
    }
  }
  void GetScreenshot(ScreenshotDataCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  std::vector<AppsData> GetAppsList() override { return {}; }
  void LaunchApp(const std::string& app_id) override {}
  void Click(int x, int y) override {}
  void KeyboardEntry(std::string text) override {}
  void KeyPress(const std::string& key,
                bool control,
                bool alt,
                bool shift) override {}
  void GetMyFiles(FilesDataCallback callback,
                  bool obtain_bytes,
                  std::set<std::string> allowed_file_paths) override {}
  void LaunchFile(const std::string& file_path) override {}
  void WriteFile(const std::string& name, const std::string& bytes) override {}

  void ObtainStorageInfo(StorageDataCallback storage_callback) override {
    std::move(storage_callback)
        .Run(std::make_unique<manta::StorageData>("78 GB", "128 GB"));
  }

  void UpdateFileSummaries(
      const std::vector<manta::FileData>& files_with_summary) override {}
  std::vector<manta::FileData> GetFileSummaries() override {
    std::vector<manta::FileData> files;
    return files;
  }

 private:
  SettingsDataList current_prefs_;
};

class FakeSystemInfoDelegate : public SystemInfoDelegate {
 public:
  FakeSystemInfoDelegate() = default;

  // TODO (b:340963863) Build out this fake component.
  // manta::SystemInfoDelegate
  void ObtainDiagnostics(
      const std::vector<manta::Diagnostics>& diagnostics,
      manta::DiagnosticsDataCallback diagnostics_callback) override {
    std::move(diagnostics_callback).Run(nullptr);
  }
};

class FakeSparkyProvider : public SparkyProvider, public FakeBaseProvider {
 public:
  FakeSparkyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory, identity_manager),
        SparkyProvider(test_url_loader_factory,
                       identity_manager,
                       std::make_unique<FakeSparkyDelegate>(),
                       std::make_unique<FakeSystemInfoDelegate>()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}

  std::optional<base::Value> CheckSettingValue(const std::string& setting_id) {
    return sparky_delegate_->GetSettingValue(setting_id)->Clone();
  }
};

class SparkyProviderTest : public BaseProviderTest {
 public:
  SparkyProviderTest() = default;

  SparkyProviderTest(const SparkyProviderTest&) = delete;
  SparkyProviderTest& operator=(const SparkyProviderTest&) = delete;

  ~SparkyProviderTest() override = default;

  std::unique_ptr<FakeSparkyProvider> CreateSparkyProvider() {
    return std::make_unique<FakeSparkyProvider>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_->identity_manager());
  }
};

// Test that string answer response is correctly passed to the callback.
TEST_F(SparkyProviderTest, SimpleRequestPayload) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::SparkyResponse& sparky_response =
      *output_data.mutable_sparky_response();
  auto* latest_reply = sparky_response.mutable_latest_reply();
  latest_reply->set_message("text answer");
  latest_reply->set_role(proto::ROLE_ASSISTANT);

  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();
  auto quit_closure = task_environment_.QuitClosure();

  proto::Turn new_turn = CreateTurn("When is it?", proto::Role::ROLE_USER);

  sparky_provider->QuestionAndAnswer(
      std::make_unique<SparkyContext>(new_turn, "page content"),
      base::BindLambdaForTesting(
          [&quit_closure](MantaStatus manta_status, proto::Turn* latest_turn) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_EQ("text answer", latest_turn->message());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

// Tests that the return string is empty if the returned proto does not contain
// a custom sparky response.
TEST_F(SparkyProviderTest, EmptyResponseIfSparkyDataIsNotSet) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  output_data.set_text("text response");

  std::string response_data;
  response.SerializeToString(&response_data);

  proto::Turn new_turn = CreateTurn("my question", proto::Role::ROLE_USER);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();
  auto quit_closure = task_environment_.QuitClosure();

  sparky_provider->QuestionAndAnswer(
      std::make_unique<SparkyContext>(new_turn, "page content"),
      base::BindLambdaForTesting([&quit_closure](MantaStatus manta_status,
                                                 proto::Turn* latest_turn) {
        ASSERT_EQ(manta_status.status_code, MantaStatusCode::kBlockedOutputs);
        ASSERT_FALSE(latest_turn);
        quit_closure.Run();
      }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

TEST_F(SparkyProviderTest, EmptyResponseAfterIdentityManagerShutdown) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();

  identity_test_env_.reset();

  proto::Turn new_turn = CreateTurn("my question", proto::Role::ROLE_USER);

  auto quit_closure = task_environment_.QuitClosure();

  sparky_provider->QuestionAndAnswer(
      std::make_unique<SparkyContext>(new_turn, "page content"),
      base::BindLambdaForTesting(
          [&quit_closure](MantaStatus manta_status, proto::Turn* latest_turn) {
            ASSERT_EQ(manta_status.status_code,
                      MantaStatusCode::kNoIdentityManager);
            ASSERT_FALSE(latest_turn);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // No metric logged.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    0);
}

// Test that setting actions can be executed if requested in the response.
TEST_F(SparkyProviderTest, SettingAction) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::SparkyResponse& sparky_response =
      *output_data.mutable_sparky_response();

  auto* latest_reply = sparky_response.mutable_latest_reply();
  latest_reply->set_message("text answer");
  latest_reply->set_role(proto::ROLE_ASSISTANT);
  auto* action = latest_reply->add_action();
  auto* setting_data = action->mutable_update_setting();
  setting_data->set_type(proto::SETTING_TYPE_BOOL);
  setting_data->set_settings_id("power.adaptive_charging_enabled");
  auto* settings_value = setting_data->mutable_value();
  settings_value->set_bool_val(true);

  std::string response_data;
  response.SerializeToString(&response_data);

  proto::Turn new_turn =
      CreateTurn("Turn on adaptive charging", proto::Role::ROLE_USER);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();

  auto quit_closure = task_environment_.QuitClosure();

  ASSERT_EQ(false, sparky_provider
                       ->CheckSettingValue("power.adaptive_charging_enabled")
                       ->GetBool());

  auto sparky_context =
      std::make_unique<SparkyContext>(new_turn, "page content");
  sparky_context->task = proto::Task::TASK_SETTINGS;

  sparky_provider->QuestionAndAnswer(
      std::move(sparky_context),
      base::BindLambdaForTesting(
          [&quit_closure](MantaStatus manta_status, proto::Turn* latest_turn) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_EQ("text answer", latest_turn->message());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  ASSERT_EQ(true, sparky_provider
                      ->CheckSettingValue("power.adaptive_charging_enabled")
                      ->GetBool());

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

// Test that sparky response with multiple actions is correctly executed and the
// final string is passed to the callback.
TEST_F(SparkyProviderTest, SettingActionWith2Actions) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::SparkyResponse& sparky_response =
      *output_data.mutable_sparky_response();

  auto* latest_reply = sparky_response.mutable_latest_reply();
  latest_reply->set_message("text answer");
  latest_reply->set_role(proto::ROLE_ASSISTANT);
  auto* action = latest_reply->add_action();
  auto* setting_data = action->mutable_update_setting();
  setting_data->set_type(proto::SETTING_TYPE_BOOL);
  setting_data->set_settings_id("ash.night_light.enabled");
  auto* settings_value = setting_data->mutable_value();
  settings_value->set_bool_val(true);
  auto* action2 = latest_reply->add_action();
  auto* double_setting = action2->mutable_update_setting();
  double_setting->set_type(proto::SETTING_TYPE_DOUBLE);
  double_setting->set_settings_id("ash.night_light.color_temperature");
  auto* double_value = double_setting->mutable_value();
  double_value->set_double_val(0.5);

  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();

  proto::Turn new_turn =
      CreateTurn("Turn on night light", proto::Role::ROLE_USER);
  auto quit_closure = task_environment_.QuitClosure();

  ASSERT_EQ(
      false,
      sparky_provider->CheckSettingValue("ash.night_light.enabled")->GetBool());
  ASSERT_EQ(0.1, sparky_provider
                     ->CheckSettingValue("ash.night_light.color_temperature")
                     ->GetDouble());

  auto sparky_context =
      std::make_unique<SparkyContext>(new_turn, "page content");
  sparky_context->task = proto::Task::TASK_SETTINGS;

  sparky_provider->QuestionAndAnswer(
      std::move(sparky_context),
      base::BindLambdaForTesting(
          [&quit_closure](MantaStatus manta_status, proto::Turn* latest_turn) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_EQ("text answer", latest_turn->message());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  ASSERT_EQ(
      true,
      sparky_provider->CheckSettingValue("ash.night_light.enabled")->GetBool());
  ASSERT_EQ(0.5, sparky_provider
                     ->CheckSettingValue("ash.night_light.color_temperature")
                     ->GetDouble());

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

// Test that the returned callback is empty if the settings are not defined
// correctly.
TEST_F(SparkyProviderTest, SettingActionInvalidProto) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  manta::proto::SparkyResponse& sparky_response =
      *output_data.mutable_sparky_response();

  auto* latest_reply = sparky_response.mutable_latest_reply();
  latest_reply->set_message("text answer");
  latest_reply->set_role(proto::ROLE_ASSISTANT);
  auto* action = latest_reply->add_action();
  auto* setting_data = action->mutable_update_setting();
  setting_data->set_type(proto::SETTING_TYPE_BOOL);
  setting_data->set_settings_id("power.adaptive_charging_enabled");
  auto* settings_value = setting_data->mutable_value();
  // Int value set for setting of type bool.
  settings_value->set_int_val(3);

  std::string response_data;
  response.SerializeToString(&response_data);

  proto::Turn new_turn =
      CreateTurn("Turn on adaptive charging", proto::Role::ROLE_USER);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();

  auto quit_closure = task_environment_.QuitClosure();
  auto sparky_context =
      std::make_unique<SparkyContext>(new_turn, "page content");
  sparky_context->task = proto::Task::TASK_SETTINGS;
  sparky_provider->QuestionAndAnswer(
      std::move(sparky_context),
      base::BindLambdaForTesting(
          [&quit_closure](MantaStatus manta_status, proto::Turn* latest_turn) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_FALSE(latest_turn);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

}  // namespace manta
