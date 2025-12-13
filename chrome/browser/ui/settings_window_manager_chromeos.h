// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_

#include <map>
#include <string_view>

#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/sessions/core/session_id.h"
#include "ui/display/types/display_constants.h"

class Browser;
class BrowserWindowInterface;
class GURL;
class Profile;

namespace aura {
class WindowTracker;
}  // namespace aura

namespace chrome {

// Manages Settings windows for CrOS. Each Profile is associated with a single
// Browser window for Settings that will be created when the Settings UI is
// first opened and reused for any Settings links while it exists.
class SettingsWindowManager : public ash::SettingsAppManager {
 public:
  SettingsWindowManager();
  SettingsWindowManager(const SettingsWindowManager&) = delete;
  SettingsWindowManager& operator=(const SettingsWindowManager&) = delete;
  ~SettingsWindowManager() override;

  // TODO(crbug.com/472871229): Migrate into SettingsAppManager::Get().
  static SettingsWindowManager* GetInstance();

  // See https://crbug.com/1067073.
  static void ForceDeprecatedSettingsWindowForTesting();
  static bool UseDeprecatedSettingsWindow(Profile* profile);

  // ash::SettingsAppManager:
  void Open(const user_manager::User& user, OpenParams params) override;

  // Shows a chrome:// page (e.g. Settings, About) in an an existing system
  // Browser window for `profile` or creates a new one. `callback` will run on
  // Chrome page shown. `callback` can be null.
  virtual void ShowChromePageForProfile(Profile* profile,
                                        const GURL& gurl,
                                        int64_t display_id,
                                        apps::LaunchCallback callback);

  // Shows the OS settings window for |profile|. When feature SplitSettings is
  // disabled, this behaves like ShowChromePageForProfile().
  // DEPRECATED. Please use Open().
  void ShowOSSettings(Profile* profile,
                      int64_t display_id = display::kInvalidDisplayId);

  // As above, but shows a settings sub-page.
  // DEPRECATED. Please use Open().
  void ShowOSSettings(Profile* profile,
                      std::string_view sub_page,
                      int64_t display_id = display::kInvalidDisplayId);

  // As above, but links to a specific setting.
  // DEPRECATED. Please use Open().
  void ShowOSSettings(Profile* profile,
                      std::string_view sub_page,
                      const chromeos::settings::mojom::Setting setting_id,
                      int64_t display_id = display::kInvalidDisplayId);

  // If a Browser settings window for |profile| has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfile(Profile* profile);

  // Returns true if |browser| is a settings window.
  bool IsSettingsBrowser(BrowserWindowInterface* browser) const;

 private:
  typedef std::map<Profile*, SessionID> ProfileSessionMap;

  std::unique_ptr<aura::WindowTracker> legacy_settings_title_updater_;

  // TODO(calamity): Remove when SystemWebApps are enabled by default.
  ProfileSessionMap settings_session_map_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
