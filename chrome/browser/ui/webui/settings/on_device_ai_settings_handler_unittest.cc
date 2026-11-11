// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"

#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace {
using optimization_guide::OnDeviceBaseModel;
using optimization_guide::OnDeviceModelControl;
using optimization_guide::model_execution::prefs::localstate::
    kOnDeviceAiUserSettingsEnabled;

class FakeOnDeviceModelControl : public OnDeviceModelControl {
 public:
  FakeOnDeviceModelControl() = default;
  ~FakeOnDeviceModelControl() override = default;

  void SetInstalledModels(std::vector<InstalledModel> models) {
    models_ = std::move(models);
  }

  void UninstallModels() override { models_.clear(); }

  void UninstallCapability(OnDeviceBaseModel base_model) override {
    uninstalled_models_.push_back(base_model);
    std::erase_if(models_, [base_model](const InstalledModel& model) {
      return model.base_model == base_model;
    });
  }

  void GetInstalledModels(InstalledModelsCallback callback) override {
    std::move(callback).Run(models_);
  }

  const std::vector<OnDeviceBaseModel>& uninstalled_models() const {
    return uninstalled_models_;
  }

 private:
  std::vector<InstalledModel> models_;
  std::vector<OnDeviceBaseModel> uninstalled_models_;
};

class TestingOnDeviceAiSettingsHandler
    : public settings::OnDeviceAiSettingsHandler {
 public:
  using OnDeviceAiSettingsHandler::HandleDeleteOnDeviceAiCapability;
  using OnDeviceAiSettingsHandler::HandleGetOnDeviceAiCapabilities;
  using OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled;
  using OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled;
  using OnDeviceAiSettingsHandler::set_web_ui;

  void set_model_control(OnDeviceModelControl* model_control) {
    model_control_ = model_control;
  }

 protected:
  OnDeviceModelControl* GetModelControl() override { return model_control_; }

 private:
  raw_ptr<OnDeviceModelControl> model_control_ = nullptr;
};
}  // namespace

namespace settings {

class OnDeviceAiSettingsHandlerTest : public testing::Test {
 public:
  OnDeviceAiSettingsHandlerTest() {
    handler_ = std::make_unique<TestingOnDeviceAiSettingsHandler>();
    handler_->set_web_ui(&test_web_ui_);
  }

  void SetUp() override {
    ASSERT_EQ(local_state(), g_browser_process->local_state());
    EXPECT_TRUE(test_web_ui()->call_data().empty());

    // Initialize the handler and allow Javascript.
    base::ListValue args;
    args.Append(1);  // Callback ID
    handler()->HandleGetOnDeviceAiEnabled(args);

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    EXPECT_EQ(1u, test_web_ui()->call_data().size());

    test_web_ui()->ClearTrackedCalls();
  }

  void TearDown() override { handler_.reset(); }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }
  TestingOnDeviceAiSettingsHandler* handler() { return handler_.get(); }
  content::TestWebUI* test_web_ui() { return &test_web_ui_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingOnDeviceAiSettingsHandler> handler_;
};

TEST_F(OnDeviceAiSettingsHandlerTest, HandleSetOnDeviceAIEnabled) {
  // Ensure the pref is initially false for this test.
  local_state()->SetBoolean(kOnDeviceAiUserSettingsEnabled, false);

  base::ListValue args;
  args.Append(true);
  handler()->HandleSetOnDeviceAiEnabled(args);

  EXPECT_TRUE(local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       HandleSetOnDeviceAiEnabled_PolicyDisabled) {
  // Ensure the pref is initially false for this test.
  local_state()->SetBoolean(kOnDeviceAiUserSettingsEnabled, false);

  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));

  base::ListValue args;
  args.Append(true);
  handler()->HandleSetOnDeviceAiEnabled(args);

  EXPECT_FALSE(local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       HandleGetOnDeviceAiEnabled_PolicyDisabled) {
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));

  base::ListValue args;
  args.Append("callback_id");
  handler()->HandleGetOnDeviceAiEnabled(args);

  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback_id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());  // success
  const base::DictValue& result = call_data.arg3()->GetDict();
  EXPECT_FALSE(result.FindBool("allowedByPolicy").value());
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       PrefChangesNotifyPage_UserSettingEnabled) {
  // Toggle the pref.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("on-device-ai-enabled-changed", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetDict().FindBool("allowedByPolicy").value());

  test_web_ui()->ClearTrackedCalls();
  handler()->DisallowJavascript();

  // Toggle the pref again, while JavaScript is disabled.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_TRUE(test_web_ui()->call_data().empty());
}

TEST_F(OnDeviceAiSettingsHandlerTest, PrefChangesNotifyPage_PolicyDisabled) {
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));

  // Toggle the pref.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("on-device-ai-enabled-changed", call_data.arg1()->GetString());
  EXPECT_FALSE(call_data.arg2()->GetDict().FindBool("allowedByPolicy").value());
}

