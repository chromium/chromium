// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"

class ArcAppListPrefs;
class ExtensionEnableFlow;
class Profile;

namespace content {
class WebContents;
}

// Assists the LauncherController with ExtensionService interaction.
class LauncherControllerHelper : public ExtensionEnableFlowDelegate {
 public:
  explicit LauncherControllerHelper(Profile* profile);
  ~LauncherControllerHelper() override;

  // Helper function to return the title associated with |app_id|.
  // Returns an empty title if no matching extension can be found.
  static base::string16 GetAppTitle(Profile* profile,
                                    const std::string& app_id);

  // Returns the app id of the specified tab, or an empty string if there is
  // no app. All known profiles will be queried for this.
  virtual std::string GetAppID(content::WebContents* tab);

  // Returns true if |id| is valid for the currently active profile.
  // Used during restore to ignore no longer valid extensions.
  // Note that already running applications are ignored by the restore process.
  virtual bool IsValidIDForCurrentUser(const std::string& app_id) const;

  void LaunchApp(const ash::ShelfID& id,
                 ash::ShelfLaunchSource source,
                 int event_flags,
                 int64_t display_id);

  virtual ArcAppListPrefs* GetArcAppListPrefs() const;

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  void set_profile(Profile* profile) { profile_ = profile; }

 private:
  // ExtensionEnableFlowDelegate:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // Returns true if |id| is a valid ARC app for the currently active profile.
  bool IsValidIDForArcApp(const std::string& app_id) const;

  // Returns true if |id| is a valid app from AppService. ARC app is not
  // handled by this method.
  bool IsValidIDFromAppService(const std::string& app_id) const;

  // The currently active profile for the usage of |GetAppID|.
  Profile* profile_;
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;

  DISALLOW_COPY_AND_ASSIGN(LauncherControllerHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_
