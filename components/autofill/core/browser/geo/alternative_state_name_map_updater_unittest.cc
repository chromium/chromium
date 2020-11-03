// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

class AlternativeStateNameMapUpdaterTest : public ::testing::Test {
 public:
  AlternativeStateNameMapUpdaterTest()
      : pref_service_(test::PrefServiceForTesting()) {}

  void SetUp() override {
    ASSERT_TRUE(data_install_dir_.CreateUniqueTempDir());
  }

  const base::FilePath& GetPath() const { return data_install_dir_.GetPath(); }

  void WritePathToPref(const base::FilePath& file_path) {
    pref_service_->SetFilePath(autofill::prefs::kAutofillStatesDataDir,
                               file_path);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  AlternativeStateNameMapUpdater alternative_state_name_map_updater;
  std::unique_ptr<PrefService> pref_service_;
  base::ScopedTempDir data_install_dir_;
};

// Tests that the states data is added to AlternativeStateNameMap.
TEST_F(AlternativeStateNameMapUpdaterTest, EntryAddedToStateMap) {
  test::ClearAlternativeStateNameMapForTesting();
  std::string states_data = test::CreateStatesProtoAsString();
  std::vector<AlternativeStateNameMap::StateName> test_strings = {
      AlternativeStateNameMap::StateName(ASCIIToUTF16("Bavaria")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("Bayern")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("B.Y")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("Bav-aria")),
      AlternativeStateNameMap::StateName(UTF8ToUTF16("amapá")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("Broen")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("Bavaria is in Germany")),
      AlternativeStateNameMap::StateName(ASCIIToUTF16("BA is in Germany"))};
  std::vector<bool> state_data_present = {true,  true,  true,  true,
                                          false, false, false, false};

  alternative_state_name_map_updater.ProcessLoadedStateFileContentForTesting(
      test_strings, states_data, base::DoNothing());
  AlternativeStateNameMap* alternative_state_name_map =
      AlternativeStateNameMap::GetInstance();
  DCHECK(!alternative_state_name_map->IsLocalisedStateNamesMapEmpty());

  for (size_t i = 0; i < test_strings.size(); i++) {
    SCOPED_TRACE(test_strings[i]);
    EXPECT_EQ(alternative_state_name_map->GetCanonicalStateName(
                  AlternativeStateNameMap::CountryCode("DE"),
                  test_strings[i]) != base::nullopt,
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

  base::RunLoop run_loop;
  alternative_state_name_map_updater.LoadStatesData(
      {{AlternativeStateNameMap::CountryCode("DE"),
        {AlternativeStateNameMap::StateName(ASCIIToUTF16("Bavaria"))}}},
      pref_service_.get(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_NE(
      AlternativeStateNameMap::GetInstance()->GetCanonicalStateName(
          AlternativeStateNameMap::CountryCode("DE"),
          AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bavaria"))),
      base::nullopt);
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

  base::RunLoop run_loop;
  alternative_state_name_map_updater.LoadStatesData(
      {{AlternativeStateNameMap::CountryCode("ES"),
        {AlternativeStateNameMap::StateName(ASCIIToUTF16("Parana"))}}},
      pref_service_.get(), run_loop.QuitClosure());
  run_loop.Run();

  base::Optional<StateEntry> entry1 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(base::UTF8ToUTF16("Paraná")));
  EXPECT_NE(entry1, base::nullopt);
  EXPECT_EQ(entry1->canonical_name(), "Paraná");
  EXPECT_THAT(entry1->abbreviations(),
              testing::UnorderedElementsAreArray({"PR"}));
  EXPECT_THAT(entry1->alternative_names(), testing::UnorderedElementsAreArray(
                                               {"Parana", "State of Parana"}));

  base::Optional<StateEntry> entry2 =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode("ES"),
          AlternativeStateNameMap::StateName(base::UTF8ToUTF16("Parana")));
  EXPECT_NE(entry2, base::nullopt);
  EXPECT_EQ(entry2->canonical_name(), "Paraná");
  EXPECT_THAT(entry2->abbreviations(),
              testing::UnorderedElementsAreArray({"PR"}));
  EXPECT_THAT(entry2->alternative_names(), testing::UnorderedElementsAreArray(
                                               {"Parana", "State of Parana"}));
}

// Tests the |StateNameMapUpdater::ContainsState()| functionality.
TEST_F(AlternativeStateNameMapUpdaterTest, ContainsState) {
  EXPECT_TRUE(AlternativeStateNameMapUpdater::ContainsStateForTesting(
      {AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bavaria")),
       AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bayern")),
       AlternativeStateNameMap::StateName(base::ASCIIToUTF16("BY"))},
      AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bavaria"))));
  EXPECT_FALSE(AlternativeStateNameMapUpdater::ContainsStateForTesting(
      {AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bavaria")),
       AlternativeStateNameMap::StateName(base::ASCIIToUTF16("Bayern")),
       AlternativeStateNameMap::StateName(base::ASCIIToUTF16("BY"))},
      AlternativeStateNameMap::StateName(base::ASCIIToUTF16("California"))));
}

}  // namespace autofill
