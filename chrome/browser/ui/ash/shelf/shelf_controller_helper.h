// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTROLLER_HELPER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTROLLER_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"

class ArcAppListPrefs;
class ExtensionEnableFlow;
class Profile;

namespace apps {
enum class PromiseStatus;
}

namespace content {
class WebContents;
}

// Assists ChromeShelfController with ExtensionService interaction.
class ShelfControllerHelper : public ExtensionEnableFlowDelegate {
 public:
  explicit ShelfControllerHelper(Profile* profile);

  ShelfControllerHelper(const ShelfControllerHelper&) = delete;
  ShelfControllerHelper& operator=(const ShelfControllerHelper&) = delete;

  ~ShelfControllerHelper() override;

  // Get the item label that should be shown for the specified promise app
  // status.
  static std::u16string GetLabelForPromiseStatus(apps::PromiseStatus status);

  // Get the accessible label that should be announced by the screenreader for
  // the specified promise app name and status.
  static std::u16string GetAccessibleLabelForPromiseStatus(
      std::optional<std::string> app_name,
      apps::PromiseStatus status);

  // Helper function to return the title associated with |app_id|.
  // Returns an empty title if no matching extension can be found.
  static std::u16string GetAppTitle(Profile* profile,
                                    const std::string& app_id);

  // Helper function to return the package id associated with |app_id|.
  // Returns an empty string if no matching extension can be found.
  static std::string GetAppPackageId(Profile* profile,
                                     const std::string& app_id);

  // Helper function to return the app status associated with |app_id|. if the
  // app is blocked, return AppStatus::kBlocked. Otherwise, if the app is
  // paused, return AppStatus::kPaused. Otherwise, return AppStatus::kReady.
  static ash::AppStatus GetAppStatus(Profile* profile,
                                     const std::string& app_id);

  // Returns whether the app with `app_id` was installed by default.
  static bool IsAppDefaultInstalled(Profile* profile,
                                    const std::string& app_id);

  // Returns the app id of the specified tab, or an empty string if there is
  // no app. All known profiles will be queried for this.
  virtual std::string GetAppID(content::WebContents* tab);

  // Get the accessible label that should be announced by the screenreader for
  // the specified promise shelf item.
  static std::u16string GetPromiseAppAccessibleName(
      Profile* profile,
      const std::string& package_id);

  // Retrieve the label for a registered promise app. If there isn't a promise
  // app with the specified package ID, return an empty string.
  static std::u16string GetPromiseAppTitle(
      Profile* profile,
      const std::string& string_package_id);

  // Retrieve the installation progress value for a registered promise app. If
  // there isn't a promise app with the specified package ID, return -1.
  static float GetPromiseAppProgress(Profile* profile,
                                     const std::string& string_package_id);

  // Check whether this item is a registered promise app.
  static bool IsPromiseApp(Profile* profile, const std::string& id);

  // Convert promise status into general app status.
  static ash::AppStatus ConvertPromiseStatusToAppStatus(
      apps::PromiseStatus promise_status);

  // Returns true if |id| is valid for the currently active profile.
  // Used during restore to ignore no longer valid extensions.
  // Note that already running applications are ignored by the restore process.
  virtual bool IsValidIDForCurrentUser(const std::string& app_id) const;

  void LaunchApp(const ash::ShelfID& id,
                 ash::ShelfLaunchSource source,
                 int event_flags,
                 int64_t display_id,
                 bool new_window);

  virtual ArcAppListPrefs* GetArcAppListPrefs() const;

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  void set_profile(Profile* profile) { profile_ = profile; }

  bool IsValidPromisePackageIdFromAppService(
      const std::string& promise_package_id) const;

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
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<ExtensionEnableFlow> extension_enable_flow_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTROLLER_HELPER_H_
