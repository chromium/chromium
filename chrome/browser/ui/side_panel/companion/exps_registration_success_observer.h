// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_EXPS_REGISTRATION_SUCCESS_OBSERVER_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_EXPS_REGISTRATION_SUCCESS_OBSERVER_H_

#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace companion {

// An observer that observes page navigations on a tab and determines if the
// user has laned on the success page of exps registration. Additionally, it
// also keeps a watch for navigations in active tab and shows an IPH if
// crieterias match.
class ExpsRegistrationSuccessObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExpsRegistrationSuccessObserver> {
 public:
  explicit ExpsRegistrationSuccessObserver(content::WebContents* web_contents);
  ~ExpsRegistrationSuccessObserver() override;

  // Disallow copy/assign.
  ExpsRegistrationSuccessObserver(const ExpsRegistrationSuccessObserver&) =
      delete;
  ExpsRegistrationSuccessObserver& operator=(
      const ExpsRegistrationSuccessObserver&) = delete;

 protected:
  void MaybeShowIPH();
  virtual void ShowIPH();
  virtual bool IsSearchInCompanionSidePanelSupported();
  virtual PrefService* pref_service();

 private:
  friend class content::WebContentsUserData<ExpsRegistrationSuccessObserver>;

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;

  // Called on every page load. Determines and shows IPH if the conditions are
  // met.

  // Whether the current URL is blocklisted from showing IPH.
  bool IsUrlBlockListedForIPH(const GURL& url);

  // The list of URLs to search for a match that represents exps registration
  // success.
  std::vector<GURL> urls_to_match_against_;

  // The list of blocklisted URLs to search for a match for which IPH isn't
  // shown.
  std::vector<GURL> blocklisted_iph_urls_to_match_against_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_COMPANION_EXPS_REGISTRATION_SUCCESS_OBSERVER_H_
