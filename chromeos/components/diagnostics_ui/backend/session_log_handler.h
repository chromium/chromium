// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}  // namespace content

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace diagnostics {

class TelemetryLog;
class RoutineLog;

class SessionLogHandler : public content::WebUIMessageHandler,
                          public ui::SelectFileDialog::Listener {
 public:
  using SelectFilePolicyCreator =
      base::RepeatingCallback<std::unique_ptr<ui::SelectFilePolicy>(
          content::WebContents*)>;
  explicit SessionLogHandler(
      const SelectFilePolicyCreator& select_file_policy_creator);

  // Constructor for testing. Should not be called outside of tests.
  SessionLogHandler(const SelectFilePolicyCreator& select_file_policy_creator,
                    std::unique_ptr<TelemetryLog> telemetry_log,
                    std::unique_ptr<RoutineLog> routine_log);

  ~SessionLogHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  void FileSelectionCanceled(void* params) override;

  SessionLogHandler(const SessionLogHandler&) = delete;
  SessionLogHandler& operator=(const SessionLogHandler&) = delete;

  TelemetryLog* GetTelemetryLog() const;
  RoutineLog* GetRoutineLog() const;

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Creates a session log at `file_path`. The session log includes the contents
  // of both `telemetry_log_` and `routine_log_`. Returns true if the file was
  // successfully written. Retrns false otherwise.
  bool CreateSessionLog(const base::FilePath& file_path);

  // Opens the select dialog.
  void HandleSaveSessionLogRequest(const base::ListValue* args);

  // Initializes Javascript.
  void HandleInitialize(const base::ListValue* args);

  SelectFilePolicyCreator select_file_policy_creator_;
  std::unique_ptr<TelemetryLog> telemetry_log_;
  std::unique_ptr<RoutineLog> routine_log_;
  std::string save_session_log_callback_id_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
