// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"
#include "components/full_restore/desk_template_read_handler.h"

class Profile;

namespace apps {
enum class AppTypeName;
}  // namespace apps

namespace full_restore {
class RestoreData;
struct WindowInfo;
}  // namespace full_restore

// The DeskTemplateAppLaunchHandler class is passed in the desk template restore
// data and profile, and will launch apps and web pages based on the template.
class DeskTemplateAppLaunchHandler
    : public chromeos::AppLaunchHandler,
      public full_restore::DeskTemplateReadHandler::Delegate {
 public:
  explicit DeskTemplateAppLaunchHandler(Profile* profile);
  DeskTemplateAppLaunchHandler(const DeskTemplateAppLaunchHandler&) = delete;
  DeskTemplateAppLaunchHandler& operator=(const DeskTemplateAppLaunchHandler&) =
      delete;
  ~DeskTemplateAppLaunchHandler() override;

  void SetRestoreDataAndLaunch(
      std::unique_ptr<full_restore::RestoreData> restore_data);

  // full_restore::DeskTemplateReadHandler::Delegate:
  std::unique_ptr<full_restore::WindowInfo> GetWindowInfo(
      int restore_window_id) override;
  int32_t FetchRestoreWindowId(const std::string& app_id) override;
  bool IsFullRestoreRunning() const override;

  Profile* profile() { return profile_; }

 protected:
  // chromeos::AppLaunchHandler:
  void OnExtensionLaunching(const std::string& app_id) override;
  base::WeakPtr<chromeos::AppLaunchHandler> GetWeakPtrAppLaunchHandler()
      override;

 private:
  // Go through the restore data launch list and launches the browser windows.
  void LaunchBrowsers();

  // chromeos::AppLaunchHandler:
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

  // Resets `restore_data_clone_`. Callback for a timeout after
  // `SetRestoreDataAndLaunch()` sets new RestoreData. Once
  // `restore_data_clone_` is deleted, the current desk template launch is
  // ended.
  void ClearRestoreDataClone();

  // A copy of `restore_data_` from when it was set. `restore_data_` has entries
  // removed before launching, but we need to reference the data during launch,
  // which is async, so keep a copy around.
  std::unique_ptr<full_restore::RestoreData> restore_data_clone_;

  base::WeakPtrFactory<DeskTemplateAppLaunchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
