// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/computed_app_size.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class AppLock;
class GetIsolatedWebAppSizeJob;
class GetProgressiveWebAppSizeJob;

// ComputeAppSizeCommand calculates the app and data size of a given app
class ComputeAppSizeCommand
    : public WebAppCommand<AppLock, std::optional<ComputedAppSizeWithOrigin>> {
 public:
  ComputeAppSizeCommand(
      const webapps::AppId& app_id,
      Profile* profile,
      base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin>)>
          callback);

  ~ComputeAppSizeCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void ReportResultAndDestroy(CommandResult result);
  void OnIsolatedAppSizeComputed(
      std::optional<ComputedAppSizeWithOrigin> result);
  void OnProgressiveAppSizeComputed(
      std::optional<ComputedAppSizeWithOrigin> result);

  std::unique_ptr<AppLock> lock_;

  const webapps::AppId app_id_;
  const raw_ptr<Profile> profile_;

  ComputedAppSizeWithOrigin size_;

  std::unique_ptr<GetIsolatedWebAppSizeJob> get_isolated_web_app_size_job_;

  std::unique_ptr<GetProgressiveWebAppSizeJob>
      get_progressive_web_app_size_job_;

  base::WeakPtrFactory<ComputeAppSizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_
