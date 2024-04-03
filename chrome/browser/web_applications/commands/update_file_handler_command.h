// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_FILE_HANDLER_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_FILE_HANDLER_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

enum class Result;

// UpdateFileHandlerCommand updates file handler registration to match with the
// user choice.
class UpdateFileHandlerCommand : public WebAppCommand<AppLock> {
 public:
  // Updates the File Handling API approval state for the given app. If
  // necessary, it also updates the registration with the OS.
  static std::unique_ptr<UpdateFileHandlerCommand> CreateForPersistUserChoice(
      const webapps::AppId& app_id,
      bool allowed,
      base::OnceClosure callback);

  ~UpdateFileHandlerCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  UpdateFileHandlerCommand(const webapps::AppId& app_id,
                           bool user_choice_to_remember,
                           base::OnceClosure callback);
  void OnFileHandlerUpdated(bool file_handling_enabled);
  void ReportResultAndDestroy(CommandResult result);

  std::unique_ptr<AppLock> lock_;

  const webapps::AppId app_id_;
  const bool user_choice_to_remember_;

  base::WeakPtrFactory<UpdateFileHandlerCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_FILE_HANDLER_COMMAND_H_
