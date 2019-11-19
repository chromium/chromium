// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
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
  ~FakeContentPasswordManagerDriver() override {}

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
      const std::vector<autofill::PasswordForm>& forms) override {}

  void PasswordFormsRendered(
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading) override {}

  void PasswordFormSubmitted(
      const autofill::PasswordForm& password_form) override {}

  void ShowManualFallbackForSaving(
      const autofill::PasswordForm& password_form) override {}

  void HideManualFallbackForSaving() override {}

  void SameDocumentNavigation(autofill::mojom::SubmissionIndicatorEvent
                                  submission_indication_event) override {}

  void ShowPasswordSuggestions(base::i18n::TextDirection text_direction,
                               const base::string16& typed_username,
                               int options,
                               const gfx::RectF& bounds) override {}

  void ShowTouchToFill() override {}

  void RecordSavePasswordProgress(const std::string& log) override {
    called_record_save_ = true;
    log_ = log;
  }

  void UserModifiedPasswordField() override {}

  void UserModifiedNonPasswordField(uint32_t renderer_id,
                                    const base::string16& value) override {}

  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override {}

  void FocusedInputChanged(
      autofill::mojom::FocusedFieldType focused_field_type) override {}
  void LogFirstFillingResult(uint32_t form_renderer_id,
                             int32_t result) override {}

  // Records whether RecordSavePasswordProgress() gets called.
  bool called_record_save_;
  // Records data received via RecordSavePasswordProgress() call.
  base::Optional<std::string> log_;

  mojo::Receiver<mojom::PasswordManagerDriver> receiver_{this};
};

class TestLogger : public RendererSavePasswordProgressLogger {
 public:
  TestLogger(mojom::PasswordManagerDriver* driver)
      : RendererSavePasswordProgressLogger(driver) {}

  using RendererSavePasswordProgressLogger::SendLog;
};

}  // namespace

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

}  // namespace autofill