TEST_F(OnDeviceAiSettingsHandlerTest, GetCapabilities_NoModelControl) {
  handler()->set_model_control(nullptr);

  base::ListValue args;
  args.Append("callback_id");
  handler()->HandleGetOnDeviceAiCapabilities(args);

  ASSERT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback_id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());
  const base::DictValue& result = call_data.arg3()->GetDict();
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(base::ByteSize(0))),
            *result.FindString("totalSize"));
  EXPECT_TRUE(result.FindList("capabilities")->empty());
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       HandleGetOnDeviceAiCapabilities_FiltersHiddensAndSortsBySize) {
  FakeOnDeviceModelControl fake_control;
  handler()->set_model_control(&fake_control);

  const base::ByteSize kAsrSize = base::GiB(3);
  const base::ByteSize kNanoSize = base::GiB(1);
  const base::ByteSize kGemmaSize = base::GiB(2);

  fake_control.SetInstalledModels({
      OnDeviceModelControl::InstalledModel(
          OnDeviceBaseModel::kSpeechRecognitionSmallExpert, kAsrSize.InBytes()),
      // Unmapped models are excluded by GetOnDeviceModelUiMetadata().
      OnDeviceModelControl::InstalledModel(OnDeviceBaseModel::kClassifier,
                                           base::MiB(200).InBytes()),
      OnDeviceModelControl::InstalledModel(
          OnDeviceBaseModel::kProofreaderSmallExpert, base::MiB(300).InBytes()),
      OnDeviceModelControl::InstalledModel(OnDeviceBaseModel::kV3NanoGpu,
                                           kNanoSize.InBytes()),
      OnDeviceModelControl::InstalledModel(OnDeviceBaseModel::kGemma4,
                                           kGemmaSize.InBytes()),
  });

  base::ListValue args;
  args.Append("callback_id");
  handler()->HandleGetOnDeviceAiCapabilities(args);

  ASSERT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback_id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictValue& result = call_data.arg3()->GetDict();
  EXPECT_EQ(
      base::UTF16ToUTF8(ui::FormatBytes(kGemmaSize + kNanoSize + kAsrSize)),
      *result.FindString("totalSize"));

  const base::ListValue* capabilities = result.FindList("capabilities");
  ASSERT_NE(nullptr, capabilities);
  ASSERT_EQ(3u, capabilities->size());

  // Sorted by category first (GeneralPurpose before LiveTranscription), then
  // by size descending within each category: Gemma4 (2 GiB), V3NanoGpu (1 GiB),
  // ASR (3 GiB).
  const base::DictValue& row0 = (*capabilities)[0].GetDict();
  EXPECT_EQ(static_cast<int>(OnDeviceBaseModel::kGemma4), row0.FindInt("id"));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_TITLE),
            *row0.FindString("categoryTitle"));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_ON_DEVICE_MODEL_CATEGORY_GENERAL_PURPOSE_DESCRIPTION),
            *row0.FindString("description"));
  EXPECT_EQ("Gemma 4", *row0.FindString("modelName"));
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(kGemmaSize)),
            *row0.FindString("size"));

  const base::DictValue& row1 = (*capabilities)[1].GetDict();
  EXPECT_EQ(static_cast<int>(OnDeviceBaseModel::kV3NanoGpu),
            row1.FindInt("id"));
  EXPECT_EQ("Gemini Nano", *row1.FindString("modelName"));
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(kNanoSize)),
            *row1.FindString("size"));

  const base::DictValue& row2 = (*capabilities)[2].GetDict();
  EXPECT_EQ(static_cast<int>(OnDeviceBaseModel::kSpeechRecognitionSmallExpert),
            row2.FindInt("id"));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_TITLE),
            *row2.FindString("categoryTitle"));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_ON_DEVICE_MODEL_CATEGORY_LIVE_TRANSCRIPTION_DESCRIPTION),
            *row2.FindString("description"));
  EXPECT_EQ("Live Caption and Transcription", *row2.FindString("modelName"));
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(kAsrSize)),
            *row2.FindString("size"));
}

TEST_F(OnDeviceAiSettingsHandlerTest, HandleDeleteOnDeviceAiCapability) {
  FakeOnDeviceModelControl fake_control;
  handler()->set_model_control(&fake_control);

  const base::ByteSize kNanoSize = base::GiB(1);
  const base::ByteSize kGemmaSize = base::GiB(2);

  fake_control.SetInstalledModels({
      OnDeviceModelControl::InstalledModel(OnDeviceBaseModel::kV3NanoGpu,
                                           kNanoSize.InBytes()),
      OnDeviceModelControl::InstalledModel(OnDeviceBaseModel::kGemma4,
                                           kGemmaSize.InBytes()),
  });

  // Hidden/unmapped models (e.g. kClassifier) should not be uninstallable.
  {
    base::ListValue invalid_args;
    invalid_args.Append("ignored_callback_id");
    invalid_args.Append(static_cast<int>(OnDeviceBaseModel::kClassifier));
    handler()->HandleDeleteOnDeviceAiCapability(invalid_args);
    EXPECT_TRUE(fake_control.uninstalled_models().empty());
    test_web_ui()->ClearTrackedCalls();
  }

  base::ListValue args;
  args.Append("delete_callback_id");
  args.Append(static_cast<int>(OnDeviceBaseModel::kGemma4));
  handler()->HandleDeleteOnDeviceAiCapability(args);

  ASSERT_EQ(1u, fake_control.uninstalled_models().size());
  EXPECT_EQ(OnDeviceBaseModel::kGemma4, fake_control.uninstalled_models()[0]);

  ASSERT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("delete_callback_id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictValue& result = call_data.arg3()->GetDict();
  EXPECT_EQ(base::UTF16ToUTF8(ui::FormatBytes(kNanoSize)),
            *result.FindString("totalSize"));

  const base::ListValue* capabilities = result.FindList("capabilities");
  ASSERT_NE(nullptr, capabilities);
  ASSERT_EQ(1u, capabilities->size());
  EXPECT_EQ(static_cast<int>(OnDeviceBaseModel::kV3NanoGpu),
            (*capabilities)[0].GetDict().FindInt("id"));
}

}  // namespace settings
