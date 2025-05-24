// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

// The API & structure of the component is not yet stable. Until it stabilizes,
// we forward this include here so tests can keep referencing
// `kEeaChoiceCountriesIds`.
#include "components/regional_capabilities/eea_countries_ids.h"  // IWYU pragma: export

namespace regional_capabilities {

using country_codes::CountryId;

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId country_id = CountryId());

class FakeRegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit FakeRegionalCapabilitiesServiceClient(
      CountryId country_id = CountryId());

  FakeRegionalCapabilitiesServiceClient(
      const FakeRegionalCapabilitiesServiceClient&) = delete;
  FakeRegionalCapabilitiesServiceClient& operator=(
      const FakeRegionalCapabilitiesServiceClient&) = delete;

  ~FakeRegionalCapabilitiesServiceClient() override;

  void FetchCountryId(
      base::OnceCallback<void(CountryId)> on_country_id_fetched) override;

  CountryId GetFallbackCountryId() override;

  CountryId GetVariationsLatestCountryId() override;

 private:
  const CountryId country_id_;
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_
