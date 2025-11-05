// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"

#include <optional>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_constants.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/grit/autofill_alternative_state_name_map_resources.h"
#include "components/autofill/core/browser/geo/grit/test_autofill_address_rewriter_resources_map.h"
#include "components/autofill/core/browser/geo/mock_alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/proto/states.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace i18n::addressinput {

using ::operator<<;

}  // namespace i18n::addressinput

namespace autofill {

class AlternativeStateNameMapUpdaterTest : public ::testing::Test {
 public:
  AlternativeStateNameMapUpdaterTest() = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    alternative_state_name_map_updater_ =
        std::make_unique<AlternativeStateNameMapUpdater>(
            autofill_client_.GetPrefs(), &address_data_manager_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  TestAddressDataManager address_data_manager_;
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;
};

// Tests that the states data is added to AlternativeStateNameMap.
TEST_F(AlternativeStateNameMapUpdaterTest, EntryAddedToStateMap) {
  test::ClearAlternativeStateNameMapForTesting();
  std::string states_data = test::CreateStatesProtoAsString();
  std::vector<AlternativeStateNameMap::StateName> test_strings = {
      AlternativeStateNameMap::StateName(u"Bavaria"),
      AlternativeStateNameMap::StateName(u"Bayern"),
      AlternativeStateNameMap::StateName(u"B.Y"),
      AlternativeStateNameMap::StateName(u"Bav-aria"),
      AlternativeStateNameMap::StateName(u"amapá"),
      AlternativeStateNameMap::StateName(u"Broen"),
      AlternativeStateNameMap::StateName(u"Bavaria is in Germany"),
      AlternativeStateNameMap::StateName(u"BA is in Germany")};
  std::vector<bool> state_data_present = {true,  true,  true,  true,
                                          false, false, false, false};

  alternative_state_name_map_updater_->ProcessLoadedStateFileContentForTesting(
      test_strings, states_data, base::DoNothing());
  AlternativeStateNameMap* alternative_state_name_map =
      AlternativeStateNameMap::GetInstance();
  DCHECK(!alternative_state_name_map->IsLocalisedStateNamesMapEmpty());
  for (size_t i = 0; i < test_strings.size(); i++) {
    SCOPED_TRACE(test_strings[i]);
    EXPECT_EQ(AlternativeStateNameMap::GetCanonicalStateName(
                  "DE", test_strings[i].value()) != std::nullopt,
              state_data_present[i]);
  }
}

// Tests that the AlternativeStateNameMap is populated when profile with
// supported country is added.
TEST_F(AlternativeStateNameMapUpdaterTest, TestLoadStatesData) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("DE"),
       {AlternativeStateNameMap::StateName(u"Bavaria")}}};
  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName("DE", u"Bayern"),
            std::nullopt);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests that Chrome states data is loaded in case of branded build.
TEST_F(AlternativeStateNameMapUpdaterTest, TestLoadChromeStatesData) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("PL"),
       {AlternativeStateNameMap::StateName(u"kujawskopomorskie")}}};
  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName(
                "PL", u"kujawskopomorskie"),
            std::nullopt);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that there is no insertion in the AlternativeStateNameMap when a
// garbage country code is supplied to the LoadStatesData for which the states
// data file does not exist.
TEST_F(AlternativeStateNameMapUpdaterTest, NoTaskIsPosted) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("DEE"),
       {AlternativeStateNameMap::StateName(u"Bavaria")}}};
  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
}

// Tests that the AlternativeStateNameMap is populated when
// |StateNameMapUpdater::LoadStatesData()| is called and there are UTF8 strings.
TEST_F(AlternativeStateNameMapUpdaterTest, TestLoadStatesDataUTF8) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("ES"),
       {AlternativeStateNameMap::StateName(u"Andalucia"),
        AlternativeStateNameMap::StateName(u"Euskadi")}}};

  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  std::optional<StateEntry> entry1 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(u"Andalucía"));
  ASSERT_NE(entry1, std::nullopt);
  EXPECT_EQ(entry1->canonical_name(), "andalucía");
  EXPECT_THAT(entry1->abbreviations(),
              testing::UnorderedElementsAreArray({"an"}));
  EXPECT_THAT(entry1->alternative_names(),
              testing::UnorderedElementsAreArray({"andalusia"}));

  std::optional<StateEntry> entry2 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(u"Euskadi"));
  ASSERT_NE(entry2, std::nullopt);
  EXPECT_EQ(entry2->canonical_name(), "euskadi");
  EXPECT_THAT(entry2->abbreviations(),
              testing::UnorderedElementsAreArray({"pv"}));
  EXPECT_THAT(
      entry2->alternative_names(),
      testing::UnorderedElementsAreArray({"basque country", "euskal herria"}));
}

// Tests that the AlternativeStateNameMap is populated when
// |StateNameMapUpdater::LoadStatesData()| is called for states data of
// multiple countries simultaneously.
TEST_F(AlternativeStateNameMapUpdaterTest,
       TestLoadStatesDataOfMultipleCountriesSimultaneously) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names = {
      {AlternativeStateNameMap::CountryCode("ES"),
       {AlternativeStateNameMap::StateName(u"Andalucia")}},
      {AlternativeStateNameMap::CountryCode("DE"),
       {AlternativeStateNameMap::StateName(u"Bavaria")}}};

  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  std::optional<StateEntry> entry1 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(u"Andalucía"));
  ASSERT_NE(entry1, std::nullopt);
  EXPECT_EQ(entry1->canonical_name(), "andalucía");
  EXPECT_THAT(entry1->abbreviations(),
              testing::UnorderedElementsAreArray({"an"}));
  EXPECT_THAT(entry1->alternative_names(),
              testing::UnorderedElementsAreArray({"andalusia"}));

  std::optional<StateEntry> entry2 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("DE"),
          AlternativeStateNameMap::StateName(u"Bavaria"));
  ASSERT_NE(entry2, std::nullopt);
  EXPECT_EQ(entry2->canonical_name(), "bayern");
  EXPECT_THAT(entry2->abbreviations(),
              testing::UnorderedElementsAreArray({"by"}));
  EXPECT_THAT(entry2->alternative_names(),
              testing::UnorderedElementsAreArray({"bavaria"}));
}

