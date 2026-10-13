// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider_metrics_provider.h"

#include <algorithm>
#include <utility>

#include "components/prefs/pref_service.h"
#include "components/site_token_provider/site_token_provider_prefs.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace site_token_provider {

SiteTokenProviderMetricsProvider::SiteTokenProviderMetricsProvider(
    GetProfilePrefsCallback get_profile_prefs)
    : get_profile_prefs_(std::move(get_profile_prefs)) {}

SiteTokenProviderMetricsProvider::~SiteTokenProviderMetricsProvider() = default;

void SiteTokenProviderMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  const std::vector<const PrefService*> profile_prefs =
      get_profile_prefs_.Run();
  // Without any loaded profile the state is unknown, so leave it unset.
  if (profile_prefs.empty()) {
    return;
  }

  const bool any_enabled =
      std::ranges::any_of(profile_prefs, [](const PrefService* pref_service) {
        return pref_service->GetBoolean(prefs::kSiteTokenProviderPref);
      });
  system_profile->mutable_site_token_provider_state()->set_sync_pref_enabled(
      any_enabled);
}

}  // namespace site_token_provider
