// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SET_USER_DISPLAY_MODE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SET_USER_DISPLAY_MODE_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

// Sets the user display mode for an app, and also makes sure os integration
// is triggered if the new user display mode is one that requires that (i.e.
// anything other than "browser").
class SetUserDisplayModeCommand : public WebAppCommand<AppLock> {
 public:
  SetUserDisplayModeCommand(const webapps::AppId& app_id,
                            mojom::UserDisplayMode user_display_mode,
                            base::OnceClosure synchronize_callback);
  ~SetUserDisplayModeCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> app_lock) override;

  // Helper method that only updates user display mode without also possibly
  // triggering os integration. This automatically upgrades to
  // `InstallState::INSTALLED_WITH_OS_INTEGRATION` if necessary. This returns
  // true if the caller needs to synchronize os integration. If the given app_id
  // is not installed, then this is a no-op.
  //
  // This primarily exists for places where the user display mode needs to be
  // set as part of a different Command.
  //
  // Note: This will notify any observers of the registrar of the display mode
  // change synchronously inside this method.
  static bool DoSetDisplayMode(WithAppResources& resources,
                               const webapps::AppId& app_id,
                               mojom::UserDisplayMode user_display_mode,
                               bool is_user_action);

 private:
  void OnSynchronizeComplete();

  std::unique_ptr<AppLock> app_lock_;
  webapps::AppId app_id_;
  mojom::UserDisplayMode user_display_mode_;

  base::WeakPtrFactory<SetUserDisplayModeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SET_USER_DISPLAY_MODE_COMMAND_H_
