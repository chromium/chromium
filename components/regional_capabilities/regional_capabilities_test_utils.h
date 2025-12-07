// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_

#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_base.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

// The API & structure of the component is not yet stable. Until it stabilizes,
// we forward this include here so tests can keep referencing
// `kEeaChoiceCountriesIds`.
#include "components/regional_capabilities/eea_countries_ids.h"  // IWYU pragma: export

namespace base {
class HistogramTester;
class Location;
}  // namespace base

namespace regional_capabilities {

using country_codes::CountryId;

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId country_id = CountryId());

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId variations_latest_country_id,
    CountryId fetched_country_id,
    CountryId fallback_country_id);

class FakeRegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit FakeRegionalCapabilitiesServiceClient(
      CountryId country_id = CountryId());
  FakeRegionalCapabilitiesServiceClient(CountryId variations_latest_country_id,
                                        CountryId fetched_country_id,
                                        CountryId fallback_country_id);

  FakeRegionalCapabilitiesServiceClient(
      const FakeRegionalCapabilitiesServiceClient&) = delete;
  FakeRegionalCapabilitiesServiceClient& operator=(
      const FakeRegionalCapabilitiesServiceClient&) = delete;

  ~FakeRegionalCapabilitiesServiceClient() override;

  // RegionalCapabilitiesService::Client:
  CountryId GetVariationsLatestCountryId() override;
  void FetchCountryId(
      base::OnceCallback<void(CountryId)> on_country_id_fetched) override;
  CountryId GetFallbackCountryId() override;
#if BUILDFLAG(IS_ANDROID)
  Program GetDeviceProgram() override;
#endif

  // Internal state setters. They return a reference to the current object for
  // chaining convenience.
  FakeRegionalCapabilitiesServiceClient& SetVariationsLatestCountryId(
      CountryId country_id);
  FakeRegionalCapabilitiesServiceClient& SetFetchedCountryId(
      CountryId country_id);
  FakeRegionalCapabilitiesServiceClient& SetFallbackCountryId(
      CountryId country_id);

 private:
  CountryId variations_latest_country_id_;
  CountryId fetched_country_id_;
  CountryId fallback_country_id_;
};

using HistogramExpectation =
    std::variant<base::HistogramBase::Count32,
                 std::tuple<base::HistogramBase::Sample32,
                            base::HistogramBase::Count32,
                            bool>>;

inline HistogramExpectation ExpectHistogramNever() {
  return 0;
}

template <typename T>
HistogramExpectation ExpectHistogramBucket(
    T sample,
    base::HistogramBase::Count32 count = 1) {
  return std::make_tuple(static_cast<base::HistogramBase::Sample32>(sample),
                         count, /* unique= */ false);
}

template <typename T>
HistogramExpectation ExpectHistogramUnique(
    T sample,
    base::HistogramBase::Count32 count = 1) {
  return std::make_tuple(static_cast<base::HistogramBase::Sample32>(sample),
                         count, /* unique= */ false);
}

void CheckHistogramExpectation(const base::HistogramTester& histogram_tester,
                               std::string_view histogram_name,
                               const HistogramExpectation& expectation,
                               const base::Location& location = FROM_HERE);

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_TEST_UTILS_H_
