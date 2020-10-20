// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "components/sessions/core/session_id.h"

class Browser;
class Profile;

namespace chromeos {

class DiscoverWindowManagerObserver;

// Manages Discover windows for CrOS. Each Profile is associated with a single
// Browser window for Discover UI that will be created when the separate
// Discover App is first opened.
class DiscoverWindowManager {
 public:
  static DiscoverWindowManager* GetInstance();

  void AddObserver(DiscoverWindowManagerObserver* observer);
  void RemoveObserver(const DiscoverWindowManagerObserver* observer);

  // Shows a chrome://oobe/discover/ page in an an existing system
  // Browser window for `profile` or creates a new one.
  void ShowChromeDiscoverPageForProfile(Profile* profile);

  // If a Browser Discover app window for `profile` has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfile(Profile* profile);

  // Returns true if `browser` is a Discover app window.
  bool IsDiscoverBrowser(Browser* browser) const;

 private:
  friend class base::NoDestructor<DiscoverWindowManager>;
  using ProfileSessionMap = std::map<Profile*, SessionID>;

  DiscoverWindowManager();
  ~DiscoverWindowManager();

  base::ObserverList<DiscoverWindowManagerObserver> observers_;
  ProfileSessionMap discover_session_map_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverWindowManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_WINDOW_MANAGER_H_
