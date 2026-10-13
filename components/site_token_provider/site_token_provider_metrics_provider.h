// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_METRICS_PROVIDER_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_METRICS_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/metrics/metrics_provider.h"

class PrefService;

namespace site_token_provider {

// Reports whether any loaded profile has the site token provider enabled via
// the synced `prefs::kSiteTokenProviderPref`.
class SiteTokenProviderMetricsProvider : public metrics::MetricsProvider {
 public:
  // Returns the prefs of the profiles to consider. Callers are expected to
  // exclude Guest, System, and off-the-record profiles.
  using GetProfilePrefsCallback =
      base::RepeatingCallback<std::vector<const PrefService*>()>;

  explicit SiteTokenProviderMetricsProvider(
      GetProfilePrefsCallback get_profile_prefs);

  SiteTokenProviderMetricsProvider(const SiteTokenProviderMetricsProvider&) =
      delete;
  SiteTokenProviderMetricsProvider& operator=(
      const SiteTokenProviderMetricsProvider&) = delete;

  ~SiteTokenProviderMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  GetProfilePrefsCallback get_profile_prefs_;
};

}  // namespace site_token_provider

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_METRICS_PROVIDER_H_
