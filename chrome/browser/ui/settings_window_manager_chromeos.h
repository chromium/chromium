// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/sessions/core/session_id.h"

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
  static SettingsWindowManager* GetInstance();

  void AddObserver(SettingsWindowManagerObserver* observer);
  void RemoveObserver(SettingsWindowManagerObserver* observer);

  // Shows a chrome:// page (e.g. Settings, About) in an an existing system
  // Browser window for |profile| or creates a new one.
  void ShowChromePageForProfile(Profile* profile, const GURL& gurl);

  // Shows the OS settings window for |profile|. When feature SplitSettings is
  // disabled, this behaves like ShowChromePageForProfile().
  void ShowOSSettings(Profile* profile);

  // As above, but shows a settings sub-page.
  void ShowOSSettings(Profile* profile, const std::string& sub_page);

  // If a Browser settings window for |profile| has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfile(Profile* profile);

  // Returns true if |browser| is a settings window.
  bool IsSettingsBrowser(Browser* browser) const;

 private:
  friend struct base::DefaultSingletonTraits<SettingsWindowManager>;
  typedef std::map<Profile*, SessionID> ProfileSessionMap;

  SettingsWindowManager();
  ~SettingsWindowManager();

  base::ObserverList<SettingsWindowManagerObserver>::Unchecked observers_;

  // TODO(calamity): Remove when SystemWebApps are enabled by default.
  ProfileSessionMap settings_session_map_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowManager);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SETTINGS_WINDOW_MANAGER_CHROMEOS_H_
