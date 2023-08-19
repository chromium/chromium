// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_BYPASS_USER_BYPASS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_USER_BYPASS_USER_BYPASS_WEB_CONTENTS_OBSERVER_H_

#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace user_bypass {

// Helps set the storage partitioning blink runtime feature state based on the
// available user-specified cookie setting entries for bypass. The state change
// are made to take effect before the top level frame's navigation commits.
class UserBypassWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<UserBypassWebContentsObserver> {
 public:
  UserBypassWebContentsObserver(const UserBypassWebContentsObserver&) = delete;
  UserBypassWebContentsObserver& operator=(
      const UserBypassWebContentsObserver&) = delete;

  ~UserBypassWebContentsObserver() override;

  // Helps access the cookie settings object during tests.
  content_settings::CookieSettings* GetCookieSettingsForTesting() {
    return cookie_settings_.get();
  }

 private:
  explicit UserBypassWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<UserBypassWebContentsObserver>;

  // Loads User Bypass state and sets the BREF's state in the runtime features
  // state context.
  void LoadUserBypass(content::NavigationHandle* navigation_handle);

  // content::WebContentObserver overrides:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  // End of content::WebContentObserver overrides.

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace user_bypass
#endif  // CHROME_BROWSER_USER_BYPASS_USER_BYPASS_WEB_CONTENTS_OBSERVER_H_
