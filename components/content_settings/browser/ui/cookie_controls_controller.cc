// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/site_for_cookies.h"

using base::UserMetricsAction;

namespace content_settings {

CookieControlsController::CookieControlsController(
    scoped_refptr<CookieSettings> cookie_settings,
    scoped_refptr<CookieSettings> original_cookie_settings)
    : cookie_settings_(cookie_settings),
      original_cookie_settings_(original_cookie_settings) {
  cookie_observation_.Observe(cookie_settings_.get());
}

CookieControlsController::~CookieControlsController() = default;

void CookieControlsController::OnUiClosing() {
  auto* web_contents = GetWebContents();
  if (should_reload_ && web_contents && !web_contents->IsBeingDestroyed()) {
    web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  }
  should_reload_ = false;
}

void CookieControlsController::Update(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!tab_observer_ || GetWebContents() != web_contents) {
    tab_observer_ = std::make_unique<TabObserver>(this, web_contents);
  }
  auto status = GetStatus(web_contents);
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    int allowed_sites = GetAllowedSitesCount();
    int blocked_sites = GetBlockedSitesCount();

    for (auto& observer : observers_) {
      observer.OnStatusChanged(status.status, status.enforcement,
                               status.expiration);
      observer.OnSitesCountChanged(allowed_sites, blocked_sites);
      // TODO(crbug.com/1446230): Return the actual confidence level.
      observer.OnBreakageConfidenceLevelChanged(
          CookieControlsBreakageConfidenceLevel::kMedium);
    }
  } else {
    int allowed_cookies = GetAllowedCookieCount();
    int blocked_cookies = GetBlockedCookieCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : old_observers_) {
      observer.OnStatusChanged(status.status, status.enforcement,
                               allowed_cookies, blocked_cookies);
      observer.OnStatefulBounceCountChanged(bounce_count);
    }
  }
}

CookieControlsController::Status CookieControlsController::GetStatus(
    content::WebContents* web_contents) {
  if (!cookie_settings_->ShouldBlockThirdPartyCookies()) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement, absl::nullopt};
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(kExtensionScheme)) {
    return {CookieControlsStatus::kDisabled,
            CookieControlsEnforcement::kNoEnforcement, absl::nullopt};
  }

  SettingSource source;
  // TODO(crbug.com/1446230): Return the expiration of the active exception when
  // available.
  bool is_allowed = cookie_settings_->IsThirdPartyAccessAllowed(
      web_contents->GetLastCommittedURL(), &source);

  CookieControlsStatus status = is_allowed
                                    ? CookieControlsStatus::kDisabledForSite
                                    : CookieControlsStatus::kEnabled;
  CookieControlsEnforcement enforcement;
  if (source == SETTING_SOURCE_POLICY) {
    enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  } else if (is_allowed && original_cookie_settings_ &&
             original_cookie_settings_->ShouldBlockThirdPartyCookies() &&
             original_cookie_settings_->IsThirdPartyAccessAllowed(
                 web_contents->GetLastCommittedURL(), nullptr /* source */)) {
    // TODO(crbug.com/1015767): Rules from regular mode can't be temporarily
    // overridden in incognito.
    enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  } else {
    enforcement = CookieControlsEnforcement::kNoEnforcement;
  }
  return {status, enforcement, absl::nullopt};
}

void CookieControlsController::OnCookieBlockingEnabledForSite(
    bool block_third_party_cookies) {
  if (block_third_party_cookies) {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOn"));
    should_reload_ = false;
    cookie_settings_->ResetThirdPartyCookieSetting(
        GetWebContents()->GetLastCommittedURL());
  } else {
    base::RecordAction(UserMetricsAction("CookieControls.Bubble.TurnOff"));
    should_reload_ = true;
    cookie_settings_->SetThirdPartyCookieSetting(
        GetWebContents()->GetLastCommittedURL(),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }
}

bool CookieControlsController::FirstPartyCookiesBlocked() {
  // No overrides are given since existing ones only pertain to 3P checks.
  const GURL& url = GetWebContents()->GetLastCommittedURL();
  return !cookie_settings_->IsFullCookieAccessAllowed(
      url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
      net::CookieSettingOverrides());
}

int CookieControlsController::GetAllowedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      tab_observer_->web_contents()->GetPrimaryPage());
  if (pscs) {
    return pscs->allowed_local_shared_objects().GetObjectCount();
  } else {
    return 0;
  }
}
int CookieControlsController::GetBlockedCookieCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      tab_observer_->web_contents()->GetPrimaryPage());
  if (pscs) {
    return pscs->blocked_local_shared_objects().GetObjectCount();
  } else {
    return 0;
  }
}

int CookieControlsController::GetAllowedSitesCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      tab_observer_->web_contents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }
  return browsing_data::GetUniqueHostCount(
      pscs->allowed_local_shared_objects(),
      *(pscs->allowed_browsing_data_model()));
}

int CookieControlsController::GetBlockedSitesCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      tab_observer_->web_contents()->GetPrimaryPage());
  if (!pscs) {
    return 0;
  }
  return browsing_data::GetUniqueHostCount(
      pscs->blocked_local_shared_objects(),
      *(pscs->blocked_browsing_data_model()));
}

int CookieControlsController::GetStatefulBounceCount() const {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      tab_observer_->web_contents()->GetPrimaryPage());
  if (pscs) {
    return pscs->stateful_bounce_count();
  } else {
    return 0;
  }
}

void CookieControlsController::PresentBlockedCookieCounter() {
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    int allowed_sites = GetAllowedSitesCount();
    int blocked_sites = GetBlockedSitesCount();

    for (auto& observer : observers_) {
      observer.OnSitesCountChanged(allowed_sites, blocked_sites);
    }
  } else {
    int allowed_cookies = GetAllowedCookieCount();
    int blocked_cookies = GetBlockedCookieCount();
    int bounce_count = GetStatefulBounceCount();

    for (auto& observer : old_observers_) {
      observer.OnCookiesCountChanged(allowed_cookies, blocked_cookies);
      observer.OnStatefulBounceCountChanged(bounce_count);
    }
  }
}

void CookieControlsController::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  if (GetWebContents()) {
    Update(GetWebContents());
  }
}

void CookieControlsController::OnCookieSettingChanged() {
  if (GetWebContents()) {
    Update(GetWebContents());
  }
}

content::WebContents* CookieControlsController::GetWebContents() {
  if (!tab_observer_) {
    return nullptr;
  }
  return tab_observer_->web_contents();
}

void CookieControlsController::AddObserver(OldCookieControlsObserver* obs) {
  old_observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(OldCookieControlsObserver* obs) {
  old_observers_.RemoveObserver(obs);
}

void CookieControlsController::AddObserver(CookieControlsObserver* obs) {
  observers_.AddObserver(obs);
}

void CookieControlsController::RemoveObserver(CookieControlsObserver* obs) {
  observers_.RemoveObserver(obs);
}

CookieControlsController::TabObserver::TabObserver(
    CookieControlsController* cookie_controls,
    content::WebContents* web_contents)
    : content_settings::PageSpecificContentSettings::SiteDataObserver(
          web_contents),
      cookie_controls_(cookie_controls) {}

void CookieControlsController::TabObserver::OnSiteDataAccessed(
    const AccessDetails& access_details) {
  cookie_controls_->PresentBlockedCookieCounter();
}

void CookieControlsController::TabObserver::OnStatefulBounceDetected() {
  cookie_controls_->PresentBlockedCookieCounter();
}

}  // namespace content_settings
