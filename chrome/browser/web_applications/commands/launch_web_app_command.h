// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppProvider;

class LaunchWebAppCommand : public WebAppCommandTemplate<AppLock> {
 public:
  LaunchWebAppCommand(Profile* profile,
                      WebAppProvider* provider,
                      apps::AppLaunchParams params,
                      LaunchWebAppWindowSetting launch_setting,
                      LaunchWebAppCallback callback);
  ~LaunchWebAppCommand() override;

  // WebAppCommandTemplate<AppLock>:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnShutdown() override;

 private:
  void FirstRunServiceCompleted(bool success);
  void OnAppLaunched(base::WeakPtr<Browser> browser,
                     base::WeakPtr<content::WebContents> web_contents,
                     apps::LaunchContainer container,
                     base::Value debug_value);
  void Complete(CommandResult result,
                base::WeakPtr<Browser> browser = nullptr,
                base::WeakPtr<content::WebContents> web_contents = nullptr,
                apps::LaunchContainer container =
                    apps::LaunchContainer::kLaunchContainerNone);

  apps::AppLaunchParams params_;
  LaunchWebAppWindowSetting launch_setting_;
  LaunchWebAppCallback callback_;

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  base::Value::Dict debug_value_;

  const raw_ptr<Profile> profile_ = nullptr;
  const raw_ptr<WebAppProvider> provider_ = nullptr;

  base::WeakPtrFactory<LaunchWebAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
