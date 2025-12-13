// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_test_utils.h"

#include <memory>
#include <string_view>
#include <variant>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace regional_capabilities {

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId country_id) {
  return std::make_unique<RegionalCapabilitiesService>(
      profile_prefs,
      std::make_unique<FakeRegionalCapabilitiesServiceClient>(country_id));
}

std::unique_ptr<RegionalCapabilitiesService> CreateServiceWithFakeClient(
    PrefService& profile_prefs,
    CountryId variations_latest_country_id,
    CountryId fetched_country_id,
    CountryId fallback_country_id) {
  return std::make_unique<RegionalCapabilitiesService>(
      profile_prefs, std::make_unique<FakeRegionalCapabilitiesServiceClient>(
                         variations_latest_country_id, fetched_country_id,
                         fallback_country_id));
}

FakeRegionalCapabilitiesServiceClient::FakeRegionalCapabilitiesServiceClient(
    CountryId country_id)
    : FakeRegionalCapabilitiesServiceClient(
          /* variations_latest_country_id= */ country_id,
          /* fetched_country_id= */ country_id,
          /* fallback_country_id= */ country_id) {}

FakeRegionalCapabilitiesServiceClient::FakeRegionalCapabilitiesServiceClient(
    CountryId variations_latest_country_id,
    CountryId fetched_country_id,
    CountryId fallback_country_id)
    : variations_latest_country_id_(variations_latest_country_id),
      fetched_country_id_(fetched_country_id),
      fallback_country_id_(fallback_country_id) {}

FakeRegionalCapabilitiesServiceClient::
    ~FakeRegionalCapabilitiesServiceClient() = default;

CountryId
FakeRegionalCapabilitiesServiceClient::GetVariationsLatestCountryId() {
  return variations_latest_country_id_;
}

void FakeRegionalCapabilitiesServiceClient::FetchCountryId(
    base::OnceCallback<void(CountryId)> on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(fetched_country_id_);
}

CountryId FakeRegionalCapabilitiesServiceClient::GetFallbackCountryId() {
  return fallback_country_id_;
}

#if BUILDFLAG(IS_ANDROID)
Program FakeRegionalCapabilitiesServiceClient::GetDeviceProgram() {
  return Program::kDefault;
}
#endif

FakeRegionalCapabilitiesServiceClient&
FakeRegionalCapabilitiesServiceClient::SetVariationsLatestCountryId(
    CountryId country_id) {
  variations_latest_country_id_ = country_id;
  return *this;
}

FakeRegionalCapabilitiesServiceClient&
FakeRegionalCapabilitiesServiceClient::SetFetchedCountryId(
    CountryId country_id) {
  fetched_country_id_ = country_id;
  return *this;
}

FakeRegionalCapabilitiesServiceClient&
FakeRegionalCapabilitiesServiceClient::SetFallbackCountryId(
    CountryId country_id) {
  fallback_country_id_ = country_id;
  return *this;
}

void CheckHistogramExpectation(const base::HistogramTester& histogram_tester,
                               std::string_view histogram_name,
                               const HistogramExpectation& expectation,
                               const base::Location& location) {
  std::visit(absl::Overload{
                 [&](const base::HistogramBase::Count32& expected_total_count) {
                   histogram_tester.ExpectTotalCount(
                       histogram_name, expected_total_count, location);
                 },
                 [&](const std::tuple<base::HistogramBase::Sample32,
                                      base::HistogramBase::Count32, bool>&
                         expected_samples) {
                   if (std::get<2>(expected_samples)) {
                     histogram_tester.ExpectUniqueSample(
                         histogram_name, std::get<0>(expected_samples),
                         std::get<1>(expected_samples), location);
                   } else {
                     histogram_tester.ExpectBucketCount(
                         histogram_name, std::get<0>(expected_samples),
                         std::get<1>(expected_samples), location);
                   }
                 },
             },
             expectation);
}

}  // namespace regional_capabilities
