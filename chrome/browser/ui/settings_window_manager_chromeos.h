// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_

#include <map>
#include <string>

#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/sessions/core/session_id.h"
#include "ui/display/types/display_constants.h"

class Browser;
class GURL;
class Profile;

namespace chrome {

class SettingsWindowManagerObserver;

// Manages Settings windows for CrOS. Each Profile is associated with a single
// Browser window for Settings that will be created when the Settings UI is
// first opened and reused for any Settings links while it exists.

class SettingsWindowManager {
 public:
  SettingsWindowManager(const SettingsWindowManager&) = delete;
  SettingsWindowManager& operator=(const SettingsWindowManager&) = delete;

  static SettingsWindowManager* GetInstance();

  // Caller is responsible for |manager|'s life time.
  static void SetInstanceForTesting(SettingsWindowManager* manager);

  // See https://crbug.com/1067073.
  static void ForceDeprecatedSettingsWindowForTesting();
  static bool UseDeprecatedSettingsWindow(Profile* profile);

  void AddObserver(SettingsWindowManagerObserver* observer);
  void RemoveObserver(SettingsWindowManagerObserver* observer);

  // Shows a chrome:// page (e.g. Settings, About) in an an existing system
  // Browser window for `profile` or creates a new one. `callback` will run on
  // Chrome page shown. `callback` can be null.
  virtual void ShowChromePageForProfile(Profile* profile,
                                        const GURL& gurl,
                                        int64_t display_id,
                                        apps::LaunchCallback callback);

  // Shows the OS settings window for |profile|. When feature SplitSettings is
  // disabled, this behaves like ShowChromePageForProfile().
  void ShowOSSettings(Profile* profile,
                      int64_t display_id = display::kInvalidDisplayId);

  // As above, but shows a settings sub-page.
  void ShowOSSettings(Profile* profile,
                      const std::string& sub_page,
                      int64_t display_id = display::kInvalidDisplayId);

  // As above, but links to a specific setting.
  void ShowOSSettings(Profile* profile,
                      const std::string& sub_page,
                      const chromeos::settings::mojom::Setting setting_id,
                      int64_t display_id = display::kInvalidDisplayId);

  // If a Browser settings window for |profile| has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfile(Profile* profile);

  // Returns true if |browser| is a settings window.
  bool IsSettingsBrowser(Browser* browser) const;

 protected:
  SettingsWindowManager();
  virtual ~SettingsWindowManager();

 private:
  friend struct base::DefaultSingletonTraits<SettingsWindowManager>;
  typedef std::map<Profile*, SessionID> ProfileSessionMap;

  base::ObserverList<SettingsWindowManagerObserver>::Unchecked observers_;

  // TODO(calamity): Remove when SystemWebApps are enabled by default.
  ProfileSessionMap settings_session_map_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
