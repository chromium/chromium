// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "regional_capabilities_country_id.h"
#include "regional_capabilities_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {

namespace {

class AsyncRegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit AsyncRegionalCapabilitiesServiceClient(
      int fallback_country_id = country_codes::kCountryIDUnknown)
      : fallback_country_id_(fallback_country_id) {}

  ~AsyncRegionalCapabilitiesServiceClient() override = default;

  int GetFallbackCountryId() override { return fallback_country_id_; }

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override {
    ASSERT_FALSE(cached_country_id_callback_) << "Test setup error";
    if (fetched_country_id_.has_value()) {
      std::move(country_id_fetched_callback).Run(fetched_country_id_.value());
    } else {
      // To be run next time we run `SetFetchedCountry()`;
      cached_country_id_callback_ = std::move(country_id_fetched_callback);
    }
  }

  void SetFetchedCountry(std::optional<int> fetched_country_id) {
    fetched_country_id_ = fetched_country_id;
    if (cached_country_id_callback_ && fetched_country_id_.has_value()) {
      std::move(cached_country_id_callback_).Run(fetched_country_id_.value());
    }
  }

  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const int fallback_country_id_;
  std::optional<int> fetched_country_id_;
  CountryIdCallback cached_country_id_callback_;

  base::WeakPtrFactory<AsyncRegionalCapabilitiesServiceClient>
      weak_ptr_factory_{this};
};

constexpr char kBelgiumCountryCode[] = "BE";

constexpr int kBelgiumCountryId =
    country_codes::CountryCharsToCountryID('B', 'E');

int GetCountryId(RegionalCapabilitiesService& service) {
  return service.GetCountryId().GetForTesting();
}

}  // namespace

class RegionalCapabilitiesServiceTest : public ::testing::Test {
 public:
  RegionalCapabilitiesServiceTest() {
    country_codes::RegisterProfilePrefs(pref_service_.registry());
  }

  ~RegionalCapabilitiesServiceTest() override = default;

