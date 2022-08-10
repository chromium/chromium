// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/headless_script_controller_impl.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/cud_condition.pb.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/headless/client_headless.h"
#include "components/autofill_assistant/browser/mock_autofill_assistant_tts_controller.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/mock_controller_observer.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/mock_runtime_manager.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/starter.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/ukm_test_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

const char kExampleDeeplink[] = "https://www.example.com";

class HeadlessScriptControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    mock_runtime_manager_ = std::make_unique<MockRuntimeManager>();

    starter_ = std::make_unique<Starter>(
        web_contents(), fake_platform_delegate_.GetWeakPtr(), &ukm_recorder_,
        mock_runtime_manager_->GetWeakPtr(),
        task_environment_.GetMockTickClock());

    mock_service_to_inject_ = std::make_unique<NiceMock<MockService>>();
    mock_service_ = mock_service_to_inject_.get();
    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, GetScriptsForUrl)
        .WillByDefault(RunOnceCallback<2>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, GetActions)
        .WillByDefault(RunOnceCallback<5>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));
    ON_CALL(*mock_service_, GetNextActions)
        .WillByDefault(RunOnceCallback<6>(
            net::HTTP_OK, "", ServiceRequestSender::ResponseInfo{}));

    mock_web_controller_to_inject_ =
        std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = mock_web_controller_to_inject_.get();
    ON_CALL(*mock_web_controller_, FindElement)
        .WillByDefault(RunOnceCallback<2>(ClientStatus(), nullptr));

    auto client = std::make_unique<ClientHeadless>(
        web_contents_.get(), starter_->GetCommonDependencies(), nullptr,
        nullptr, task_environment_.GetMockTickClock(),
        mock_runtime_manager_->GetWeakPtr(), &ukm_recorder_, nullptr);
    headless_script_controller_ =
        std::make_unique<HeadlessScriptControllerImpl>(
            web_contents_.get(), starter_.get(), std::move(client));
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  // Note that calling this method moves |service_| and |web_controller_| so
  // it should not be called more than once per test.
  void Start(const base::flat_map<std::string, std::string>& params,
             bool expect_success) {
    // Since the callback is often called in a PostTask, we use this to make
    // sure the test does not fininsh before the callback is called.
    base::RunLoop run_loop;

    EXPECT_CALL(mock_script_ended_callback_, Run)
        .WillOnce([&run_loop, &expect_success](
                      HeadlessScriptController::ScriptResult result) {
          EXPECT_EQ(result.success, expect_success);
          run_loop.Quit();
        });
    headless_script_controller_->StartScript(
        params, mock_script_ended_callback_.Get(),
        /* use_autofill_assistant_onboarding = */ false, base::DoNothing(),
        std::move(mock_service_to_inject_),
        std::move(mock_web_controller_to_inject_));
    run_loop.Run();
  }

  void SetupScripts(SupportsScriptResponseProto scripts) {
    EXPECT_CALL(*mock_service_, GetScriptsForUrl)
        .WillOnce(RunOnceCallback<2>(net::HTTP_OK, scripts.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));
  }

  void SetupActionsForScript(const std::string& path,
                             ActionsResponseProto actions_response) {
    EXPECT_CALL(*mock_service_, GetActions(StrEq(path), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                     actions_response.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));
  }

  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path,
      bool direct_action = true) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    if (direct_action) {
      script->mutable_presentation()->mutable_direct_action()->add_names(
          name_and_path);
    }
    return script;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::MockCallback<
      base::OnceCallback<void(HeadlessScriptController::ScriptResult)>>
      mock_script_ended_callback_;
  FakeStarterPlatformDelegate fake_platform_delegate_;
  std::unique_ptr<MockRuntimeManager> mock_runtime_manager_;
  std::unique_ptr<Starter> starter_;
  std::unique_ptr<HeadlessScriptControllerImpl> headless_script_controller_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  raw_ptr<MockService> mock_service_;
  raw_ptr<MockWebController> mock_web_controller_;

 private:
  // These will be moved when the |Start| method is called, so expectations
  // should be written using |mock_service| and |mock_web_controller_| instead.
  std::unique_ptr<MockService> mock_service_to_inject_;
  std::unique_ptr<MockWebController> mock_web_controller_to_inject_;
};

TEST_F(HeadlessScriptControllerImplTest,
       StartFailsWithoutMandatoryScriptParameter) {
  // The startup will fail because we are missing the initial URL.
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"}, {"START_IMMEDIATELY", "true"}};
  Start(params, /* expect_success= */ false);
}

TEST_F(HeadlessScriptControllerImplTest, StartFailsIfNoScriptsAvailable) {
  EXPECT_CALL(*mock_service_, GetScriptsForUrl)
      .WillOnce(RunOnceCallback<2>(net::HTTP_OK, "",
                                   ServiceRequestSender::ResponseInfo{}));
  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ false);
}

TEST_F(HeadlessScriptControllerImplTest, SuccessfulRun) {
  SupportsScriptResponseProto script_response;
  auto* script = AddRunnableScript(&script_response, "script");
  script->mutable_presentation()->set_autostart(true);
  SetupScripts(script_response);

  ActionsResponseProto script_actions;
  script_actions.add_actions()->mutable_stop();
  SetupActionsForScript("script", script_actions);

  base::flat_map<std::string, std::string> params = {
      {"ENABLED", "true"},
      {"START_IMMEDIATELY", "true"},
      {"ORIGINAL_DEEPLINK", kExampleDeeplink}};
  Start(params, /* expect_success= */ true);
}

}  // namespace autofill_assistant
