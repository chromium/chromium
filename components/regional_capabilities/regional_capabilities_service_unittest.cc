// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "regional_capabilities_country_id.h"
#include "regional_capabilities_metrics.h"
#include "regional_capabilities_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/device_form_factor.h"

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

class AsyncRegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit AsyncRegionalCapabilitiesServiceClient(
      CountryId fallback_country_id = CountryId())
      : fallback_country_id_(fallback_country_id) {}

  ~AsyncRegionalCapabilitiesServiceClient() override = default;

  CountryId GetFallbackCountryId() override { return fallback_country_id_; }

  CountryId GetVariationsLatestCountryId() override { return CountryId(); }

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override {
    ASSERT_FALSE(cached_country_id_callback_) << "Test setup error";
    if (fetched_country_id_.has_value()) {
      std::move(country_id_fetched_callback).Run(fetched_country_id_.value());
    } else {
      // To be run next time we run `SetFetchedCountry()`;
      cached_country_id_callback_ = std::move(country_id_fetched_callback);
    }
  }

  void SetFetchedCountry(std::optional<CountryId> fetched_country_id) {
    fetched_country_id_ = fetched_country_id;
    if (cached_country_id_callback_ && fetched_country_id_.has_value()) {
      std::move(cached_country_id_callback_).Run(fetched_country_id_.value());
    }
  }

  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const CountryId fallback_country_id_;
  std::optional<CountryId> fetched_country_id_;
  CountryIdCallback cached_country_id_callback_;

  base::WeakPtrFactory<AsyncRegionalCapabilitiesServiceClient>
      weak_ptr_factory_{this};
};

constexpr char kBelgiumCountryCode[] = "BE";

constexpr CountryId kBelgiumCountryId = CountryId("BE");

CountryId GetCountryId(RegionalCapabilitiesService& service) {
  return service.GetCountryId().GetForTesting();
}

// Helper function to concatenate multiple `std::vector`s, intended for the
// parameterized test params.
template <typename Vec, typename... Vecs>
Vec Concatenate(const Vec& first, const Vecs&... rest) {
  Vec result;
  // Reserve space in the result vector to avoid multiple reallocations.
  result.reserve(first.size() + (rest.size() + ... + 0));

  // Insert the first vector, then a fold expression for the rest.
  result.insert(result.end(), first.begin(), first.end());
  (result.insert(result.end(), rest.begin(), rest.end()), ...);

  return result;
}

