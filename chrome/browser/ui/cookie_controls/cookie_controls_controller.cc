
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"

#include <memory>
#include "base/bind.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

CookieControlsController::CookieControlsController(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&CookieControlsController::OnPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::BindRepeating(&CookieControlsController::OnPrefChanged,
                          base::Unretained(this)));
}

CookieControlsController::~CookieControlsController() {}

void CookieControlsController::OnBubbleUiClosing(
    content::WebContents* web_contents) {
  if (should_reload_ && web_contents && !web_contents->IsBeingDestroyed())
    web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  should_reload_ = false;
}

void CookieControlsController::Update(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!tab_observer_ || GetWebContents() != web_contents) {
    DCHECK(TabSpecificContentSettings::FromWebContents(web_contents));
    tab_observer_ = std::make_unique<TabObserver>(
        this, TabSpecificContentSettings::FromWebContents(web_contents));
  }
  for (auto& observer : observers_)
    observer.OnStatusChanged(GetStatus(web_contents), GetBlockedCookieCount());
}

CookieControlsController::Status CookieControlsController::GetStatus(
    content::WebContents* web_contents) {
  if (!cookie_settings_->IsCookieControlsEnabled())
    return CookieControlsController::Status::kDisabled;

  const GURL& url = web_contents->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(extensions::kExtensionScheme)) {
    return CookieControlsController::Status::kDisabled;
  }

  return cookie_settings_->IsThirdPartyAccessAllowed(web_contents->GetURL())
             ? CookieControlsController::Status::kDisabledForSite
             : CookieControlsController::Status::kEnabled;
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  if (block_third_party_cookies) {
    should_reload_ = false;
    cookie_settings_->ResetThirdPartyCookieSetting(GetWebContents()->GetURL());
  } else {
    should_reload_ = true;
    cookie_settings_->SetThirdPartyCookieSetting(
        GetWebContents()->GetURL(), ContentSetting::CONTENT_SETTING_ALLOW);
  }
  Update(GetWebContents());
}

int CookieControlsController::GetBlockedCookieCount() {
  const LocalSharedObjectsContainer& blocked_objects =
      tab_observer_->tab_specific_content_settings()
          ->blocked_local_shared_objects();
  return blocked_objects.GetObjectCount();
}

void CookieControlsController::PresentBlockedCookieCounter() {
  int blocked_cookies = GetBlockedCookieCount();
  for (auto& observer : observers_)
    observer.OnBlockedCookiesCountChanged(blocked_cookies);
}

void CookieControlsController::OnPrefChanged() {
  if (GetWebContents())
    Update(GetWebContents());
}

content::WebContents* CookieControlsController::GetWebContents() {
  if (!tab_observer_ || !tab_observer_->tab_specific_content_settings())
    return nullptr;
  return tab_observer_->tab_specific_content_settings()->web_contents();
}

void CookieControlsController::AddObserver(CookieControlsView* obs) {
  observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(CookieControlsView* obs) {
  observers_.RemoveObserver(obs);
}

CookieControlsController::TabObserver::TabObserver(
    CookieControlsController* cookie_controls,
    TabSpecificContentSettings* tab_specific_content_settings)
    : TabSpecificContentSettings::SiteDataObserver(
          tab_specific_content_settings),
      cookie_controls_(cookie_controls) {}

void CookieControlsController::TabObserver::OnSiteDataAccessed() {
  cookie_controls_->PresentBlockedCookieCounter();
}
