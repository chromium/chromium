// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate_impl.h"

#include "base/functional/callback.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"

namespace companion {

SigninDelegateImpl::SigninDelegateImpl(content::WebContents* webui_contents)
    : webui_contents_(webui_contents) {}

SigninDelegateImpl::~SigninDelegateImpl() = default;

bool SigninDelegateImpl::AllowedSignin() {
  if (!GetProfile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  if (!SyncServiceFactory::IsSyncAllowed(GetProfile())) {
    return false;
  }

  return true;
}

bool SigninDelegateImpl::IsSignedIn() {
  return GetProfile() &&
         IdentityManagerFactory::GetForProfile(GetProfile())
             ->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         SyncServiceFactory::GetForProfile(GetProfile()) &&
         (SyncServiceFactory::GetForProfile(GetProfile())
              ->GetTransportState() !=
          syncer::SyncService::TransportState::PAUSED);
}

void SigninDelegateImpl::StartSigninFlow() {
  if (IsSignedIn()) {
    return;
  }

  DCHECK(AllowedSignin());

  // Show the promo here.
  if (SyncServiceFactory::GetForProfile(GetProfile())->GetTransportState() !=
      syncer::SyncService::TransportState::PAUSED) {
    signin_ui_util::EnableSyncFromSingleAccountPromo(
        GetProfile(),
        IdentityManagerFactory::GetForProfile(GetProfile())
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin),
        signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION);
    return;
  }

  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      GetProfile(), signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION);
}

void SigninDelegateImpl::EnableMsbb(bool enable_msbb) {
  auto* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(GetProfile());
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(enable_msbb);
}

void SigninDelegateImpl::OpenUrlInBrowser(const GURL& url, bool use_new_tab) {
  auto* browser = companion::GetBrowserForWebContents(webui_contents_);
  if (!browser) {
    return;
  }

  content::OpenURLParams params(url, content::Referrer(),
                                use_new_tab
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                /*is_renderer_initiated*/ false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
}

bool SigninDelegateImpl::ShouldShowRegionSearchIPH() {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  return tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHCompanionSidePanelRegionSearchFeature);
}

Profile* SigninDelegateImpl::GetProfile() {
  return Profile::FromBrowserContext(webui_contents_->GetBrowserContext());
}

}  // namespace companion
