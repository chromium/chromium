// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/full_restore/app_launch_handler.h"

class Profile;

namespace apps {
enum class AppTypeName;
}  // namespace apps

namespace full_restore {
class RestoreData;
}  // namespace full_restore

// The DeskTemplateAppLaunchHandler class is passed in the desk template restore
// data and profile, and will launch apps and web pages based on the template.
class DeskTemplateAppLaunchHandler : public chromeos::AppLaunchHandler {
 public:
  explicit DeskTemplateAppLaunchHandler(Profile* profile);
  DeskTemplateAppLaunchHandler(const DeskTemplateAppLaunchHandler&) = delete;
  DeskTemplateAppLaunchHandler& operator=(const DeskTemplateAppLaunchHandler&) =
      delete;
  ~DeskTemplateAppLaunchHandler() override;

  void SetRestoreDataAndLaunch(
      std::unique_ptr<full_restore::RestoreData> restore_data);

 protected:
  // chromeos::AppLaunchHandler:
  base::WeakPtr<chromeos::AppLaunchHandler> GetWeakPtrAppLaunchHandler()
      override;

 private:
  // chromeos::AppLaunchHandler:
  void LaunchBrowser() override;
  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;
  void RecordArcGhostWindowLaunch(bool is_arc_ghost_window) override;

  base::WeakPtrFactory<DeskTemplateAppLaunchHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESK_TEMPLATE_APP_LAUNCH_HANDLER_H_
