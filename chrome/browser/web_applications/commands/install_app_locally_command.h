// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class InstallAppLocallyCommand : public WebAppCommand<AppLock> {
 public:
  InstallAppLocallyCommand(const webapps::AppId& app_id,
                           base::OnceClosure install_callback);
  ~InstallAppLocallyCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> app_lock) override;

 private:
  // Records locally installed app success metric to UMA after OS Hooks are
  // installed.
  void OnOsIntegrationSynchronized();

  std::unique_ptr<AppLock> app_lock_;
  webapps::AppId app_id_;
  base::WeakPtrFactory<InstallAppLocallyCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_LOCALLY_COMMAND_H_
