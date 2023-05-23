// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/prefs/pref_change_registrar.h"

namespace content {
class WebContents;
}  // namespace content

namespace content_settings {

class CookieSettings;
class CookieControlsView;

// Handles the tab specific state for cookie controls.
class CookieControlsController
    : content_settings::CookieSettings::Observer,
      public base::SupportsWeakPtr<CookieControlsController> {
 public:
  CookieControlsController(
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<content_settings::CookieSettings> original_cookie_settings);
  CookieControlsController(const CookieControlsController& other) = delete;
  CookieControlsController& operator=(const CookieControlsController& other) =
      delete;
  ~CookieControlsController() override;

  // Called when the web_contents has changed.
  void Update(content::WebContents* web_contents);

  // Called when CookieControlsView is closing.
  void OnUiClosing();

  // Called when the user clicks on the button to enable/disable cookie
  // blocking.
  void OnCookieBlockingEnabledForSite(bool block_third_party_cookies);

  // Returns whether first-party cookies are blocked.
  bool FirstPartyCookiesBlocked();

  void AddObserver(CookieControlsView* obs);
  void RemoveObserver(CookieControlsView* obs);

 private:
  // The observed WebContents changes during the lifetime of the
  // CookieControlsController. SiteDataObserver can't change the observed
  // object, so we need an inner class that can be recreated when necessary.
  // TODO(dullweber): Make it possible to change the observed class and maybe
  // convert SiteDataObserver to a pure virtual interface.
  class TabObserver
      : public content_settings::PageSpecificContentSettings::SiteDataObserver {
   public:
    TabObserver(CookieControlsController* cookie_controls,
                content::WebContents* web_contents);

    TabObserver(const TabObserver&) = delete;
    TabObserver& operator=(const TabObserver&) = delete;

    // PageSpecificContentSettings::SiteDataObserver:
    void OnSiteDataAccessed(const AccessDetails& access_details) override;
    void OnStatefulBounceDetected() override;

   private:
    raw_ptr<CookieControlsController> cookie_controls_;
  };

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;
  void OnCookieSettingChanged() override;

  // Determine the CookieControlsStatus based on |web_contents|.
  std::pair<CookieControlsStatus, CookieControlsEnforcement> GetStatus(
      content::WebContents* web_contents);

  // Updates the blocked cookie count of |icon_|.
  void PresentBlockedCookieCounter();

  // Returns the number of allowed cookies.
  int GetAllowedCookieCount();

  // Returns the number of blocked cookies.
  int GetBlockedCookieCount();

  // Returns the number of stateful bounces leading to this page.
  int GetStatefulBounceCount();

  content::WebContents* GetWebContents();

  std::unique_ptr<TabObserver> tab_observer_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Cookie_settings for the original profile associated with
  // |cookie_settings_|, if there is one. For example, in Chrome, this
  // corresponds to the regular profile when |cookie_settings_| is incognito.
  // This may be null.
  scoped_refptr<content_settings::CookieSettings> original_cookie_settings_;

  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_observation_{this};

  bool should_reload_ = false;

  base::ObserverList<CookieControlsView> observers_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_COOKIE_CONTROLS_CONTROLLER_H_
