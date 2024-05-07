// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_provider.h"

#include <memory>
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
    current_prefs_["dark_mode"] = std::make_unique<SettingsData>(
        "dark_mode", PrefType::kBoolean, std::make_optional<base::Value>(true));
    current_prefs_["adaptive_charging"] =
        std::make_unique<SettingsData>("adaptive_charging", PrefType::kBoolean,
                                       std::make_optional<base::Value>(false));
  }

  // manta::SparkyDelegate
  bool SetSettings(std::unique_ptr<SettingsData> settings_data) override {
    current_prefs_[settings_data->pref_name] = std::make_unique<SettingsData>(
        settings_data->pref_name, settings_data->pref_type,
        std::move(settings_data->value));
    return true;
  }

  SettingsDataList* GetSettingsList() override {
    if (current_prefs_.empty()) {
      return nullptr;
    } else {
      return &current_prefs_;
    }
  }

 private:
  SettingsDataList current_prefs_;
};

class FakeSparkyProvider : public SparkyProvider, public FakeBaseProvider {
 public:
  FakeSparkyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory,
      signin::IdentityManager* identity_manager)
      : BaseProvider(test_url_loader_factory,
                     identity_manager,
                     /*is_demo_mode=*/false),
        SparkyProvider(test_url_loader_factory,
                       identity_manager,
                       std::make_unique<FakeSparkyDelegate>()),
        FakeBaseProvider(test_url_loader_factory, identity_manager) {}
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
  manta::proto::Proto3Any& custom_response = *output_data.mutable_custom();
  custom_response.set_type_url(
      "type.googleapis.com/mdi.aretea.sparky_interaction.SparkyResponse");
  proto::SparkyResponse sparky_response;
  auto* final_response = sparky_response.mutable_final_response();
  final_response->set_answer("text answer");

  std::string serialized_sparky_response;
  sparky_response.SerializeToString(&serialized_sparky_response);
  custom_response.set_value(serialized_sparky_response);

  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();
  auto quit_closure = task_environment_.QuitClosure();
  auto qa_history = std::vector<FakeSparkyProvider::SparkyQAPair>({
      std::make_pair("Where is it?", "In Tokyo"),
      std::make_pair("When is it?", "In July"),
  });

  sparky_provider->QuestionAndAnswer(
      "page content", qa_history, "What is the climate like then",
      proto::Task::TASK_PLANNER,
      base::BindLambdaForTesting(
          [&quit_closure](const std::string& answer_string,
                          MantaStatus manta_status) {
            ASSERT_EQ(manta_status.status_code, MantaStatusCode::kOk);
            ASSERT_STREQ("text answer", answer_string.c_str());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // Metric is logged when response is successfully parsed.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    1);
}

// Tests that the return string is empty if the returned proto does not contain
// a custom sparky response.
TEST_F(SparkyProviderTest, EmptyResponseIfCustomIsNotSet) {
  base::HistogramTester histogram_tester;
  manta::proto::Response response;
  manta::proto::OutputData& output_data = *response.add_output_data();
  output_data.set_text("text response");

  std::string response_data;
  response.SerializeToString(&response_data);

  SetEndpointMockResponse(GURL{kMockEndpoint}, response_data, net::HTTP_OK,
                          net::OK);
  std::unique_ptr<FakeSparkyProvider> sparky_provider = CreateSparkyProvider();
  auto quit_closure = task_environment_.QuitClosure();

  sparky_provider->QuestionAndAnswer(
      "page content", std::vector<FakeSparkyProvider::SparkyQAPair>(),
      "my question", proto::Task::TASK_PLANNER,
      base::BindLambdaForTesting([&quit_closure](
                                     const std::string& answer_string,
                                     MantaStatus manta_status) {
        ASSERT_EQ(manta_status.status_code, MantaStatusCode::kBlockedOutputs);
        ASSERT_STREQ("", answer_string.c_str());
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

  auto quit_closure = task_environment_.QuitClosure();

  sparky_provider->QuestionAndAnswer(
      "page content", std::vector<FakeSparkyProvider::SparkyQAPair>(),
      "my question", proto::Task::TASK_PLANNER,
      base::BindLambdaForTesting(
          [&quit_closure](const std::string& answer_string,
                          MantaStatus manta_status) {
            ASSERT_EQ(manta_status.status_code,
                      MantaStatusCode::kNoIdentityManager);
            ASSERT_STREQ("", answer_string.c_str());
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();

  // No metric logged.
  histogram_tester.ExpectTotalCount("Ash.MantaService.SparkyProvider.TimeCost",
                                    0);
}

}  // namespace manta
