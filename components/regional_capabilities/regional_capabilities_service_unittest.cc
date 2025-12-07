// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/strcat.h"
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

namespace regional_capabilities {

void PrintTo(const Program& program, std::ostream* os) {
  switch (program) {
    case Program::kDefault:
      *os << "kDefault";
      break;
    case Program::kTaiyaki:
      *os << "kTaiyaki";
      break;
    case Program::kWaffle:
      *os << "kWaffle";
      break;
  }
}

namespace {

using ::country_codes::CountryId;
using ::testing::get;

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
const ui::DeviceFormFactorSet kPhoneFormFactors{
    ui::DEVICE_FORM_FACTOR_PHONE, ui::DEVICE_FORM_FACTOR_FOLDABLE};
const ui::DeviceFormFactorSet kNonPhoneFormFactors =
    base::Difference(ui::DeviceFormFactorSet::All(), kPhoneFormFactors);
#endif

class AsyncRegionalCapabilitiesServiceClient
    : public RegionalCapabilitiesService::Client {
 public:
  explicit AsyncRegionalCapabilitiesServiceClient(
      CountryId fallback_country_id = CountryId())
      : fallback_country_id_(fallback_country_id) {}

  ~AsyncRegionalCapabilitiesServiceClient() override = default;

  CountryId GetFallbackCountryId() override { return fallback_country_id_; }

  CountryId GetVariationsLatestCountryId() override {
    return variations_latest_country_id_;
  }

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override {
    ASSERT_FALSE(cached_country_id_callback_) << "Test setup error";
    if (fetched_country_id_.has_value()) {
      std::move(country_id_fetched_callback).Run(fetched_country_id_.value());
    } else {
      // To be run next time we run `SetFetchedCountry()`;
      cached_country_id_callback_ = std::move(country_id_fetched_callback);
    }
  }

#if BUILDFLAG(IS_ANDROID)
  Program GetDeviceProgram() override { return device_program_; }
  void SetDeviceProgram(Program device_program) {
    device_program_ = device_program;
  }
#endif

  void SetFetchedCountry(std::optional<CountryId> fetched_country_id) {
    fetched_country_id_ = fetched_country_id;
    if (cached_country_id_callback_ && fetched_country_id_.has_value()) {
      std::move(cached_country_id_callback_).Run(fetched_country_id_.value());
    }
  }

  void SetVariationsLatestCountry(CountryId variations_latest_country_id) {
    variations_latest_country_id_ = variations_latest_country_id;
  }

  base::WeakPtr<AsyncRegionalCapabilitiesServiceClient> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const CountryId fallback_country_id_;
  CountryId variations_latest_country_id_;
  std::optional<CountryId> fetched_country_id_;
  CountryIdCallback cached_country_id_callback_;

#if BUILDFLAG(IS_ANDROID)
  Program device_program_ = Program::kDefault;
#endif

  base::WeakPtrFactory<AsyncRegionalCapabilitiesServiceClient>
      weak_ptr_factory_{this};
};

constexpr char kBelgiumCountryCode[] = "BE";

constexpr CountryId kBelgiumCountryId = CountryId("BE");

CountryId GetCountryId(RegionalCapabilitiesService& service) {
  return service.GetCountryId().GetForTesting();
}

Program GetActiveProgram(RegionalCapabilitiesService& service) {
  return service.GetActiveProgramSettingsForTesting().program;
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
      CountryId fallback_country_id = CountryId()
#if BUILDFLAG(IS_ANDROID)
          ,
      Program device_program = Program::kDefault
#endif  // BUILDFLAG(IS_ANDROID)
  ) {
    auto client = std::make_unique<AsyncRegionalCapabilitiesServiceClient>(
        fallback_country_id);
    weak_client_ = client->AsWeakPtr();
#if BUILDFLAG(IS_ANDROID)
    client->SetDeviceProgram(device_program);
#endif  // BUILDFLAG(IS_ANDROID)

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

struct ProgramDeterminationTestParam {
  // Identifier of the test, will be used as parameterized test name suffix.
  std::string test_name;

  // When non-empty, skips the test when the current form factor is not in the
  // provided set.
  ui::DeviceFormFactorSet run_only_on;

  // Country to apply for the current run. Will be set on the
  // `RegionalCapabilitiesServiceClient`.
  country_codes::CountryId client_fetched_country;

  // If valid, will be injected to the service through its client's
  // `GetVariationsLatestCountryId()`.
  country_codes::CountryId variations_latest_country_id;

#if BUILDFLAG(IS_ANDROID)
  Program device_program_override = Program::kDefault;
#endif

  // -- Expectations ----------------------------------------------------------
  Program expected_program;
  std::optional<bool> expected_is_in_choice_screen_region = std::nullopt;
  std::optional<SearchEngineListType> expected_ose_list_type = std::nullopt;
  base::flat_map<std::string, HistogramExpectation> expected_histograms;
};

using TestParamWithTaiyakiFeatureState =
    ::testing::tuple<ProgramDeterminationTestParam, bool>;

auto WithCompatibleTaiyakiFeatureState(
    std::vector<ProgramDeterminationTestParam> params_to_combine) {
  return ::testing::Combine(
      ::testing::ValuesIn(params_to_combine),
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
      ::testing::Bool()  // All feature states are supported
#else
      ::testing::Values(
          false)  // The feature is not supported, consider it only disabled.
#endif
  );
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
auto WithTaiyakiFeatureState(
    std::vector<ProgramDeterminationTestParam> params_for_enabled,
    std::vector<ProgramDeterminationTestParam> params_for_disabled) {
  std::vector<TestParamWithTaiyakiFeatureState> output;
  for (auto& param : params_for_enabled) {
    output.emplace_back(param, true);
  }
  for (auto& param : params_for_disabled) {
    output.emplace_back(param, false);
  }
  return ::testing::ValuesIn(output);
}
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

class RegionalCapabilitiesServiceProgramDeterminationTest
    : public RegionalCapabilitiesServiceTest,
      public testing::WithParamInterface<TestParamWithTaiyakiFeatureState> {
 public:
  RegionalCapabilitiesServiceProgramDeterminationTest() {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitWithFeatureState(switches::kTaiyaki,
                                              GetTaiyakiFeatureEnabled());
#else
    EXPECT_FALSE(GetTaiyakiFeatureEnabled());
    scoped_feature_list_.Init();
#endif
  }

  static std::string GetTestName(
      const testing::TestParamInfo<TestParamWithTaiyakiFeatureState>& info) {
    return base::StrCat(
        {get<0>(info.param).test_name,
         get<1>(info.param) ? "_taiyaki_enabled" : "_taiyaki_disabled"});
  }

  void SetUp() override {
    if (auto run_only_on = GetTestParam().run_only_on;
        !run_only_on.empty() && !run_only_on.Has(ui::GetDeviceFormFactor())) {
      GTEST_SKIP();
    }
  }

  ProgramDeterminationTestParam GetTestParam() { return get<0>(GetParam()); }

  bool GetTaiyakiFeatureEnabled() { return get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(RegionalCapabilitiesServiceProgramDeterminationTest, Run) {
  std::unique_ptr<RegionalCapabilitiesService> service = InitService(
      /* fallback_country_id= */ CountryId()
#if BUILDFLAG(IS_ANDROID)
          ,
      /* device_program= */ GetTestParam().device_program_override
#endif  // BUILDFLAG(IS_ANDROID)
  );
  if (GetTestParam().variations_latest_country_id.IsValid()) {
    client()->SetVariationsLatestCountry(
        GetTestParam().variations_latest_country_id);
  }
  client()->SetFetchedCountry(GetTestParam().client_fetched_country);

  EXPECT_EQ(GetTestParam().expected_program,
            service->GetActiveProgramSettingsForTesting().program);

  if (GetTestParam().expected_is_in_choice_screen_region.has_value()) {
    EXPECT_EQ(GetTestParam().expected_is_in_choice_screen_region.value(),
              service->IsInSearchEngineChoiceScreenRegion());
  }

  if (GetTestParam().expected_ose_list_type.has_value()) {
    EXPECT_EQ(
        GetTestParam().expected_ose_list_type.value(),
        service->GetActiveProgramSettingsForTesting().search_engine_list_type);
  }

  for (const auto& [histogram, expectation] :
       GetTestParam().expected_histograms) {
    CheckHistogramExpectation(histogram_tester(), histogram, expectation);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Common,
    RegionalCapabilitiesServiceProgramDeterminationTest,
    WithCompatibleTaiyakiFeatureState({
        ProgramDeterminationTestParam{
            .test_name = "fr_to_waffle",
            .client_fetched_country = CountryId("FR"),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kWaffle,
#endif  // BUILDFLAG(IS_ANDROID)
            .expected_program = Program::kWaffle,
            .expected_is_in_choice_screen_region = true,
            .expected_ose_list_type = SearchEngineListType::kShuffled,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(LoadedCountrySource::kCurrentOnly)},
                },
        },
#if BUILDFLAG(IS_ANDROID)
        ProgramDeterminationTestParam{
            .test_name = "fr_to_default",
            .client_fetched_country = CountryId("FR"),
            .device_program_override = Program::kDefault,
            .expected_program = Program::kDefault,
            .expected_is_in_choice_screen_region = false,
            .expected_ose_list_type = SearchEngineListType::kTopN,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(LoadedCountrySource::kCurrentOnly)},
                },
        },
        ProgramDeterminationTestParam{
            // Waffle is not compatible with the USA, so instead choice screen
            // settings are defaulted.
            .test_name = "us_ignores_waffle",
            .client_fetched_country = CountryId("US"),
            .device_program_override = Program::kWaffle,
            .expected_program = Program::kDefault,
            .expected_is_in_choice_screen_region = false,
            .expected_ose_list_type = SearchEngineListType::kTopN,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(LoadedCountrySource::kCurrentOnly)},
                },
        },
#endif  // BUILDFLAG(IS_ANDROID)

        ProgramDeterminationTestParam{
            .test_name = "us_to_default",
            .client_fetched_country = CountryId("US"),
            .expected_program = Program::kDefault,
            .expected_is_in_choice_screen_region = false,
            .expected_ose_list_type = SearchEngineListType::kTopN,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(LoadedCountrySource::kCurrentOnly)},
                },
        },
        ProgramDeterminationTestParam{
            .test_name = "err_to_default",
            .client_fetched_country = CountryId("??"),
            .expected_program = Program::kDefault,
            .expected_is_in_choice_screen_region = false,
            .expected_ose_list_type = SearchEngineListType::kTopN,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(
                         LoadedCountrySource::kNoneAvailable)},
                },
        },
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
        ProgramDeterminationTestParam{
            .test_name = "jp_to_default",
            .client_fetched_country = CountryId("JP"),
            .expected_program = Program::kDefault,
            .expected_is_in_choice_screen_region = false,
            .expected_ose_list_type = SearchEngineListType::kTopN,
            .expected_histograms =
                {
                    {"RegionalCapabilities.LoadedCountrySource",
                     ExpectHistogramBucket(LoadedCountrySource::kCurrentOnly)},
                },
        },
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
    }),
    &RegionalCapabilitiesServiceProgramDeterminationTest::GetTestName);

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(
    FeatureStateSpecific,
    RegionalCapabilitiesServiceProgramDeterminationTest,
    WithTaiyakiFeatureState(
        /*params_for_enabled=*/
        {
#if BUILDFLAG(IS_IOS)
            ProgramDeterminationTestParam{
                .test_name = "jp_to_taiyaki",
                .run_only_on = kPhoneFormFactors,
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kTaiyaki,
                .expected_is_in_choice_screen_region = true,
                .expected_ose_list_type = SearchEngineListType::kShuffled,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
            ProgramDeterminationTestParam{
                .test_name = "jp_to_default_non_phone",
                .run_only_on = kNonPhoneFormFactors,
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kDefault,
                .expected_is_in_choice_screen_region = false,
                .expected_ose_list_type = SearchEngineListType::kTopN,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_ANDROID)
            ProgramDeterminationTestParam{
                .test_name = "jp_to_default",
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kDefault,
                .expected_is_in_choice_screen_region = false,
                .expected_ose_list_type = SearchEngineListType::kTopN,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
#endif  // BUILDFLAG(IS_ANDROID)
        },
        /*params_for_disabled=*/
        {
#if BUILDFLAG(IS_IOS)
            ProgramDeterminationTestParam{
                .test_name = "jp_to_taiyaki",
                .run_only_on = kPhoneFormFactors,
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kTaiyaki,
                .expected_is_in_choice_screen_region = false,
                .expected_ose_list_type = SearchEngineListType::kTopN,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
            ProgramDeterminationTestParam{
                .test_name = "jp_to_default_non_phone",
                .run_only_on = kNonPhoneFormFactors,
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kDefault,
                .expected_is_in_choice_screen_region = false,
                .expected_ose_list_type = SearchEngineListType::kTopN,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_ANDROID)
            ProgramDeterminationTestParam{
                .test_name = "jp_to_default",
                .client_fetched_country = CountryId("JP"),
                .expected_program = Program::kDefault,
                .expected_is_in_choice_screen_region = false,
                .expected_ose_list_type = SearchEngineListType::kTopN,
                .expected_histograms =
                    {
                        {"RegionalCapabilities.LoadedCountrySource",
                         ExpectHistogramBucket(
                             LoadedCountrySource::kCurrentOnly)},
                    },
            },
#endif  // BUILDFLAG(IS_ANDROID)
        }),
    &RegionalCapabilitiesServiceProgramDeterminationTest::GetTestName);
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(
    RegionalPresence,
    RegionalCapabilitiesServiceProgramDeterminationTest,
    WithCompatibleTaiyakiFeatureState({
        ProgramDeterminationTestParam{
            .test_name = "in_scope_and_variations_country_match",
            .client_fetched_country = CountryId("FR"),
            .variations_latest_country_id = CountryId("FR"),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kWaffle,
#endif
            .expected_program = Program::kWaffle,
            .expected_histograms =
                {
                    {"RegionalCapabilities.FunnelStage.RegionalPresence",
                     ExpectHistogramBucket(
                         ProgramAndLocationMatch::SameAsProfileCountry)},
                },
        },
        ProgramDeterminationTestParam{
            .test_name = "in_scope_and_variations_country_in_region",
            .client_fetched_country = CountryId("FR"),
            .variations_latest_country_id = CountryId("DE"),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kWaffle,
#endif
            .expected_program = Program::kWaffle,
            .expected_histograms =
                {
                    {"RegionalCapabilities.FunnelStage.RegionalPresence",
                     ExpectHistogramBucket(
                         ProgramAndLocationMatch::SameRegionAsProgram)},
                },
        },
        ProgramDeterminationTestParam{
            .test_name = "in_scope_and_variations_country_not_in_region",
            .client_fetched_country = CountryId("FR"),
            .variations_latest_country_id = CountryId("US"),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kWaffle,
#endif
            .expected_program = Program::kWaffle,
            .expected_histograms =
                {
                    {"RegionalCapabilities.FunnelStage.RegionalPresence",
                     ExpectHistogramBucket(ProgramAndLocationMatch::NoMatch)},
                },
        },
        ProgramDeterminationTestParam{
            .test_name = "out_of_scope",
            .client_fetched_country = CountryId("US"),
            .variations_latest_country_id = CountryId("US"),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kDefault,
#endif
            .expected_program = Program::kDefault,
            .expected_histograms =
                {
                    {"RegionalCapabilities.FunnelStage.RegionalPresence",
                     ExpectHistogramNever()},
                },
        },
        ProgramDeterminationTestParam{
            .test_name = "in_scope_no_variations_country",
            .client_fetched_country = CountryId("FR"),
            .variations_latest_country_id = CountryId(),
#if BUILDFLAG(IS_ANDROID)
            .device_program_override = Program::kWaffle,
#endif
            .expected_program = Program::kWaffle,
            .expected_histograms =
                {
                    {"RegionalCapabilities.FunnelStage.RegionalPresence",
                     ExpectHistogramNever()},
                },
        },
    }),
    &RegionalCapabilitiesServiceProgramDeterminationTest::GetTestName);

TEST_F(RegionalCapabilitiesServiceTest,
       GetCountryAndProgramCommandLineOverride) {
  // The command line value bypasses the country ID cache and does not
  // require recreating the service.
  std::unique_ptr<RegionalCapabilitiesService> service = InitService();

  SetCommandLineCountry(kBelgiumCountryCode);
  EXPECT_EQ(GetCountryId(*service), kBelgiumCountryId);
  EXPECT_EQ(GetActiveProgram(*service), Program::kWaffle);

  // When the command line value is not two uppercase basic Latin alphabet
  // characters, the country code should not be valid.
  SetCommandLineCountry("??");
  EXPECT_FALSE(GetCountryId(*service).IsValid());
  EXPECT_EQ(GetActiveProgram(*service), Program::kDefault);

  SetCommandLineCountry("us");
  EXPECT_FALSE(GetCountryId(*service).IsValid());
  EXPECT_EQ(GetActiveProgram(*service), Program::kDefault);

  SetCommandLineCountry("USA");
  EXPECT_FALSE(GetCountryId(*service).IsValid());
  EXPECT_EQ(GetActiveProgram(*service), Program::kDefault);

  SetCommandLineCountry(switches::kEeaListCountryOverride);
  EXPECT_FALSE(GetCountryId(*service).IsValid());
  EXPECT_EQ(GetActiveProgram(*service), Program::kWaffle);

  SetCommandLineCountry(switches::kDefaultListCountryOverride);
  EXPECT_FALSE(GetCountryId(*service).IsValid());
  EXPECT_EQ(GetActiveProgram(*service), Program::kWaffle);

  SetCommandLineCountry(switches::kTaiyakiProgramOverride);
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  if (kPhoneFormFactors.Has(ui::GetDeviceFormFactor())) {
    EXPECT_EQ(GetCountryId(*service), CountryId("JP"));
    EXPECT_EQ(GetActiveProgram(*service), Program::kTaiyaki);
  } else
#endif
  {
    EXPECT_FALSE(GetCountryId(*service).IsValid());
    EXPECT_EQ(GetActiveProgram(*service), Program::kDefault);
  }

  CheckHistogramExpectation(histogram_tester(),
                            "RegionalCapabilities.FetchedCountryMatching",
                            ExpectHistogramNever());
  CheckHistogramExpectation(histogram_tester(),
                            "RegionalCapabilities.FallbackCountryMatching",
                            ExpectHistogramNever());
  CheckHistogramExpectation(histogram_tester(),
                            "RegionalCapabilities.PersistedCountryMatching",
                            ExpectHistogramNever());
  CheckHistogramExpectation(histogram_tester(),
                            "RegionalCapabilities.LoadedCountrySource",
                            ExpectHistogramNever());
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
      InitService(kBelgiumCountryId
#if BUILDFLAG(IS_ANDROID)
                  ,
                  Program::kWaffle
#endif  // BUILDFLAG(IS_ANDROID)
      );
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

TEST_F(RegionalCapabilitiesServiceTest, IsInSearchEngineChoiceScreenRegion) {
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("DE")));
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("FR")));
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("VA")));
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("AX")));
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("YT")));
  EXPECT_TRUE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("NC")));

#if BUILDFLAG(IS_IOS)
  {
    base::test::ScopedFeatureList scoped_feature_list{switches::kTaiyaki};
    EXPECT_EQ(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
                  CountryId("JP")),
              kPhoneFormFactors.Has(ui::GetDeviceFormFactor()));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(switches::kTaiyaki);
    EXPECT_FALSE(
        RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
            CountryId("JP")));
  }
#else
  EXPECT_FALSE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("JP")));
#endif  // BUILDFLAG(IS_IOS)

  EXPECT_FALSE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId("US")));
  EXPECT_FALSE(RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
      CountryId()));
}

}  // namespace
}  // namespace regional_capabilities
