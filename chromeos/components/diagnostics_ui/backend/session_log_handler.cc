// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/session_log_handler.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/diagnostics_ui/backend/routine_log.h"
#include "chromeos/components/diagnostics_ui/backend/telemetry_log.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kRoutineLogSectionHeader[] = "=== Routine Log === \n";
const char kTelemetryLogSectionHeader[] = "=== Telemetry Log === \n";
const char kDefaultSessionLogFileName[] = "session_log.txt";
const char kRoutineLogPath[] = "/tmp/diagnostics/diagnostics_routine_log";

}  // namespace

SessionLogHandler::SessionLogHandler(
    const SelectFilePolicyCreator& select_file_policy_creator)
    : SessionLogHandler(
          select_file_policy_creator,
          std::make_unique<TelemetryLog>(),
          std::make_unique<RoutineLog>(base::FilePath(kRoutineLogPath))) {}

SessionLogHandler::SessionLogHandler(
    const SelectFilePolicyCreator& select_file_policy_creator,
    std::unique_ptr<TelemetryLog> telemetry_log,
    std::unique_ptr<RoutineLog> routine_log)
    : select_file_policy_creator_(select_file_policy_creator),
      telemetry_log_(std::move(telemetry_log)),
      routine_log_(std::move(routine_log)) {}

SessionLogHandler::~SessionLogHandler() = default;

void SessionLogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&SessionLogHandler::HandleInitialize,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveSessionLog",
      base::BindRepeating(&SessionLogHandler::HandleSaveSessionLogRequest,
                          base::Unretained(this)));
}

void SessionLogHandler::FileSelected(const base::FilePath& path,
                                     int index,
                                     void* params) {
  const bool success = CreateSessionLog(path);
  ResolveJavascriptCallback(base::Value(save_session_log_callback_id_),
                            base::Value(success));
  save_session_log_callback_id_ = "";
}

void SessionLogHandler::FileSelectionCanceled(void* params) {
  RejectJavascriptCallback(base::Value(save_session_log_callback_id_),
                           /*success=*/base::Value(false));
  save_session_log_callback_id_ = "";
}

TelemetryLog* SessionLogHandler::GetTelemetryLog() const {
  return telemetry_log_.get();
}

RoutineLog* SessionLogHandler::GetRoutineLog() const {
  return routine_log_.get();
}

void SessionLogHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

bool SessionLogHandler::CreateSessionLog(const base::FilePath& file_path) {
  // Fetch RoutineLog
  const std::string routine_log_contents = routine_log_->GetContents();

  // Fetch TelemetryLog
  const std::string telemetry_log_contents = telemetry_log_->GetContents();

  const std::string combined_contents =
      base::StrCat({kTelemetryLogSectionHeader, telemetry_log_contents,
                    kRoutineLogSectionHeader, routine_log_contents});
  return base::WriteFile(file_path, combined_contents);
}

void SessionLogHandler::HandleSaveSessionLogRequest(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  DCHECK(save_session_log_callback_id_.empty());
  save_session_log_callback_id_ = args->GetList()[0].GetString();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::kNullNativeWindow;

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, select_file_policy_creator_.Run(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      /*title=*/l10n_util::GetStringUTF16(IDS_DIAGNOSTICS_SELECT_DIALOG_TITLE),
      /*default_path=*/base::FilePath(kDefaultSessionLogFileName),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owning_window,
      /*params=*/nullptr);
}

void SessionLogHandler::HandleInitialize(const base::ListValue* args) {
  DCHECK(args && args->empty());
  AllowJavascript();
}

}  // namespace diagnostics
}  // namespace chromeos
