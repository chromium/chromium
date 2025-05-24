// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_test_utils.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

namespace regional_capabilities {

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId country_id) {
  return std::make_unique<RegionalCapabilitiesService>(
      profile_prefs,
      std::make_unique<FakeRegionalCapabilitiesServiceClient>(country_id));
}

FakeRegionalCapabilitiesServiceClient::FakeRegionalCapabilitiesServiceClient(
    CountryId country_id)
    : country_id_(country_id) {}

FakeRegionalCapabilitiesServiceClient::
    ~FakeRegionalCapabilitiesServiceClient() = default;

CountryId FakeRegionalCapabilitiesServiceClient::GetFallbackCountryId() {
  return country_id_;
}

void FakeRegionalCapabilitiesServiceClient::FetchCountryId(
    base::OnceCallback<void(CountryId)> on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(country_id_);
}

CountryId
FakeRegionalCapabilitiesServiceClient::GetVariationsLatestCountryId() {
  return country_id_;
}

namespace testing {}  // namespace testing

}  // namespace regional_capabilities
