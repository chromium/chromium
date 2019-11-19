// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTROLLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"

namespace content {
class WebContents;
}

namespace content_settings {
class CookieSettings;
}

class CookieControlsView;

// A controller for CookieControlsIconView and CookieControlsBubbleView.
class CookieControlsController {
 public:
  enum class Status {
    kUninitialized,
    // Cookie blocking is enabled.
    kEnabled,
    // Cookie blocking is disabled.
    kDisabled,
    // Cookie blocking is enabled in general but was disabled for this site.
    kDisabledForSite,
  };

  explicit CookieControlsController(content::WebContents* web_contents);
  ~CookieControlsController();

  // Called when the web_contents has changed.
  void Update(content::WebContents* web_contents);

  // Called when CookieControlsBubbleView is closing.
  void OnBubbleUiClosing(content::WebContents* web_contents);

  // Called when the user clicks on the button to enable/disable cookie
  // blocking.
  void OnCookieBlockingEnabledForSite(bool block_third_party_cookies);


  void AddObserver(CookieControlsView* obs);
  void RemoveObserver(CookieControlsView* obs);

 private:
  // The observed WebContents changes during the lifetime of the
  // CookieControlsController. SiteDataObserver can't change the observed
  // object, so we need an inner class that can be recreated when necessary.
  // TODO(dullweber): Make it possible to change the observed class and maybe
  // convert SiteDataObserver to a pure virtual interface.
  class TabObserver : public TabSpecificContentSettings::SiteDataObserver {
   public:
    TabObserver(CookieControlsController* cookie_controls,
                TabSpecificContentSettings* tab_specific_content_settings);

    // TabSpecificContentSettings::SiteDataObserver:
    void OnSiteDataAccessed() override;

   private:
    CookieControlsController* cookie_controls_;

    DISALLOW_COPY_AND_ASSIGN(TabObserver);
  };

  // Determine the CookieControlsController::Status based on |web_contents|.
  Status GetStatus(content::WebContents* web_contents);

  // Updates the blocked cookie count of |icon_|.
  void PresentBlockedCookieCounter();

  // Returns the number of blocked cookies.
  int GetBlockedCookieCount();

  // Callback for when the cookie controls or third-party cookie blocking
  // preference changes.
  void OnPrefChanged();

  content::WebContents* GetWebContents();

  std::unique_ptr<TabObserver> tab_observer_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  PrefChangeRegistrar pref_change_registrar_;

  bool should_reload_ = false;

  base::ObserverList<CookieControlsView> observers_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsController);
};

#endif  // CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTROLLER_H_
