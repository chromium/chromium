// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/geo/mock_alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
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
    ASSERT_TRUE(data_install_dir_.CreateUniqueTempDir());
    alternative_state_name_map_updater_ =
        std::make_unique<AlternativeStateNameMapUpdater>(
            autofill_client_.GetPrefs(), &address_data_manager_);
  }

  const base::FilePath& GetPath() const { return data_install_dir_.GetPath(); }

  void WritePathToPref(const base::FilePath& file_path) {
    autofill_client_.GetPrefs()->SetFilePath(
        autofill::prefs::kAutofillStatesDataDir, file_path);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  TestAddressDataManager address_data_manager_;
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;
  base::ScopedTempDir data_install_dir_;
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

// Tests that the AlternativeStateNameMap is populated when
// |StateNameMapUpdater::LoadStatesData()| is called.
TEST_F(AlternativeStateNameMapUpdaterTest, TestLoadStatesData) {
  test::ClearAlternativeStateNameMapForTesting();

  base::WriteFile(GetPath().AppendASCII("DE"),
                  test::CreateStatesProtoAsString());
  WritePathToPref(GetPath());
  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("DE"),
       {AlternativeStateNameMap::StateName(u"Bavaria")}}};
  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_NE(AlternativeStateNameMap::GetCanonicalStateName("DE", u"Bavaria"),
            std::nullopt);
}

// Tests that there is no insertion in the AlternativeStateNameMap when a
// garbage country code is supplied to the LoadStatesData for which the states
// data file does not exist.
TEST_F(AlternativeStateNameMapUpdaterTest, NoTaskIsPosted) {
  test::ClearAlternativeStateNameMapForTesting();

  base::WriteFile(GetPath().AppendASCII("DE"),
                  test::CreateStatesProtoAsString());
  WritePathToPref(GetPath());

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

  base::WriteFile(
      GetPath().AppendASCII("ES"),
      test::CreateStatesProtoAsString(
          "ES", {.canonical_name = "Paraná",
                 .abbreviations = {"PR"},
                 .alternative_names = {"Parana", "State of Parana"}}));
  WritePathToPref(GetPath());

  CountryToStateNamesListMapping country_to_state_names_list_mapping = {
      {AlternativeStateNameMap::CountryCode("ES"),
       {AlternativeStateNameMap::StateName(u"Parana")}}};

  base::RunLoop run_loop;
  alternative_state_name_map_updater_->LoadStatesDataForTesting(
      country_to_state_names_list_mapping, autofill_client_.GetPrefs(),
      run_loop.QuitClosure());
  run_loop.Run();

  std::optional<StateEntry> entry1 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(u"Paraná"));
  EXPECT_NE(entry1, std::nullopt);
  EXPECT_EQ(entry1->canonical_name(), "Paraná");
  EXPECT_THAT(entry1->abbreviations(),
              testing::UnorderedElementsAreArray({"PR"}));
  EXPECT_THAT(entry1->alternative_names(), testing::UnorderedElementsAreArray(
                                               {"Parana", "State of Parana"}));

  std::optional<StateEntry> entry2 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(u"Parana"));
  EXPECT_NE(entry2, std::nullopt);
  EXPECT_EQ(entry2->canonical_name(), "Paraná");
  EXPECT_THAT(entry2->abbreviations(),
              testing::UnorderedElementsAreArray({"PR"}));
  EXPECT_THAT(entry2->alternative_names(), testing::UnorderedElementsAreArray(
                                               {"Parana", "State of Parana"}));
}

// Tests that the AlternativeStateNameMap is populated when
// |StateNameMapUpdater::LoadStatesData()| is called for states data of
// multiple countries simultaneously.
TEST_F(AlternativeStateNameMapUpdaterTest,
       TestLoadStatesDataOfMultipleCountriesSimultaneously) {
  test::ClearAlternativeStateNameMapForTesting();

  base::WriteFile(GetPath().AppendASCII("DE"),
                  test::CreateStatesProtoAsString());
  base::WriteFile(
      GetPath().AppendASCII("ES"),
      test::CreateStatesProtoAsString(
          "ES", {.canonical_name = "Paraná",
                 .abbreviations = {"PR"},
                 .alternative_names = {"Parana", "State of Parana"}}));
  WritePathToPref(GetPath());

  CountryToStateNamesListMapping country_to_state_names = {
      {AlternativeStateNameMap::CountryCode("ES"),
       {AlternativeStateNameMap::StateName(u"Parana")}},
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
          AlternativeStateNameMap::StateName(u"Paraná"));
  EXPECT_NE(entry1, std::nullopt);
  EXPECT_EQ(entry1->canonical_name(), "Paraná");
  EXPECT_THAT(entry1->abbreviations(),
              testing::UnorderedElementsAreArray({"PR"}));
  EXPECT_THAT(entry1->alternative_names(), testing::UnorderedElementsAreArray(
                                               {"Parana", "State of Parana"}));

  std::optional<StateEntry> entry2 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("DE"),
          AlternativeStateNameMap::StateName(u"Bavaria"));
  EXPECT_NE(entry2, std::nullopt);
  EXPECT_EQ(entry2->canonical_name(), "Bavaria");
  EXPECT_THAT(entry2->abbreviations(),
              testing::UnorderedElementsAreArray({"BY"}));
  EXPECT_THAT(entry2->alternative_names(),
              testing::UnorderedElementsAreArray({"Bayern"}));
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
  WritePathToPref(GetPath());
  base::WriteFile(GetPath().AppendASCII("DE"),
                  test::CreateStatesProtoAsString());

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

}  // namespace autofill
