// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_ICON_DIAGNOSTIC_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_ICON_DIAGNOSTIC_COMMAND_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class AppLock;

struct WebAppIconDiagnosticResult {
  bool has_empty_downloaded_icon_sizes = false;
  bool has_generated_icon_flag = false;
  bool has_generated_icon_flag_false_negative = false;
  bool has_generated_icon_bitmap = false;
  bool has_empty_icon_bitmap = false;
  bool has_empty_icon_file = false;
  bool has_missing_icon_file = false;
  // TODO(crbug.com/40858602): Add more checks.

 public:
  // Keep attributes in sync with |CreateIconDiagnosticDebugData| and
  // IsAnyFallbackUsed.
  bool IsAnyFallbackUsed() const {
    return has_empty_downloaded_icon_sizes || has_generated_icon_flag ||
           has_generated_icon_flag_false_negative ||
           has_generated_icon_bitmap || has_empty_icon_bitmap ||
           has_empty_icon_file || has_missing_icon_file;
  }
};

using WebAppIconDiagnosticResultCallback =
    base::OnceCallback<void(std::optional<WebAppIconDiagnosticResult>)>;

// Runs a series of icon health checks for |app_id|.
class WebAppIconDiagnosticCommand
    : public WebAppCommand<AppLock, std::optional<WebAppIconDiagnosticResult>> {
 public:
  WebAppIconDiagnosticCommand(
      Profile* profile,
      const webapps::AppId& app_id,
      WebAppIconDiagnosticResultCallback result_callback);
  ~WebAppIconDiagnosticCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  base::WeakPtr<WebAppIconDiagnosticCommand> GetWeakPtr();
  void ReportResultAndDestroy(CommandResult command_result);

  void LoadIconFromProvider(
      WebAppIconManager::ReadIconWithPurposeCallback callback);
  void DiagnoseGeneratedOrEmptyIconBitmap(base::OnceClosure done_callback,
                                          IconPurpose purpose,
                                          SkBitmap icon_bitmap);

  void CheckForEmptyOrMissingIconFiles(
      base::OnceCallback<void(WebAppIconManager::IconFilesCheck)>
          icon_files_callback);
  void DiagnoseEmptyOrMissingIconFiles(
      base::OnceCallback<void(CommandResult)> done_callback,
      WebAppIconManager::IconFilesCheck icon_files_check);

  const raw_ref<Profile> profile_;
  const webapps::AppId app_id_;

  std::unique_ptr<AppLock> app_lock_;

  std::optional<SquareSizePx> icon_size_;
  std::optional<WebAppIconDiagnosticResult> result_;

  base::WeakPtrFactory<WebAppIconDiagnosticCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_ICON_DIAGNOSTIC_COMMAND_H_