class RegionalCapabilitiesServiceTest : public ::testing::Test {
 public:
  RegionalCapabilitiesServiceTest() {
    feature_list_.InitWithFeatures({switches::kDynamicProfileCountry}, {});

    prefs::RegisterProfilePrefs(pref_service_.registry());
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

  std::optional<int> GetPrefSerializedCountryIDAtInstall() {
    if (!pref_service().HasPrefPath(prefs::kCountryIDAtInstall)) {
      return std::nullopt;
    }

    return pref_service().GetInteger(prefs::kCountryIDAtInstall);
  }

  std::optional<int> GetPrefSerializedCountryID() {
    if (!pref_service().HasPrefPath(prefs::kCountryID)) {
      return std::nullopt;
    }

    return pref_service().GetInteger(prefs::kCountryID);
  }

  void SetPrefCountryIDAtInstall(CountryId country_id) {
    pref_service().SetInteger(prefs::kCountryIDAtInstall,
                              country_id.Serialize());
  }

  void SetPrefCountryID(CountryId country_id) {
    pref_service().SetInteger(prefs::kCountryID, country_id.Serialize());
  }

  std::unique_ptr<RegionalCapabilitiesService> InitService(
      CountryId fallback_country_id = CountryId()) {
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
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> weak_client_;

  base::HistogramTester histogram_tester_;
};

template <typename T>
class RegionalCapabilitiesServiceTestWithParam
    : public RegionalCapabilitiesServiceTest,
      public testing::WithParamInterface<T> {
 public:
  static std::string GetTestName(const testing::TestParamInfo<T>& info) {
    return info.param.test_name;
  }
};

struct ActiveProgramFromOverrideTestParam {
  std::string test_name;
  std::string country_override;
  Program expected_program;
};

using RegionalCapabilitiesServiceActiveProgramFromOverrideTest =
    RegionalCapabilitiesServiceTestWithParam<
        ActiveProgramFromOverrideTestParam>;

TEST_P(RegionalCapabilitiesServiceActiveProgramFromOverrideTest, Run) {
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  SetCommandLineCountry(GetParam().country_override);
  EXPECT_EQ(GetParam().expected_program, service->GetActiveProgramForTesting());
}

const std::vector<ActiveProgramFromOverrideTestParam>
    kActiveProgramFromOverrideCommonTestCases = {
        ActiveProgramFromOverrideTestParam{
            .test_name = "fr_to_waffle",
            .country_override = "FR",
            .expected_program = Program::kWaffle,
        },
        ActiveProgramFromOverrideTestParam{
            .test_name = "us_to_default",
            .country_override = "US",
            .expected_program = Program::kDefault,
        },
        ActiveProgramFromOverrideTestParam{
            .test_name = "err_to_default",
            .country_override = "??",
            .expected_program = Program::kDefault,
        },
        ActiveProgramFromOverrideTestParam{
            .test_name = "default_eea_list",
            .country_override = switches::kDefaultListCountryOverride,
            .expected_program = Program::kWaffle,
        },
        ActiveProgramFromOverrideTestParam{
            .test_name = "full_eea_list",
            .country_override = switches::kEeaListCountryOverride,
            .expected_program = Program::kWaffle,
        },

};

INSTANTIATE_TEST_SUITE_P(
    ,
    RegionalCapabilitiesServiceActiveProgramFromOverrideTest,
    ::testing::ValuesIn(
        Concatenate(kActiveProgramFromOverrideCommonTestCases,
                    std::vector<ActiveProgramFromOverrideTestParam>{
                        ActiveProgramFromOverrideTestParam{
                            .test_name = "jp_to_default",
                            .country_override = "JP",
                            .expected_program = Program::kDefault,
                        },
                    })),
    &RegionalCapabilitiesServiceActiveProgramFromOverrideTest::GetTestName);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
bool IsIPhone() {
#if BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
#else
  return false;
#endif
}

class RegionalCapabilitiesServiceActiveProgramFromOverrideTaiyakiForcedTest
    : public RegionalCapabilitiesServiceTestWithParam<
          ActiveProgramFromOverrideTestParam> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{switches::kTaiyaki};
};

TEST_P(RegionalCapabilitiesServiceActiveProgramFromOverrideTaiyakiForcedTest,
       Run) {
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  SetCommandLineCountry(GetParam().country_override);
  EXPECT_EQ(GetParam().expected_program, service->GetActiveProgramForTesting());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RegionalCapabilitiesServiceActiveProgramFromOverrideTaiyakiForcedTest,
    ::testing::ValuesIn(
        Concatenate(kActiveProgramFromOverrideCommonTestCases,
                    std::vector<ActiveProgramFromOverrideTestParam>{
                        ActiveProgramFromOverrideTestParam{
                            .test_name = "jp_to_taiyaki",
                            .country_override = "JP",
                            .expected_program = IsIPhone() ? Program::kTaiyaki
                                                           : Program::kDefault,
                        },
                    })),
    &RegionalCapabilitiesServiceActiveProgramFromOverrideTaiyakiForcedTest::
        GetTestName);
#endif

TEST_F(RegionalCapabilitiesServiceTest, GetCountryIdCommandLineOverride) {
  // The command line value bypasses the country ID cache and does not
  // require recreating the service.
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  SetCommandLineCountry(kBelgiumCountryCode);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // When the command line value is not two uppercase basic Latin alphabet
  // characters, the country code should not be valid.
  SetCommandLineCountry("??");
  EXPECT_FALSE(GetCountryId(*service).IsValid());

  SetCommandLineCountry("us");
  EXPECT_FALSE(GetCountryId(*service).IsValid());

  SetCommandLineCountry("USA");
  EXPECT_FALSE(GetCountryId(*service).IsValid());

  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FetchedCountryMatching", 0);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FallbackCountryMatching", 0);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.PersistedCountryMatching", 0);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.LoadedCountrySource", 0);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_FetchedSync) {
  const auto kFallbackCountryId = CountryId("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The fetched country is available synchronously, before `GetCountryId` was
  // invoked for the first time this run, so the new value should be used
  // right away.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  // The prefs should be updated as well.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());

  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FetchedCountryMatching", 2, 1);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FallbackCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kCurrentOnly), 1);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_FetchedAsyncUsesFallback) {
  const auto kFallbackCountryId = CountryId("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // We didn't get a response from the device API call before `GetCountryId`
  // was invoked, so the fallback country should be used.
  EXPECT_EQ(GetCountryId(*service), kFallbackCountryId);
  // The pref should not be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(), std::nullopt);
  EXPECT_EQ(GetPrefSerializedCountryID(), std::nullopt);

  // Simulate a response arriving after the first `GetCountryId` call.
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The prefs should be updated so the new country can be used the next run.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
  // However, the `GetCountryId()` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(*service), kFallbackCountryId);

  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FetchedCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FallbackCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching", 1 /* kCountryMissing */,
      1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kCurrentOnly), 1);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_FetchedAsyncUsesPref) {
  const auto kFallbackCountryId = CountryId("FR");
  const auto kGermanyCountryId = CountryId("DE");
  const auto kPolandCountryId = CountryId("PL");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // CountryID pref is preferred over CountryID at install pref.
  SetPrefCountryIDAtInstall(kPolandCountryId);
  SetPrefCountryID(kGermanyCountryId);

  // We didn't get a response from the device API call before `GetCountryId`
  // was invoked, so the fallback country should be used.
  EXPECT_EQ(GetCountryId(*service), kGermanyCountryId);
  // The pref should NOT be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  EXPECT_EQ(GetPrefSerializedCountryID(), kGermanyCountryId.Serialize());

  // Simulate a response arriving after the first `GetCountryId` call.
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The CountryID at install pref should NOT be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  // The CountryID pref should be updated so the new country can be used the
  // next run.
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
  // However, the `GetCountryId()` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(*service), kGermanyCountryId);

  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FetchedCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FallbackCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kPersistedPreferredOverFallback),
      1);
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryId_FetchedAsyncUsesPref_CountryIDPrefUnset) {
  const auto kFallbackCountryId = CountryId("FR");
  const auto kPolandCountryId = CountryId("PL");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // Static pref is preferred because the dynamic pref is unset.
  SetPrefCountryIDAtInstall(kPolandCountryId);

  // We didn't get a response from the device API call before `GetCountryId`
  // was invoked, so the persisted country should be used.
  EXPECT_EQ(GetCountryId(*service), kPolandCountryId);
  // The static pref should NOT be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  EXPECT_EQ(GetPrefSerializedCountryID(), std::nullopt);

  // Simulate a response arriving after the first `GetCountryId` call.
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The CountryID at install pref should NOT be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  // The CountryID pref should be initialised so the new country can be used the
  // next run.
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
  // However, the `GetCountryId()` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(*service), kPolandCountryId);

  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FetchedCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FallbackCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kPersistedPreferredOverFallback),
      1);
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryId_FetchedAsyncUsesPref_CountryIDPrefInvalid) {
  const auto kFallbackCountryId = CountryId("FR");
  const auto kPolandCountryId = CountryId("PL");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // Static pref is preferred because the dynamic pref is invalid.
  SetPrefCountryIDAtInstall(kPolandCountryId);
  SetPrefCountryID(CountryId("usa"));

  // We didn't get a response from the device API call before `GetCountryId`
  // was invoked, so the fallback country should be used.
  EXPECT_EQ(GetCountryId(*service), kPolandCountryId);
  // The static pref should NOT be updated as it is valid.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  // The dynamic pref should be cleared.
  EXPECT_EQ(GetPrefSerializedCountryID(), std::nullopt);

  // Simulate a response arriving after the first `GetCountryId` call.
  client()->SetFetchedCountry(kBelgiumCountryId);

  // The CountryID at install pref should NOT be updated.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kPolandCountryId.Serialize());
  // The CountryID pref should be initialised so the new country can be used the
  // next run.
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
  // However, the `GetCountryId()` result shouldn't change until the next run.
  EXPECT_EQ(GetCountryId(*service), kPolandCountryId);

  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FetchedCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FallbackCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching",
      2 /* kVariationsCountryMissing */, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kPersistedPreferredOverFallback),
      1);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_PrefAlreadyWritten) {
  const auto kFallbackCountryId = CountryId("FR");
  const auto kFetchedCountryId = CountryId("US");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kFetchedCountryId);

  SetPrefCountryIDAtInstall(kBelgiumCountryId);
  SetPrefCountryID(kBelgiumCountryId);

  // The fetched value should be used instead of the ones from the pref.
  EXPECT_EQ(GetCountryId(*service), kFetchedCountryId);

  // The fetched value from the client does not overwrites CountryID at install
  // pref.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());

  // The fetched value from the client initialise CountryID pref.
  EXPECT_EQ(GetPrefSerializedCountryID(), kFetchedCountryId.Serialize());

  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FetchedCountryMatching", 2, 1);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FallbackCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kCurrentPreferred), 1);
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryId_PrefAlreadyWritten_DynamicProfileCountryIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {switches::kDynamicProfileCountry});

  const auto kFallbackCountryId = CountryId("FR");
  const auto kFetchedCountryId = CountryId("US");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kFetchedCountryId);

  SetPrefCountryIDAtInstall(kBelgiumCountryId);

  // The value set from the pref should be used instead of the ones from the
  // client.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // The fetched value from the client does not overwrite the prefs either.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());

  // The fetched value from the client does NOT initialise CountryID pref as
  // the kDynamicProfileCountry feature flag is disabled.
  EXPECT_EQ(GetPrefSerializedCountryID(), std::nullopt);

  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FetchedCountryMatching", 2, 1);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FallbackCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kPersistedPreferred), 1);
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryId_BothPrefsAlreadyWritten_DynamicProfileCountryIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {switches::kDynamicProfileCountry});

  const auto kFallbackCountryId = CountryId("FR");
  const auto kFetchedCountryId = CountryId("US");
  const auto kGermanyCountryId = CountryId("DE");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);
  client()->SetFetchedCountry(kFetchedCountryId);

  SetPrefCountryIDAtInstall(kBelgiumCountryId);
  // Make sure that this pref is ignored when kDynamicProfileCountry is
  // disabled.
  SetPrefCountryID(kGermanyCountryId);

  // The value set from the pref should be used instead of the ones from the
  // client.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // The fetched value from the client does not overwrite the prefs either.
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());

  // The fetched value from the client does NOT update CountryID pref as
  // the kDynamicProfileCountry feature flag is disabled.
  EXPECT_EQ(GetPrefSerializedCountryID(), kGermanyCountryId.Serialize());

  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FetchedCountryMatching", 2, 1);
  histogram_tester().ExpectTotalCount(
      "RegionalCapabilities.FallbackCountryMatching", 0);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.PersistedCountryMatching", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.LoadedCountrySource",
      static_cast<int>(LoadedCountrySource::kPersistedPreferred), 1);
}

