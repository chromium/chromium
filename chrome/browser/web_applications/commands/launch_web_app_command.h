// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebAppProvider;

class LaunchWebAppCommand
    : public WebAppCommand<AppLock,
                           base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer> {
 public:
  LaunchWebAppCommand(Profile* profile,
                      WebAppProvider* provider,
                      apps::AppLaunchParams params,
                      LaunchWebAppWindowSetting launch_setting,
                      LaunchWebAppCallback callback);
  ~LaunchWebAppCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void FirstRunServiceCompleted(bool success);
  void OnOsIntegrationSynchronized();
  void DoLaunch();
  void OnAppLaunched(base::WeakPtr<Browser> browser,
                     base::WeakPtr<content::WebContents> web_contents,
                     apps::LaunchContainer container,
                     base::Value debug_value);

  apps::AppLaunchParams params_;
  LaunchWebAppWindowSetting launch_setting_;

  std::unique_ptr<AppLock> lock_;

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;

  base::WeakPtrFactory<LaunchWebAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_WEB_APP_COMMAND_H_
