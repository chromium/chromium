// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_utils.h"

#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

bool IsFirstRunDisabledByPolicy(Profile& profile) {
  const PrefService* const local_state = g_browser_process->local_state();
  if (!local_state->GetBoolean(prefs::kPromotionsEnabled)) {
    // Corresponding policy: PromotionsEnabled=false
    return true;
  }

  if (!SyncServiceFactory::IsSyncAllowed(&profile)) {
    // Corresponding policy: SyncDisabled=true
    return true;
  }

  if (!profile.GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      !profile.GetPrefs()->GetBoolean(prefs::kSigninAllowedOnNextStartup)) {
    // Corresponding policy: BrowserSignin=0
    return true;
  }

  return false;
}

}  // namespace

void OpenLearnMorePopup(Profile* profile,
                        std::unique_ptr<content::WebContents> contents,
                        const GURL& target_url,
                        const blink::mojom::WindowFeatures& window_features) {
  NavigateParams params(profile, target_url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(contents);
  params.window_features = window_features;
  Navigate(&params);
}

std::optional<ProfilePicker::FirstRunFinishReason> ComputeFirstRunSkipReason(
    Profile& profile,
    signin::IdentityManager& identity_manager) {
  // This check should be done prior to the profile already set up check below,
  // because the policy `BrowserSignin=2` can cause the profile to be signed in
  // already at this point.
  if (signin_util::IsForceSigninEnabled()) {
    // Corresponding policy: BrowserSignin=2
    // Debugging note: On Linux this policy is not supported and does not get
    // translated to the prefs (see crbug.com/41455343), but we still respond to
    // `prefs::kForceBrowserSignin` being set (e.g. if manually edited).
    return ProfilePicker::FirstRunFinishReason::kForceSignin;
  }

  if (identity_manager.HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return ProfilePicker::FirstRunFinishReason::kProfileAlreadySetUp;
  }

  if (IsFirstRunDisabledByPolicy(profile)) {
    return ProfilePicker::FirstRunFinishReason::kSkippedByPolicies;
  }

  return std::nullopt;
}
