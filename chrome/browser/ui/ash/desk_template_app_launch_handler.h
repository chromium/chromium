// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"

class Profile;

namespace apps {
enum class AppTypeName;
}  // namespace apps

namespace app_restore {
class DeskTemplateReadHandler;
class RestoreData;
}  // namespace app_restore

// The DeskTemplateAppLaunchHandler class is passed in the desk template restore
// data and profile, and will launch apps and web pages based on the template.
class DeskTemplateAppLaunchHandler : public ash::AppLaunchHandler {
 public:
  explicit DeskTemplateAppLaunchHandler(Profile* profile);
  DeskTemplateAppLaunchHandler(const DeskTemplateAppLaunchHandler&) = delete;
  DeskTemplateAppLaunchHandler& operator=(const DeskTemplateAppLaunchHandler&) =
      delete;
  ~DeskTemplateAppLaunchHandler() override;

  void SetRestoreDataAndLaunch(
      std::unique_ptr<app_restore::RestoreData> restore_data);

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

  // Resets the restore data in `read_handler_`. Callback for a timeout after
  // `SetRestoreDataAndLaunch()` sets new RestoreData. Once this is called, the
  // current desk template launch is considered done.
  void ClearDeskTemplateReadHandlerRestoreData();

  // chromeos::AppLaunchHandler:
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

  // Cached convenience pointer to the desk template read handler.
  app_restore::DeskTemplateReadHandler* const read_handler_;

  base::WeakPtrFactory<DeskTemplateAppLaunchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