  void ClearCommandLineCountry() {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSearchEngineChoiceCountry);
  }

  void SetCommandLineCountry(std::string_view country_code) {
    ClearCommandLineCountry();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, country_code);
  }

  std::optional<int> GetPrefCountry() {
    if (!pref_service().HasPrefPath(country_codes::kCountryIDAtInstall)) {
      return std::nullopt;
    }

    return pref_service().GetInteger(country_codes::kCountryIDAtInstall);
  }

  void SetPrefCountry(int country_id) {
    pref_service().SetInteger(country_codes::kCountryIDAtInstall, country_id);
  }

  std::unique_ptr<RegionalCapabilitiesService> InitService(
      int fallback_country_id = country_codes::kCountryIDUnknown) {
    auto client = std::make_unique<AsyncRegionalCapabilitiesServiceClient>(
        fallback_country_id);
    weak_client_ = client->AsWeakPtr();

    return std::make_unique<RegionalCapabilitiesService>(pref_service_,
                                                         std::move(client));
  }

  sync_preferences::TestingPrefServiceSyncable& pref_service() {
    return pref_service_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> client() {
    return weak_client_;
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> weak_client_;

  base::HistogramTester histogram_tester_;
};

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdCommandLineOverride) {
  // The command line value bypasses the country ID cache and does not
  // require recreating the service.
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  SetCommandLineCountry(kBelgiumCountryCode);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // If the format matches (2-character strings), we might get a
  // country ID that is not valid/supported.
  SetCommandLineCountry("??");
  EXPECT_NE(GetCountryId(*service), country_codes::kCountryIDUnknown);
  EXPECT_EQ(GetCountryId(*service),
            country_codes::CountryCharsToCountryID('?', '?'));

  // When the command line value is invalid, the country code should be unknown.
  SetCommandLineCountry("USA");
  EXPECT_EQ(GetCountryId(*service), country_codes::kCountryIDUnknown);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_FetchedSync) {
  const int kFallbackCountryId = country_codes::CountryStringToCountryID("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The fetched country is available synchronously, before `GetCountryId` was
  // invoked for the first time this run, so the new value should be used
  // right away.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  // The pref should be updated as well.
  EXPECT_EQ(GetPrefCountry(), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_FetchedAsyncUsesFallback) {
  const int kFallbackCountryId = country_codes::CountryStringToCountryID("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // We didn't get a response from the device API call before `GetCountryId`
  // was invoked, so the fallback country should be used.
  EXPECT_EQ(GetCountryId(*service), kFallbackCountryId);
  // The pref should not be updated.
  EXPECT_EQ(GetPrefCountry(), std::nullopt);

  // Simulate a response arriving after the first `GetCountryId` call.
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The pref should be updated so the new country can be used the next run.
  EXPECT_EQ(GetPrefCountry(), kBelgiumCountryId);
  // However, the `GetCountryId()` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(*service), kFallbackCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_PrefAlreadyWritten) {
  const int kFallbackCountryId = country_codes::CountryStringToCountryID("FR");
  const int kFetchedCountryId = country_codes::CountryStringToCountryID("US");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kFetchedCountryId);

  SetPrefCountry(kBelgiumCountryId);

  // The value set from the pref should be used instead of the ones from the
  // client.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // The fetched value from the client does not overwrite the prefs either.
  EXPECT_EQ(GetPrefCountry(), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_PrefChangesAfterReading) {
  const int kFallbackCountryId = country_codes::CountryStringToCountryID("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // The value set from the pref should be used.
  SetPrefCountry(kBelgiumCountryId);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // Change the value in pref.
  SetPrefCountry(country_codes::CountryStringToCountryID("US"));
  // The value returned by `GetCountryId` shouldn't change.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
}

// TODO: make it parameterized?

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(RegionalCapabilitiesServiceTest, ClearPrefForUnknownCountry) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kClearPrefForUnknownCountry};

  SetPrefCountry(country_codes::kCountryIDUnknown);
  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kBelgiumCountryId);

  // The fetch needs to succeed, otherwise the obtained value is the fallback
  // one and the pref will not be persisted.
  client()->SetFetchedCountry(kBelgiumCountryId);

  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  histogram_tester().ExpectUniqueSample(
      "Search.ChoiceDebug.UnknownCountryIdStored", 2 /* kClearedPref */, 1);
  EXPECT_EQ(GetPrefCountry(), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest, ClearPrefForUnknownCountry_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kClearPrefForUnknownCountry);

  SetPrefCountry(country_codes::kCountryIDUnknown);
  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kBelgiumCountryId);
  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), country_codes::kCountryIDUnknown);
  histogram_tester().ExpectUniqueSample(
      "Search.ChoiceDebug.UnknownCountryIdStored",
      1 /* kDontClearInvalidCountry */, 1);
  EXPECT_EQ(GetPrefCountry(), country_codes::kCountryIDUnknown);
}

TEST_F(RegionalCapabilitiesServiceTest, ClearPrefForUnknownCountry_Valid) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kClearPrefForUnknownCountry};

  SetPrefCountry(kBelgiumCountryId);
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  histogram_tester().ExpectUniqueSample(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0 /* kValidCountryId */, 1);
  EXPECT_EQ(GetPrefCountry(), kBelgiumCountryId);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_LINUX)

TEST_F(RegionalCapabilitiesServiceTest, IsInEeaCountry) {
  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kBelgiumCountryId);
  EXPECT_TRUE(service->IsInEeaCountry());

  SetCommandLineCountry("US");
  EXPECT_FALSE(service->IsInEeaCountry());

  SetCommandLineCountry(kBelgiumCountryCode);
  EXPECT_TRUE(service->IsInEeaCountry());

  // When --search-engine-choice-country is set to DEFAULT_EEA or EEA_ALL, the
  // country is always considered as being in the EEA.

  SetCommandLineCountry(switches::kDefaultListCountryOverride);
  EXPECT_TRUE(service->IsInEeaCountry());

  SetCommandLineCountry(switches::kEeaListCountryOverride);
  EXPECT_TRUE(service->IsInEeaCountry());
}

}  // namespace regional_capabilities
