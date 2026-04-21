// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_OR_REPARENT_WEB_CONTENTS_INTO_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_OR_REPARENT_WEB_CONTENTS_INTO_APP_COMMAND_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/scheduler/launch_or_reparent_result.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"

class Browser;

namespace content {
class WebContents;
}

namespace web_app {

// Command that reparents a given WebContents into an app window if it is
// in-scope of the app, and otherwise launches the app at its start URL.
// This command only runs for apps that open in a dedicated window.
// If the app is not installed or does not open in a dedicated window, the
// command noops.
class LaunchOrReparentWebContentsIntoAppCommand
    : public WebAppCommand<AppLock, LaunchOrReparentResult> {
 public:
  // Creates a command that will reparent |web_contents| into the app with
  // |app_id| if it is in scope, or launch it. |callback| is called with the
  // result of the operation.
  LaunchOrReparentWebContentsIntoAppCommand(
      const webapps::AppId& app_id,
      base::WeakPtr<content::WebContents> web_contents,
      base::OnceCallback<void(LaunchOrReparentResult)> callback);
  ~LaunchOrReparentWebContentsIntoAppCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  const webapps::AppId app_id_;
  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<AppLock> lock_;

  void OnAppLaunched(base::WeakPtr<Browser> browser,
                     base::WeakPtr<content::WebContents> web_contents,
                     apps::LaunchContainer container,
                     base::Value debug_value);

  base::WeakPtrFactory<LaunchOrReparentWebContentsIntoAppCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_LAUNCH_OR_REPARENT_WEB_CONTENTS_INTO_APP_COMMAND_H_
