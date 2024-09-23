// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

const char kTestText[] = "test";

class FakeContentPasswordManagerDriver : public mojom::PasswordManagerDriver {
 public:
  FakeContentPasswordManagerDriver() : called_record_save_(false) {}
  ~FakeContentPasswordManagerDriver() override = default;

  mojo::PendingRemote<mojom::PasswordManagerDriver>
  CreatePendingRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  bool GetLogMessage(std::string* log) {
    if (!called_record_save_)
      return false;

    EXPECT_TRUE(log_);
    *log = *log_;
    return true;
  }

 private:
  // autofill::mojom::PasswordManagerDriver:
  void PasswordFormsParsed(
      const std::vector<autofill::FormData>& form_data) override {}

  void PasswordFormsRendered(
      const std::vector<autofill::FormData>& visible_forms_data) override {}

  void PasswordFormSubmitted(const autofill::FormData& form_data) override {}

  void InformAboutUserInput(const autofill::FormData& form_data) override {}

  void DynamicFormSubmission(autofill::mojom::SubmissionIndicatorEvent
                                 submission_indication_event) override {}

  void PasswordFormCleared(const autofill::FormData& form_data) override {}

  void ShowPasswordSuggestions(
      const autofill::PasswordSuggestionRequest& request) override {}
#if BUILDFLAG(IS_ANDROID)
  void ShowKeyboardReplacingSurface(
      autofill::mojom::SubmissionReadinessState submission_readiness,
      bool is_webauthn_form) override {}
#endif

  void RecordSavePasswordProgress(const std::string& log) override {
    called_record_save_ = true;
    log_ = log;
  }

  void UserModifiedPasswordField() override {}

  void UserModifiedNonPasswordField(autofill::FieldRendererId renderer_id,
                                    const std::u16string& value,
                                    bool autocomplete_attribute_has_username,
                                    bool is_likely_otp) override {}

  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override {}

  void FocusedInputChanged(
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override {}
  void LogFirstFillingResult(autofill::FormRendererId form_renderer_id,
                             int32_t result) override {}

  // Records whether RecordSavePasswordProgress() gets called.
  bool called_record_save_;
  // Records data received via RecordSavePasswordProgress() call.
  std::optional<std::string> log_;

  mojo::Receiver<mojom::PasswordManagerDriver> receiver_{this};
};

class TestLogger : public RendererSavePasswordProgressLogger {
 public:
  TestLogger(mojom::PasswordManagerDriver* driver)
      : RendererSavePasswordProgressLogger(driver) {}

  using RendererSavePasswordProgressLogger::SendLog;
};

TEST(RendererSavePasswordProgressLoggerTest, SendLog) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeContentPasswordManagerDriver fake_driver;
  mojo::Remote<mojom::PasswordManagerDriver> driver_remote(
      fake_driver.CreatePendingRemoteAndBind());
  TestLogger logger(driver_remote.get());
  logger.SendLog(kTestText);

  base::RunLoop().RunUntilIdle();
  std::string sent_log;
  EXPECT_TRUE(fake_driver.GetLogMessage(&sent_log));
  EXPECT_EQ(kTestText, sent_log);
}

}  // namespace
}  // namespace autofill
