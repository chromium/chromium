// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_DESKS_TEMPLATES_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_UI_ASH_DESKS_DESKS_TEMPLATES_APP_LAUNCH_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"

class Profile;

namespace apps {
enum class AppTypeName;
}  // namespace apps

namespace app_restore {
class DeskTemplateReadHandler;
}  // namespace app_restore

namespace ash {
class DeskTemplate;
}  // namespace ash

// The DesksTemplatesAppLaunchHandler class is passed a profile, and will launch
// apps and web pages based on the template. Note that a new handler should be
// created for each template launch.
class DesksTemplatesAppLaunchHandler : public ash::AppLaunchHandler {
 public:
  explicit DesksTemplatesAppLaunchHandler(Profile* profile);
  DesksTemplatesAppLaunchHandler(const DesksTemplatesAppLaunchHandler&) =
      delete;
  DesksTemplatesAppLaunchHandler& operator=(
      const DesksTemplatesAppLaunchHandler&) = delete;
  ~DesksTemplatesAppLaunchHandler() override;

  // Launch the given template.
  void LaunchTemplate(const ash::DeskTemplate& desk_template);

 protected:
  // chromeos::AppLaunchHandler:
  bool ShouldLaunchSystemWebAppOrChromeApp(
      const std::string& app_id,
      const app_restore::RestoreData::LaunchList& launch_list) override;
  void OnExtensionLaunching(const std::string& app_id) override;
  base::WeakPtr<ash::AppLaunchHandler> GetWeakPtrAppLaunchHandler() override;

 private:
  // Go through the restore data launch list and launches the browser windows.
  void LaunchBrowsers();

  // Launches ARC apps if they are supported.
  void MaybeLaunchArcApps();

  // Launches Lacros browsers if there are entries for them in the restore data.
  void MaybeLaunchLacrosBrowsers();

  // Notifies observers that a single instance app has moved.
  void NotifyMovedSingleInstanceApp(int32_t window_id);

  // chromeos::AppLaunchHandler:
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

  // Cached convenience pointer to the desk template read handler.
  app_restore::DeskTemplateReadHandler* const read_handler_;

  // The ID of the specific launch this handler deals with.
  int32_t launch_id_ = 0;

  base::WeakPtrFactory<DesksTemplatesAppLaunchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_DESKS_TEMPLATES_APP_LAUNCH_HANDLER_H_