// Tests the |StateNameMapUpdater::ContainsState()| functionality.
TEST_F(AlternativeStateNameMapUpdaterTest, ContainsState) {
  EXPECT_TRUE(AlternativeStateNameMapUpdater::ContainsStateForTesting(
      {AlternativeStateNameMap::StateName(u"Bavaria"),
       AlternativeStateNameMap::StateName(u"Bayern"),
       AlternativeStateNameMap::StateName(u"BY")},
      AlternativeStateNameMap::StateName(u"Bavaria")));
  EXPECT_FALSE(AlternativeStateNameMapUpdater::ContainsStateForTesting(
      {AlternativeStateNameMap::StateName(u"Bavaria"),
       AlternativeStateNameMap::StateName(u"Bayern"),
       AlternativeStateNameMap::StateName(u"BY")},
      AlternativeStateNameMap::StateName(u"California")));
}

// Tests that the |AlternativeStateNameMap| is populated with the help of the
// |MockAlternativeStateNameMapUpdater| observer when a new profile is added to
// the PDM.
TEST_F(AlternativeStateNameMapUpdaterTest,
       PopulateAlternativeStateNameUsingObserver) {
  test::ClearAlternativeStateNameMapForTesting();

  AutofillProfile profile(AddressCountryCode("DE"));
  profile.SetInfo(ADDRESS_HOME_STATE, u"Bavaria", "en-US");

  base::RunLoop run_loop;
  MockAlternativeStateNameMapUpdater mock_alternative_state_name_updater(
      run_loop.QuitClosure(), autofill_client_.GetPrefs(),
      &address_data_manager_);
  address_data_manager_.AddProfile(profile);
  run_loop.Run();

  EXPECT_FALSE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName("DE", u"Bavaria"),
            AlternativeStateNameMap::CanonicalStateName(u"Bayern"));
}

// Tests that loading all a country files does not crash the browser.
TEST_F(AlternativeStateNameMapUpdaterTest, LoadAllCountryFiles) {
  test::ClearAlternativeStateNameMapForTesting();

  CountryToStateNamesListMapping country_to_state_names;
  for (const auto& [country_code, _] : kCountryAddressImportRequirementsData) {
    country_to_state_names[AlternativeStateNameMap::CountryCode(std::string(
        country_code))] = {AlternativeStateNameMap::StateName(u"Test State")};
  }

  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      std::move(country_to_state_names), autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  // The test's purpose is to ensure that loading and processing all files does
  // not crash. No specific expectation on the map's content is made here.
}

// Tests that loading a corrupted country file does not crash the browser.
TEST_F(AlternativeStateNameMapUpdaterTest, LoadCorruptedCountryFile) {
  test::ClearAlternativeStateNameMapForTesting();

  std::string corrupted_data = "This is not a valid proto message.";

  // A dummy state name for testing.
  std::vector<AlternativeStateNameMap::StateName> test_states = {
      AlternativeStateNameMap::StateName(u"Test State")};

  base::RunLoop run_loop;
  alternative_state_name_map_updater_->ProcessLoadedStateFileContentForTesting(
      test_states, corrupted_data, run_loop.QuitClosure());
  run_loop.Run();

  // The test's purpose is to ensure that processing corrupted data does not
  // crash. The map should be empty as the data is invalid.
  EXPECT_TRUE(
      AlternativeStateNameMap::GetInstance()->IsLocalisedStateNamesMapEmpty());
}

TEST_F(AlternativeStateNameMapUpdaterTest, AssertCorrectStateNameMapConstant) {
  // Assert the length of the `kCountriesWithAlternativeStateNames` is correct.
  EXPECT_EQ(
      IDR_STATE_NAME_MAP_BEGIN +
          static_cast<int>(kCountriesWithAlternativeStateNames.length() / 2) +
          1,
      IDR_STATE_NAME_MAP_END);

  // Assert the logic computing resource IDs relying on manually curated
  // `kCountriesWithAlternativeStateNames` yields the same IDs as generated maps
  // not included in release build.
  auto GetResourceIdForCountryFromGeneratedMap =
      [](const std::string& country) {
        std::string resource_key =
            base::StrCat({"IDR_STATE_NAME_MAP_", country, "_ALTERNATIVES"});
        for (const webui::ResourcePath& resource :
             kAutofillAlternativeStateNameMapResources) {
          if (resource.path == resource_key) {
            return resource.id;
          }
        }
        return -1;
      };

  for (size_t i = 0; i < kCountriesWithAlternativeStateNames.length(); i += 2) {
    std::string country =
        std::string(kCountriesWithAlternativeStateNames.substr(i, 2));
    int32_t resourceId = GetResourceIdForCountryFromGeneratedMap(country);
    ASSERT_NE(resourceId, -1);
    EXPECT_EQ(resourceId, FindResourceIdForCountry(country));
  }
}

}  // namespace autofill
