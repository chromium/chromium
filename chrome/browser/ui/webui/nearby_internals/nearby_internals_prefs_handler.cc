// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_prefs_handler.h"
#include "base/functional/bind.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/cross_device/logging/logging.h"

namespace {

const char kNearbySharingPrefPrefix[] = "nearby_sharing";

}  // namespace

NearbyInternalsPrefsHandler::NearbyInternalsPrefsHandler(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  pref_service_ = profile->GetPrefs();
}

NearbyInternalsPrefsHandler::~NearbyInternalsPrefsHandler() = default;

void NearbyInternalsPrefsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearNearbyPrefs",
      base::BindRepeating(&NearbyInternalsPrefsHandler::HandleClearNearbyPrefs,
                          base::Unretained(this)));
}

void NearbyInternalsPrefsHandler::OnJavascriptAllowed() {}

void NearbyInternalsPrefsHandler::OnJavascriptDisallowed() {}

void NearbyInternalsPrefsHandler::HandleClearNearbyPrefs(
    const base::Value::List& args) {
  // Reset onboarding otherwise turning off Nearby also sets Fast Initiation
  // pref.
  pref_service_->SetBoolean(prefs::kNearbySharingOnboardingCompletePrefName,
                            false);
  // Turn Nearby feature off.
  pref_service_->SetBoolean(prefs::kNearbySharingEnabledPrefName, false);

  // Clear all Nearby related prefs.
  pref_service_->ClearPrefsWithPrefixSilently(kNearbySharingPrefPrefix);

  // Add log message so users who trigger the Clear Pref button on
  // chrome://nearby-internals know that the Nearby prefs have been cleared.
  CD_LOG(INFO, Feature::NS)
      << "Nearby Share has been disabled and Nearby prefs have been cleared.";
}