TEST_F(RegionalCapabilitiesServiceTest, GetCountryId_PrefChangesAfterReading) {
  const auto kFallbackCountryId = CountryId("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // The value set from the pref should be used.
  SetPrefCountryID(kBelgiumCountryId);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // Change the value in pref.
  SetPrefCountryID(CountryId("US"));
  // The value returned by `GetCountryId` shouldn't change.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryId_AtInstallPrefChangesAfterReading) {
  const auto kFallbackCountryId = CountryId("FR");

  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kFallbackCountryId);

  // The value set from the pref should be used.
  SetPrefCountryIDAtInstall(kBelgiumCountryId);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  // Change the value in pref.
  SetPrefCountryIDAtInstall(CountryId("US"));
  // The value returned by `GetCountryId` shouldn't change.
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceTest,
       ClearPrefForUnknownCountry_BothPrefsInvalid) {
  SetPrefCountryIDAtInstall(CountryId());
  SetPrefCountryID(CountryId());
  std::unique_ptr<RegionalCapabilitiesService> service =
      InitService(kBelgiumCountryId);

  // The fetch needs to succeed, otherwise the obtained value is the fallback
  // one and the pref will not be persisted.
  client()->SetFetchedCountry(kBelgiumCountryId);

  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Search.ChoiceDebug.UnknownCountryIdStored"),
      testing::ElementsAre(base::Bucket(2 /* kClearedPref */, 1),
                           base::Bucket(4 /* kClearedDynamicPref */, 1)));

  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
}

TEST_F(RegionalCapabilitiesServiceTest,
       ClearPrefForUnknownCountry_StaticValid) {
  SetPrefCountryIDAtInstall(kBelgiumCountryId);
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  histogram_tester().ExpectUniqueSample(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0 /* kValidCountryId */, 1);
  EXPECT_EQ(GetPrefSerializedCountryIDAtInstall(),
            kBelgiumCountryId.Serialize());
}

TEST_F(RegionalCapabilitiesServiceTest,
       ClearPrefForUnknownCountry_DynamicValid) {
  SetPrefCountryID(kBelgiumCountryId);
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  histogram_tester().ExpectTotalCount(
      "Search.ChoiceDebug.UnknownCountryIdStored", 0);

  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  histogram_tester().ExpectUniqueSample(
      "Search.ChoiceDebug.UnknownCountryIdStored",
      3 /* kValidDynamicCountryId */, 1);
  EXPECT_EQ(GetPrefSerializedCountryID(), kBelgiumCountryId.Serialize());
}

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

}  // namespace
}  // namespace regional_capabilities
