// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/sessions/core/session_id.h"
#include "ui/display/types/display_constants.h"

class Browser;
class GURL;
class Profile;

class ChromeUrlWindowManagerObserver;

// Manages Lacros redirected chrome:// and os:// URL headless windows in
// Ash. The windows will be managed by Profile (even though the chrome pages
// are of system scope) and given URL. Only a single window of a URL type can
// be opened.
class ChromeUrlWindowManager : public BrowserListObserver {
 public:
  ChromeUrlWindowManager();
  ChromeUrlWindowManager(const ChromeUrlWindowManager&) = delete;
  ChromeUrlWindowManager& operator=(const ChromeUrlWindowManager&) = delete;
  ~ChromeUrlWindowManager() override;

  void AddObserver(ChromeUrlWindowManagerObserver* observer);
  void RemoveObserver(ChromeUrlWindowManagerObserver* observer);

  // Shows a chrome:// or os:// page (e.g. Flags) in an an existing system
  // Browser window for |profile| or creates a new one.
  virtual void ShowChromePageForProfile(Profile* profile,
                                        const GURL& gurl,
                                        int64_t display_id);

  // If a Browser window for |url| and |profile| has already been created,
  // returns it, otherwise returns NULL.
  Browser* FindBrowserForProfileAndUrl(Profile* profile, const GURL& gurl);

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  // TODO(crbug/1256497): Once we go to more URLs, this need to be changed.
  using ProfileSessionMap = std::map<Profile*, SessionID>;
  ProfileSessionMap chrome_url_session_map_;

  base::ObserverList<ChromeUrlWindowManagerObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_URL_WINDOW_MANAGER_H_
