// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using base::UTF8ToUTF16;
using testing::IsEmpty;
using ObservationType = ProfileTokenQuality::ObservationType;

namespace {

std::u16string GetSuggestionLabel(AutofillProfile* profile) {
  return AutofillProfile::CreateDifferentiatingLabels(
      base::span_from_ref(profile), "en-US")[0];
}

void SetupTestProfile(AutofillProfile& profile) {
  profile.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
}

std::vector<const AutofillProfile*> ToRawPointerVector(
    const std::vector<std::unique_ptr<AutofillProfile>>& list) {
  std::vector<const AutofillProfile*> result;
  for (const auto& item : list) {
    result.push_back(item.get());
  }
  return result;
}

class AutofillProfileTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
    base::Time year2020;
    ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
    task_environment_.FastForwardBy(year2020 - base::Time::Now());
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST_F(AutofillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutofillProfile profile0(i18n_model_definition::kLegacyHierarchyCountryCode);
  // Empty profile - nothing to update.
  std::u16string summary0 = GetSuggestionLabel(&profile0);
  EXPECT_EQ(std::u16string(), summary0);

  // Case 0a/empty name and address, so the first two fields of the rest of the
  // data is used: "Hollywood, CA"
  AutofillProfile profile00(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile00, "", "", "", "johnwayne@me.xyz", "Fox", "",
                       "", "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary00 = GetSuggestionLabel(&profile00);
  EXPECT_EQ(u"Hollywood, CA", summary00);

  // Case 1: "<address>" without line 2.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "", "", "", "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.", "", "Hollywood", "CA", "91601", "US",
                       "16505678910");
  std::u16string summary1 = GetSuggestionLabel(&profile1);
  EXPECT_EQ(u"123 Zoo St., Hollywood", summary1);

  // Case 1a: "<address>" with line 2.
  AutofillProfile profile1a(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1a, "", "", "", "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601",
                       "US", "16505678910");
  std::u16string summary1a = GetSuggestionLabel(&profile1a);
  EXPECT_EQ(u"123 Zoo St., unit 5", summary1a);

  // Case 2: "<lastname>"
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary2 = GetSuggestionLabel(&profile2);
  // Summary includes full name, to the maximal extent available.
  EXPECT_EQ(u"Mitchell Morrison, Hollywood", summary2);

  // Case 3: "<lastname>, <address>"
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary3 = GetSuggestionLabel(&profile3);
  EXPECT_EQ(u"Mitchell Morrison, 123 Zoo St.", summary3);

  // Case 4: "<firstname>"
  AutofillProfile profile4(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile4, "Marion", "Mitchell", "", "johnwayne@me.xyz",
                       "Fox", "", "", "Hollywood", "CA", "91601", "US",
                       "16505678910");
  std::u16string summary4 = GetSuggestionLabel(&profile4);
  EXPECT_EQ(u"Marion Mitchell, Hollywood", summary4);

  // Case 5: "<firstname>, <address>"
  AutofillProfile profile5(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile5, "Marion", "Mitchell", "", "johnwayne@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary5 = GetSuggestionLabel(&profile5);
  EXPECT_EQ(u"Marion Mitchell, 123 Zoo St.", summary5);

  // Case 6: "<firstname> <lastname>"
  AutofillProfile profile6(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile6, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary6 = GetSuggestionLabel(&profile6);
  EXPECT_EQ(u"Marion Mitchell Morrison, Hollywood", summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutofillProfile profile7(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile7, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary7 = GetSuggestionLabel(&profile7);
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St.", summary7);

  // Case 7a: "<firstname> <lastname>, <address>" - same as #7, except for
  // e-mail.
  AutofillProfile profile7a(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile7a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::vector<const AutofillProfile*> profiles;
  profiles.push_back(&profile7);
  profiles.push_back(&profile7a);
  std::vector<std::u16string> labels =
      AutofillProfile::CreateDifferentiatingLabels(profiles, "en-US");
  ASSERT_EQ(profiles.size(), labels.size());
  summary7 = labels[0];
  std::u16string summary7a = labels[1];
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St., johnwayne@me.xyz",
            summary7);
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St., marion@me.xyz", summary7a);
}

TEST_F(AutofillProfileTest, AdjustInferredLabels) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CA", "91111", "US", "16502111111");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "Jane", "", "Doe",
                       "janedoe@tertium.com", "Pluto Inc.", "123 Letha Shore.",
                       "", "Dis", "CA", "91222", "US", "12345678910");
  std::vector<std::u16string> labels =
      AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                                   "en-US");
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St.", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);

  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "johndoe@tertium.com", "Underworld", "666 Erebus St.",
                       "", "Elysium", "CA", "91111", "US", "16502111111");
  labels = AutofillProfile::CreateDifferentiatingLabels(
      ToRawPointerVector(profiles), "en-US");

  // Profile 0 and 2 inferred label now includes an e-mail.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St., johndoe@hades.com", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., johndoe@tertium.com", labels[2]);

  profiles.resize(2);

  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CO",  // State is different
                       "91111", "US", "16502111111");

  labels = AutofillProfile::CreateDifferentiatingLabels(
      ToRawPointerVector(profiles), "en-US");

  // Profile 0 and 2 inferred label now includes a state.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St., CA", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO", labels[2]);

  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[3].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CO",  // State is different for some.
                       "91111", "US",
                       "16504444444");  // Phone is different for some.

  labels = AutofillProfile::CreateDifferentiatingLabels(
      ToRawPointerVector(profiles), "en-US");
  ASSERT_EQ(4U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St., CA", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, 16502111111", labels[2]);
  // This one differs from other ones by unique phone, so no need for extra
  // information.
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, 16504444444", labels[3]);

  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[4].get(), "John", "", "Doe",
                       "johndoe@styx.com",  // E-Mail is different for some.
                       "Underworld", "666 Erebus St.", "", "Elysium",
                       "CO",  // State is different for some.
                       "91111", "US",
                       "16504444444");  // Phone is different for some.

  labels = AutofillProfile::CreateDifferentiatingLabels(
      ToRawPointerVector(profiles), "en-US");
  ASSERT_EQ(5U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St., CA", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, johndoe@hades.com, 16502111111",
            labels[2]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, johndoe@hades.com, 16504444444",
            labels[3]);
  // This one differs from other ones by unique e-mail, so no need for extra
  // information.
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, johndoe@styx.com", labels[4]);
}

TEST_F(AutofillProfileTest, CreateInferredLabelsI18n_CH) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles.back().get(), "H.", "R.", "Giger",
                       "hrgiger@beispiel.com", "Beispiel Inc",
                       "Brandschenkestrasse 110", "", "Zurich", "", "8002",
                       "CH", "+41 44-668-1800");
  profiles.back()->set_language_code("de_CH");
  static constexpr auto kExpectedLabels = std::to_array<std::u16string_view>(
      {u"", u"H. R. Giger", u"H. R. Giger, Brandschenkestrasse 110",
       u"H. R. Giger, Brandschenkestrasse 110, Zurich",
       u"H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich",
       u"Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich",
       u"Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
       u"Switzerland",
       u"Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
       u"Switzerland, hrgiger@beispiel.com",
       u"Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
       u"Switzerland, hrgiger@beispiel.com, +41 44-668-1800"});

  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
        ToRawPointerVector(profiles),
        /*suggested_fields=*/std::nullopt,
        /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
        /*minimal_fields_shown=*/i, "en-US");
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(kExpectedLabels[i], labels.back());
  }
}

TEST_F(AutofillProfileTest, CreateInferredLabelsI18n_FR) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles.back().get(), "Antoine", "", "de Saint-Exupéry",
                       "antoine@exemple.com", "Exemple Inc", "8 Rue de Londres",
                       "", "Paris", "", "75009", "FR", "+33 (0) 1 42 68 53 00");
  profiles.back()->set_language_code("fr_FR");
  static constexpr auto kExpectedLabels = std::to_array<std::u16string_view>(
      {u"", u"Antoine de Saint-Exupéry",
       u"Antoine de Saint-Exupéry, 8 Rue de Londres",
       u"Antoine de Saint-Exupéry, 8 Rue de Londres, Paris",
       u"Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris",
       u"Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris",
       u"Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
       u"France",
       u"Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
       u"France, antoine@exemple.com",
       u"Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
       u"France, antoine@exemple.com, +33 (0) 1 42 68 53 00",
       u"Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
       u"France, antoine@exemple.com, +33 (0) 1 42 68 53 00"});

  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
        ToRawPointerVector(profiles),
        /*suggested_fields=*/std::nullopt,
        /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
        /*minimal_fields_shown=*/i, "en-US");
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(kExpectedLabels[i], labels.back());
  }
}

TEST_F(AutofillProfileTest, CreateInferredLabelsI18n_KR) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles.back().get(), "Park", "", "Jae-sang",
                       "park@yeleul.com", "Yeleul Inc",
                       "Gangnam Finance Center", "152 Teheran-ro", "Gangnam-Gu",
                       "Seoul", "135-984", "KR", "+82-2-531-9000");
  profiles.back()->set_language_code("ko_Latn");
  profiles.back()->SetInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Yeoksam-Dong",
                           "en-US");
  static constexpr auto kExpectedLabels = std::to_array<std::u16string_view>(
      {u"", u"Park Jae-sang", u"Park Jae-sang, Gangnam Finance Center",
       u"Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro",
       u"Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong",
       u"Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
       u"Gangnam-Gu",
       u"Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
       u"Gangnam-Gu, Seoul",
       u"Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
       u"Gangnam-Gu, Seoul, 135-984",
       u"Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
       u"Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984",
       u"Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
       u"Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea",
       u"Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
       u"Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea, "
       u"park@yeleul.com",
       u"Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
       u"Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea, "
       u"park@yeleul.com, +82-2-531-9000"});

  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
        ToRawPointerVector(profiles),
        /*suggested_fields=*/std::nullopt,
        /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
        /*minimal_fields_shown=*/i, "en-US");
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(kExpectedLabels[i], labels.back());
  }
}

TEST_F(AutofillProfileTest, CreateInferredLabelsI18n_JP_Latn) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles.back().get(), "Miku", "", "Hatsune",
                       "miku@rei.com", "Rei Inc", "Roppongi Hills Mori Tower",
                       "6-10-1 Roppongi, Minato-ku", "", "Tokyo", "106-6126",
                       "JP", "+81-3-6384-9000");
  profiles.back()->set_language_code("ja_Latn");
  static constexpr auto kExpectedLabels = std::to_array<std::u16string_view>(
      {u"", u"Miku Hatsune", u"Miku Hatsune, Roppongi Hills Mori Tower",
       u"Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku",
       u"Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku, "
       u"Tokyo",
       u"Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku, "
       u"Tokyo, 106-6126",
       u"Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
       u"Minato-ku, Tokyo, 106-6126",
       u"Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
       u"Minato-ku, Tokyo, 106-6126, Japan",
       u"Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
       u"Minato-ku, Tokyo, 106-6126, Japan, miku@rei.com",
       u"Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
       u"Minato-ku, Tokyo, 106-6126, Japan, miku@rei.com, +81-3-6384-9000"});

  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
        ToRawPointerVector(profiles),
        /*suggested_fields=*/std::nullopt,
        /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
        /*minimal_fields_shown=*/i, "en-US");
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(kExpectedLabels[i], labels.back());
  }
}

TEST_F(AutofillProfileTest, CreateInferredLabelsI18n_JP_ja) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles.back().get(), "ミク", "", "初音",
                       "miku@rei.com", "例", "港区六本木ヒルズ森タワー",
                       "六本木 6-10-1", "", "東京都", "106-6126", "JP",
                       "03-6384-9000");
  profiles.back()->set_language_code("ja_JP");
  static constexpr auto kExpectedLabels = std::to_array<std::u16string_view>(
      {u"", u"初音ミク", u"港区六本木ヒルズ森タワー初音ミク",
       u"港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
       u"東京都港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
       u"〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
       u"〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク",
       u"〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, "
       u"Japan",
       u"〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, "
       u"Japan, "
       u"miku@rei.com",
       u"〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, "
       u"Japan, "
       u"miku@rei.com, 03-6384-9000"});

  for (size_t i = 0; i < kExpectedLabels.size(); ++i) {
    std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
        ToRawPointerVector(profiles),
        /*suggested_fields=*/std::nullopt,
        /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
        /*minimal_fields_shown=*/i, "en-US");
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(kExpectedLabels[i], labels.back());
  }
}

TEST_F(AutofillProfileTest, CreateInferredLabels) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CA", "91111", "US", "16502111111");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "Jane", "", "Doe",
                       "janedoe@tertium.com", "Pluto Inc.", "123 Letha Shore.",
                       "", "Dis", "CA", "91222", "US", "12345678910");
  // Two fields at least - no filter.
  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles),
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/2, "en-US");
  EXPECT_EQ(u"John Doe, 666 Erebus St.", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);

  // Three fields at least - no filter.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles),
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/3, "en-US");
  EXPECT_EQ(u"John Doe, 666 Erebus St., Elysium", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore., Dis", labels[1]);

  FieldTypeSet suggested_fields = {ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                   ADDRESS_HOME_ZIP};

  // Two fields at least, from suggested fields - no filter.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/2, "en-US");
  EXPECT_EQ(u"Elysium 91111", labels[0]);
  EXPECT_EQ(u"Dis 91222", labels[1]);

  // Three fields at least, from suggested fields - no filter.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, /*excluded_fields=*/{},
      /*minimal_fields_shown=*/3, "en-US");
  EXPECT_EQ(u"Elysium, CA 91111", labels[0]);
  EXPECT_EQ(u"Dis, CA 91222", labels[1]);

  // Three fields at least, from suggested fields - but filter reduces available
  // fields to two.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {ADDRESS_HOME_ZIP},
      /*minimal_fields_shown=*/3, "en-US");
  EXPECT_EQ(u"Elysium, CA", labels[0]);
  EXPECT_EQ(u"Dis, CA", labels[1]);

  // In our implementation we always display NAME_FULL for all NAME* fields...
  suggested_fields = {NAME_MIDDLE};
  // One field at least, from suggested fields - no filter.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, /*excluded_fields*/ {},
      /*minimal_fields_shown=*/1, "en-US");
  EXPECT_EQ(u"John Doe", labels[0]);
  EXPECT_EQ(u"Jane Doe", labels[1]);

  // One field at least, from suggested fields - filter the same as suggested
  // field.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {NAME_MIDDLE},
      /*minimal_fields_shown=*/1, "en-US");
  EXPECT_EQ(std::u16string(), labels[0]);
  EXPECT_EQ(std::u16string(), labels[1]);

  // In our implementation we always display NAME_FULL for NAME_MIDDLE_INITIAL
  suggested_fields = {NAME_MIDDLE};
  // One field at least, from suggested fields - no filter.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, /*excluded_fields*/ {},
      /*minimal_fields_shown=*/1, "en-US");
  EXPECT_EQ(u"John Doe", labels[0]);
  EXPECT_EQ(u"Jane Doe", labels[1]);

  // One field at least, from suggested fields - filter same as the first non-
  // unknown suggested field.
  suggested_fields = {UNKNOWN_TYPE, NAME_FULL, ADDRESS_HOME_LINE1};
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {NAME_FULL},
      /*minimal_fields_shown=*/1, "en-US");
  EXPECT_EQ(std::u16string(u"666 Erebus St."), labels[0]);
  EXPECT_EQ(std::u16string(u"123 Letha Shore."), labels[1]);

  // No suggested fields, but non-unknown excluded field.
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles),
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt, {NAME_FULL},
      /*minimal_fields_shown=*/1, "en-US");
  EXPECT_EQ(std::u16string(u"666 Erebus St."), labels[0]);
  EXPECT_EQ(std::u16string(u"123 Letha Shore."), labels[1]);
}

// Test that we fall back to using the full name if there are no other
// distinguishing fields, but only if it makes sense given the suggested fields.
TEST_F(AutofillProfileTest, CreateInferredLabelsFallsBackToFullName) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "Johnny", "K", "Doe",
                       "doe@example.com", "", "88 Nowhere Ave.", "", "", "", "",
                       "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  FieldTypeSet suggested_fields = {NAME_LAST, ADDRESS_HOME_LINE1,
                                   EMAIL_ADDRESS};
  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {NAME_LAST},
      /*minimal_fields_shown=*/1, "en-US");
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave.", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave.", labels[1]);

  // Otherwise, we should.
  suggested_fields.insert(NAME_FIRST);
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {NAME_LAST},
      /*minimal_fields_shown=*/1, "en-US");
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., John Doe", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave., Johnny K Doe", labels[1]);
}

// Test that we use the triggering field to decide whether an additional
// differentiating label should be added.
TEST_F(
    AutofillProfileTest,
    CreateInferredLabels_TriggeringFieldUsedToDecideWhetherToAddADifferentiatingLabel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillImprovedLabels,
      {{features::
            kAutofillImprovedLabelsParamWithDifferentiatingLabelsInFrontParam
                .name,
        "true"}});

  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile();
  profile1.SetRawInfo(EMAIL_ADDRESS, u"hoa@gmail.com");
  profile2.SetRawInfo(EMAIL_ADDRESS, u"pham@gmail.com");

  // First check that when `triggering_field_type` is not present, a second
  // differentiating label is added.
  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      {&profile1, &profile2},
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/1, "en-US");
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"John H. Doe, hoa@gmail.com", labels[0]);
  EXPECT_EQ(u"John H. Doe, pham@gmail.com", labels[1]);

  // If the `triggering_field_type` is present and is unique, there is no need
  // for a second differentiating label.
  labels = AutofillProfile::CreateInferredLabels(
      {&profile1, &profile2},
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/EMAIL_ADDRESS,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/1, "en-US",
      /*use_improved_labels_order=*/true);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"John H. Doe", labels[0]);
  EXPECT_EQ(u"John H. Doe", labels[1]);

  // If the `triggering_field_type` is present and is not unique, a second
  // differentiating label is added.
  labels = AutofillProfile::CreateInferredLabels(
      {&profile1, &profile2},
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/NAME_FIRST,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/1, "en-US",
      /*use_improved_labels_order=*/true);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"hoa@gmail.com, John H. Doe", labels[0]);
  EXPECT_EQ(u"pham@gmail.com, John H. Doe", labels[1]);
}

// Test that we do not show duplicate fields in the labels.
TEST_F(AutofillProfileTest, CreateInferredLabelsNoDuplicatedFields) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "John", "", "Doe", "dojo@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  FieldTypeSet suggested_fields = {ADDRESS_HOME_LINE1, EMAIL_ADDRESS};
  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/2, "en-US");
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., doe@example.com", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave., dojo@example.com", labels[1]);
}

// Make sure that empty fields are not treated as distinguishing fields.
TEST_F(AutofillProfileTest, CreateInferredLabelsSkipsEmptyFields) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "Gogole", "", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "John", "", "Doe", "doe@example.com",
                       "Ggoole", "", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "john.doe@example.com", "Goolge", "", "", "", "", "", "",
                       "");

  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles),
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/3, "en-US");
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, doe@example.com, Gogole", labels[0]);
  EXPECT_EQ(u"John Doe, doe@example.com, Ggoole", labels[1]);
  EXPECT_EQ(u"John Doe, john.doe@example.com, Goolge", labels[2]);

  // A field must have a non-empty value for each profile to be considered a
  // distinguishing field.
  profiles[1]->SetRawInfo(ADDRESS_HOME_LINE1, u"88 Nowhere Ave.");
  labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles),
      /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/std::nullopt,
      /*excluded_fields=*/{}, /*minimal_fields_shown=*/1, "en-US");
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, doe@example.com, Gogole", labels[0]);
  EXPECT_EQ(u"John Doe, 88 Nowhere Ave., doe@example.com, Ggoole", labels[1])
      << labels[1];
  EXPECT_EQ(u"John Doe, john.doe@example.com", labels[2]);
}

// Test that labels that would otherwise have multiline values are flattened.
TEST_F(AutofillProfileTest, CreateInferredLabelsFlattensMultiLineValues) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "Apt. 42", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  FieldTypeSet suggested_fields = {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS};
  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), suggested_fields,
      /*triggering_field_type=*/std::nullopt, {NAME_FULL},
      /*minimal_fields_shown=*/1, "en-US");
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., Apt. 42", labels[0]);
}

// Test that `ADDRESS_HOME_LINE2` is used as a differentiating label if
// necessary.
TEST_F(AutofillProfileTest, CreateInferredLabelsDifferentiateByAddressLine2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillImprovedLabels,
      {{features::
            kAutofillImprovedLabelsParamWithDifferentiatingLabelsInFrontParam
                .name,
        "true"}});

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "", "",
                       "88 Nowhere Ave.", "Apt. 42", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(
      i18n_model_definition::kLegacyHierarchyCountryCode));
  test::SetProfileInfo(profiles[1].get(), "John", "", "Doe", "", "",
                       "88 Nowhere Ave.", "Apt. 43", "", "", "", "", "");

  std::vector<std::u16string> labels = AutofillProfile::CreateInferredLabels(
      ToRawPointerVector(profiles), /*suggested_fields=*/std::nullopt,
      /*triggering_field_type=*/NAME_FULL, {NAME_FULL},
      /*minimal_fields_shown=*/1, "en-US",
      /*use_improved_labels_order=*/true);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"Apt. 42, 88 Nowhere Ave.", labels[0]);
  EXPECT_EQ(u"Apt. 43, 88 Nowhere Ave.", labels[1]);
}

TEST_F(AutofillProfileTest, IsSubsetOf) {
  AutofillProfileComparator comparator("en-US");
  const AutofillProfile standard_profile = test::StandardProfile();
  const AutofillProfile subset_profile = test::SubsetOfStandardProfile();

  EXPECT_FALSE(standard_profile.IsSubsetOf(comparator, subset_profile));
  EXPECT_TRUE(subset_profile.IsSubsetOf(comparator, standard_profile));

  // Profiles are subsets of themselves.
  EXPECT_TRUE(standard_profile.IsSubsetOf(comparator, standard_profile));
  EXPECT_TRUE(subset_profile.IsSubsetOf(comparator, subset_profile));
}

TEST_F(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentMiddleNames) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "M", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Genevieve", "Marie", "Fox", "", "", "", "",
                       "", "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  // When a form has a NAME_FULL field rather than a NAME_MIDDLE field, consider
  // whether one profile's full name can be derived from the other's.
  EXPECT_TRUE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_FULL}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_FULL}));
  EXPECT_TRUE(
      profile1.IsSubsetOfForFieldSet(comparator, profile3, {NAME_FULL}));
  EXPECT_FALSE(
      profile3.IsSubsetOfForFieldSet(comparator, profile1, {NAME_FULL}));
  // True because Genevieve M Fox can be derived from Genevieve Marie Fox.
  EXPECT_TRUE(
      profile2.IsSubsetOfForFieldSet(comparator, profile3, {NAME_FULL}));
  EXPECT_FALSE(
      profile3.IsSubsetOfForFieldSet(comparator, profile2, {NAME_FULL}));

  // When a form has a NAME_MIDDLE field rather than a NAME_FULL field, consider
  // a name's constituent parts.
  EXPECT_TRUE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_MIDDLE}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_MIDDLE}));
  EXPECT_TRUE(
      profile1.IsSubsetOfForFieldSet(comparator, profile3, {NAME_MIDDLE}));
  EXPECT_FALSE(
      profile3.IsSubsetOfForFieldSet(comparator, profile1, {NAME_MIDDLE}));
  // False because the middle name M doesn't equal the middle name Marie.
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile3, {NAME_MIDDLE}));
  EXPECT_FALSE(
      profile3.IsSubsetOfForFieldSet(comparator, profile2, {NAME_MIDDLE}));
}

TEST_F(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentFirstNames) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Cynthia", "", "Fox", "", "", "", "", "", "",
                       "", "US", "");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_FULL}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_FULL}));
  EXPECT_FALSE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_FIRST}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_FIRST}));
}

TEST_F(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentLastNames) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fuller", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_FULL}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_FULL}));
  EXPECT_FALSE(
      profile1.IsSubsetOfForFieldSet(comparator, profile2, {NAME_LAST}));
  EXPECT_FALSE(
      profile2.IsSubsetOfForFieldSet(comparator, profile1, {NAME_LAST}));
}

TEST_F(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentStreetAddresses) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile1.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile1.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"274 Main St");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile2.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"275 Main Street");

  const AutofillProfileComparator comparator("en-US");
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(comparator, profile2,
                                              {ADDRESS_HOME_STREET_ADDRESS}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1,
                                              {ADDRESS_HOME_STREET_ADDRESS}));
}

TEST_F(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentNonStreetAddresses) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "274 Main St",
                       "", "Northhampton", "", "", "US", "");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "274 Main St",
                       "", "Sturbridge", "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2,
      {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1,
      {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY}));
}

TEST_F(AutofillProfileTest, SetInfo_DynamicallyCreatingAlternativeNameTree) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  // Initially the profile's country does not support alternative names, so
  // setting it should do nothing.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                        "en-US", VerificationStatus::kObserved);
  ASSERT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());

  // Changing the profile's country does to one that supports alternative names
  // should result in the value being stored correctly.
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"JP", "en-US",
                                        VerificationStatus::kObserved);
  profile.SetInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                        "en-US", VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");
}

TEST_F(AutofillProfileTest, SetInfo_DynamicallyDeletingAlternativeNameTree) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  // Initially the profile's country supports alternative names, so setting it
  // should store the value as usual.
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                        "en-US", VerificationStatus::kObserved);
  ASSERT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");

  // Changing the profile's country to one that doesn't support alternative
  // names should result in the value being wiped and not set in the future.
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"XX", "en-US",
                                        VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());
  profile.SetInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                        "en-US", VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());
}

TEST_F(AutofillProfileTest,
       SetInfo_AlternativeNameTreeNotRecratedIfCountryDoesNotChange) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  // Initially the profile's country supports alternative names, so setting it
  // should store the value as usual.
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                        "en-US", VerificationStatus::kObserved);
  ASSERT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");

  // Setting the profile's country value to the existing one, shouldn't wipe the
  // data in the tree.
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"Japan", "en-US",
                                        VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");
}

TEST_F(AutofillProfileTest, SetRawInfo_DynamicallyCreatingAlternativeNameTree) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  // Initially the profile's country does not support alternative names, so
  // setting it should do nothing.
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                           VerificationStatus::kObserved);
  ASSERT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());

  // Changing the profile's country does to one that supports alternative names
  // should result in the value being stored correctly.
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"JP",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                           VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");
}

TEST_F(AutofillProfileTest, SetRawInfo_DynamicallyDeletingAlternativeNameTree) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  // Initially the profile's country supports alternative names, so setting it
  // should store the value as usual.
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                           VerificationStatus::kObserved);
  ASSERT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), u"alt_name");

  // Changing the profile's country to one that doesn't support alternative
  // names should result in the value being wiped and not set in the future.
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"XX",
                                           VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"alt_name",
                                           VerificationStatus::kObserved);
  EXPECT_THAT(profile.GetInfo(ALTERNATIVE_GIVEN_NAME, "en-US"), IsEmpty());
}

TEST_F(AutofillProfileTest,
       IsSubsetOfForFieldSet_PostalCodesWithAndWithoutSpaces) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "H3B 2Y5", "CA", "");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "H3B2Y5", "CA", "");

  const AutofillProfileComparator comparator("en-CA");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile2,
                                             {NAME_FULL, ADDRESS_HOME_ZIP}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(comparator, profile1,
                                             {NAME_FULL, ADDRESS_HOME_ZIP}));
}

TEST_F(AutofillProfileTest,
       IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutSpacesAndPunctuation) {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "CA", "+1 (514) 444-5454");

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "CA", "15144445454");

  const AutofillProfileComparator comparator("en-CA");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST_F(AutofillProfileTest,
       IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutCodes_US) {
  // Has country and city codes.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "+1 (508) 444-5454");

  // Has a city code.
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "5084445454");

  // Has neither a country nor a city code.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "4445454");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST_F(AutofillProfileTest,
       IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutCodes_BR) {
  // Has country and city codes.
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "5521987650000");

  // Has a city code.
  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "21987650000");

  // Has neither a country nor a city code.
  AutofillProfile profile3(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile3, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "987650000");

  const AutofillProfileComparator comparator("pt-BR");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST_F(AutofillProfileTest, IsStrictSupersetOf) {
  AutofillProfileComparator comparator("en-US");
  const AutofillProfile standard_profile = test::StandardProfile();
  const AutofillProfile subset_profile = test::SubsetOfStandardProfile();
  const AutofillProfile different_profile =
      test::DifferentFromStandardProfile();

  EXPECT_TRUE(standard_profile.IsStrictSupersetOf(comparator, subset_profile));
  EXPECT_FALSE(subset_profile.IsStrictSupersetOf(comparator, standard_profile));
  EXPECT_FALSE(
      standard_profile.IsStrictSupersetOf(comparator, different_profile));

  // Profiles are not strict supersets of themselves.
  EXPECT_FALSE(
      standard_profile.IsStrictSupersetOf(comparator, standard_profile));
  EXPECT_FALSE(subset_profile.IsStrictSupersetOf(comparator, subset_profile));
}

TEST_F(AutofillProfileTest, TestFinalizeAfterImport) {
  // A profile with just a full name should be finalizeable.
  {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Peter Pan",
                                             VerificationStatus::kObserved);
    EXPECT_TRUE(profile.FinalizeAfterImport());
    EXPECT_EQ(profile.GetRawInfo(NAME_FIRST), u"Peter");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_FIRST),
              VerificationStatus::kParsed);
    EXPECT_EQ(profile.GetRawInfo(NAME_LAST), u"Pan");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_LAST),
              VerificationStatus::kParsed);
  }
  // A profile with both a NAME_FULL and NAME_FIRST is currently not, because
  // the full name is likely to already contain the information in the first
  // name. The current completion logic does not support such a scenario because
  // it is highly likely that this scenario is caused by a classification error
  // and would not yield a correctly imported name.
  {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Peter Pan",
                                             VerificationStatus::kObserved);
    profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Michael",
                                             VerificationStatus::kObserved);
    EXPECT_FALSE(profile.FinalizeAfterImport());
  }
}

TEST_F(AutofillProfileTest, TestFinalizeAfterImportUserVerified) {
  // This test checks the scenario where the user modified the full name in
  // the settings page. The first and last name should be parsed and
  // `FinalizeAfterImport` should return true.
  {
    AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Peter Pan",
                                             VerificationStatus::kUserVerified);
    profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Michael",
                                             VerificationStatus::kObserved);
    EXPECT_TRUE(profile.FinalizeAfterImport());
    EXPECT_EQ(profile.GetRawInfo(NAME_FIRST), u"Peter");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_FIRST),
              VerificationStatus::kParsed);
    EXPECT_EQ(profile.GetRawInfo(NAME_LAST), u"Pan");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_LAST),
              VerificationStatus::kParsed);
    EXPECT_EQ(profile.GetRawInfo(NAME_LAST_FIRST), u"");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_LAST_FIRST),
              VerificationStatus::kParsed);
    EXPECT_EQ(profile.GetRawInfo(NAME_LAST_SECOND), u"Pan");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_LAST_SECOND),
              VerificationStatus::kParsed);
    EXPECT_EQ(profile.GetRawInfo(NAME_FULL), u"Peter Pan");
    EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
              VerificationStatus::kUserVerified);
  }
}

// Tests whether calling `FinalizeAfterImport` where a root node is user
// verified to be empty, wipes the data from subcomponents.
TEST_F(AutofillProfileTest, TestFinalizeAfterImportUserVerifiedEmpty) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  AutofillProfile profile(AddressCountryCode("JP"));
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_FULL_NAME, u"",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_FAMILY_NAME, u"Smith",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ALTERNATIVE_GIVEN_NAME, u"John",
                                           VerificationStatus::kObserved);
  EXPECT_TRUE(profile.FinalizeAfterImport());
  EXPECT_TRUE(profile.GetRawInfo(ALTERNATIVE_FULL_NAME).empty());
  EXPECT_EQ(profile.GetVerificationStatus(ALTERNATIVE_FULL_NAME),
            VerificationStatus::kFormatted);
  EXPECT_TRUE(profile.GetRawInfo(ALTERNATIVE_FAMILY_NAME).empty());
  EXPECT_EQ(profile.GetVerificationStatus(ALTERNATIVE_FAMILY_NAME),
            VerificationStatus::kNoStatus);
  EXPECT_TRUE(profile.GetRawInfo(ALTERNATIVE_GIVEN_NAME).empty());
  EXPECT_EQ(profile.GetVerificationStatus(ALTERNATIVE_GIVEN_NAME),
            VerificationStatus::kNoStatus);
}

TEST_F(AutofillProfileTest, SetAndGetRawInfoWithValidationStatus) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // An unsupported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(UNKNOWN_TYPE),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(profile.GetVerificationStatus(PHONE_HOME_NUMBER),
            VerificationStatus::kNoStatus);

  // An unassigned supported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kNoStatus);

  // Set a value with verification status and verify the results.
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"full name",
                                           VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), u"full name");
}

TEST_F(AutofillProfileTest, SetAndGetInfoWithValidationStatus) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // An unsupported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(UNKNOWN_TYPE),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(profile.GetVerificationStatus(PHONE_HOME_NUMBER),
            VerificationStatus::kNoStatus);

  // An unassigned supported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kNoStatus);

  // Set a value with verification status and verify the results.
  profile.SetInfoWithVerificationStatus(NAME_FULL, u"full name", "en-US",
                                        VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), u"full name");

  // Settings an unknown type should result in false.
  EXPECT_FALSE(profile.SetInfoWithVerificationStatus(
      UNKNOWN_TYPE, u"DM", "en-US", VerificationStatus::kFormatted));

  // Set a value with verification status and verify the results.
  EXPECT_TRUE(profile.SetInfoWithVerificationStatus(
      NAME_MIDDLE_INITIAL, u"MK", "en-US", VerificationStatus::kFormatted));
  EXPECT_EQ(profile.GetVerificationStatus(NAME_MIDDLE_INITIAL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_MIDDLE_INITIAL), u"MK");

  // Set a value with verification status and verify the results.
  EXPECT_TRUE(profile.SetInfoWithVerificationStatus(
      NAME_MIDDLE_INITIAL, u"CS", "en-US", VerificationStatus::kFormatted));
  EXPECT_EQ(profile.GetVerificationStatus(NAME_MIDDLE_INITIAL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_MIDDLE_INITIAL), u"CS");
}

TEST_F(AutofillProfileTest, MergeDataFrom_DifferentProfile) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  SetupTestProfile(a);

  // Create an identical profile except that the new profile:
  //   (1) Has a different address line 2,
  //   (2) Lacks a company name,
  //   (3) Has a different full name, and
  //   (4) Has a language code.
  AutofillProfile b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LINE2, u"Unit 5, area 51",
                                     VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(COMPANY_NAME, std::u16string(),
                                     VerificationStatus::kObserved);

  b.SetRawInfo(NAME_MIDDLE, u"M.");
  b.SetRawInfo(NAME_FULL, u"Marion M. Morrison");
  b.set_language_code("en");
  b.FinalizeAfterImport();
  a.FinalizeAfterImport();

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_EQ("Unit 5, area 51",
            base::UTF16ToUTF8(a.GetRawInfo(ADDRESS_HOME_LINE2)));
  EXPECT_EQ(u"Fox", a.GetRawInfo(COMPANY_NAME));
  std::u16string name = a.GetInfo(NAME_FULL, "en-US");
  EXPECT_EQ(u"Marion Mitchell Morrison", name);
  EXPECT_EQ("en", a.language_code());
}

TEST_F(AutofillProfileTest, MergeDataFrom_SameProfile) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  SetupTestProfile(a);

  // The profile has no full name yet. Merge will add it.
  AutofillProfile b = a;
  // For the new structured profiles, the profile must be altered for the
  // merging to have an effect. The verification status of the full name is set
  // to user verified.
  b.SetRawInfoWithVerificationStatus(NAME_FULL, b.GetRawInfo(NAME_FULL),
                                     VerificationStatus::kUserVerified);
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_EQ(1u, a.usage_history().use_count());

  // Now the profile is fully populated. Merging it again has no effect (except
  // for usage statistics).
  AutofillProfile c = a;
  c.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  c.usage_history().set_use_count(3);
  EXPECT_FALSE(a.MergeDataFrom(c, "en-US"));
  // Merge has not modified anything.
  EXPECT_EQ(3u, a.usage_history().use_count());
}

// Tests that when merging two profiles, the token quality is merged.
TEST_F(AutofillProfileTest, MergeDataFrom_TokenQuality) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile b((i18n_model_definition::kLegacyHierarchyCountryCode));
  // Set the same state for both profiles. Expect that a's quality will be kept.
  a.SetRawInfo(ADDRESS_HOME_STATE, u"TX");
  b.SetRawInfo(ADDRESS_HOME_STATE, u"TX");
  test_api(a.token_quality())
      .AddObservation(ADDRESS_HOME_STATE, ObservationType::kAccepted);
  test_api(b.token_quality())
      .AddObservation(ADDRESS_HOME_STATE, ObservationType::kEditedFallback);

  // Only set a city for b. Expect that its quality is carried over.
  b.SetRawInfo(ADDRESS_HOME_CITY, u"City");
  test_api(b.token_quality())
      .AddObservation(ADDRESS_HOME_CITY, ObservationType::kAccepted);

  // Finalize, merge and verify expectations.
  a.FinalizeAfterImport();
  b.FinalizeAfterImport();
  ASSERT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_THAT(
      a.token_quality().GetObservationTypesForFieldType(ADDRESS_HOME_STATE),
      testing::UnorderedElementsAre(ObservationType::kAccepted));
  EXPECT_THAT(
      a.token_quality().GetObservationTypesForFieldType(ADDRESS_HOME_CITY),
      testing::UnorderedElementsAre(ObservationType::kAccepted));
}

TEST_F(AutofillProfileTest, OverwriteName_AddNameFull) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");

  AutofillProfile b = a;
  a.FinalizeAfterImport();

  b.SetRawInfoWithVerificationStatus(NAME_FULL, u"Marion Mitchell Morrison",
                                     VerificationStatus::kUserVerified);
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Marion Mitchell Morrison", a.GetRawInfo(NAME_FULL));
}

// Tests that OverwriteName overwrites the name parts if they have different
// case.
TEST_F(AutofillProfileTest, OverwriteName_DifferentCase) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile b = a;

  a.SetRawInfoWithVerificationStatus(NAME_FIRST, u"marion",
                                     VerificationStatus::kObserved);
  a.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"mitchell",
                                     VerificationStatus::kObserved);
  a.SetRawInfoWithVerificationStatus(NAME_LAST, u"morrison",
                                     VerificationStatus::kObserved);

  b.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Marion",
                                     VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Mitchell",
                                     VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison",
                                     VerificationStatus::kObserved);

  a.FinalizeAfterImport();
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
}

TEST_F(AutofillProfileTest, AssignmentOperator) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison", "marion@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "12345678910");

  // Result of assignment should be logically equal to the original profile.
  AutofillProfile b(i18n_model_definition::kLegacyHierarchyCountryCode);
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = *&a;  // The *& defeats Clang's -Wself-assign warning.
  EXPECT_TRUE(a == b);
}

TEST_F(AutofillProfileTest, Copy) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison", "marion@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "12345678910");

  // Clone should be logically equal to the original.
  AutofillProfile b(a);
  EXPECT_TRUE(a == b);
}

TEST_F(AutofillProfileTest, Compare) {
  AutofillProfile a(i18n_model_definition::kLegacyHierarchyCountryCode);
  AutofillProfile b(i18n_model_definition::kLegacyHierarchyCountryCode);

  // Empty profiles are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_EQ(0, a.Compare(b));

  // Different values produce non-zero results.
  test::SetProfileInfo(&a, "Jimmy", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  test::SetProfileInfo(&b, "Ringo", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));

  // Phone numbers are compared by the full number, including the area code.
  // This is a regression test for http://crbug.com/163024
  test::SetProfileInfo(&a, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr,
                       "650.555.4321");
  test::SetProfileInfo(&b, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr,
                       "408.555.4321");
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));

  // Addresses are compared in full. Regression test for http://crbug.com/375545
  test::SetProfileInfo(&a, "John", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  a.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"line one\nline two");
  test::SetProfileInfo(&b, "John", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  b.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"line one\nline two\nline three");
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

// For each structured profile tokens, test the comparison operator for both the
// value and the status.
// TODO(crbug.com/40275657): Extend this test to cover i18n profiles.
TEST_F(AutofillProfileTest, Compare_StructuredTypes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillUseINAddressModel};
  // Those types do store a verification status.
  FieldTypeSet structured_types{
      NAME_FULL,
      NAME_FIRST,
      NAME_MIDDLE,
      NAME_LAST,
      NAME_LAST_FIRST,
      NAME_LAST_SECOND,
      NAME_LAST_CONJUNCTION,
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_LANDMARK,
      ADDRESS_HOME_OVERFLOW,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS,
      ADDRESS_HOME_BETWEEN_STREETS_1,
      ADDRESS_HOME_BETWEEN_STREETS_2,
      ADDRESS_HOME_ADMIN_LEVEL2,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_FLOOR,
      ADDRESS_HOME_APT,
      ADDRESS_HOME_APT_TYPE,
      ADDRESS_HOME_APT_NUM,
  };

  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // Those values are legal for all tokens.
  const std::u16string value1 = u"DE";
  const std::u16string value2 = u"US";

  const VerificationStatus status1 = VerificationStatus::kObserved;
  const VerificationStatus status2 = VerificationStatus::kParsed;

  ASSERT_NE(value1, value2);
  ASSERT_NE(status1, status2);
  std::vector<AddressCountryCode> country_codes;
  std::ranges::transform(country_data_map->country_codes(),
                         back_inserter(country_codes),
                         [](const std::string& country_code) {
                           return AddressCountryCode(country_code);
                         });
  // Include the legacy country code as well.
  country_codes.push_back(i18n_model_definition::kLegacyHierarchyCountryCode);

  for (const AddressCountryCode& country_code : country_codes) {
    SCOPED_TRACE(testing::Message()
                 << "Testing the Compare method for the country: "
                 << country_code);
    for (FieldType type : structured_types) {
      if (!i18n_model_definition::IsTypeEnabledForCountry(type, country_code)) {
        continue;
      }
      // Create two empty profiles to test the tokens individually.
      AutofillProfile profile1(country_code);
      AutofillProfile profile2(country_code);

      SCOPED_TRACE(testing::Message()
                   << "Testing the Compare method for the type: "
                   << FieldTypeToStringView(type));

      SCOPED_TRACE(testing::Message()
                   << "Verify the correct result for identical values");
      profile1.SetRawInfoWithVerificationStatus(type, value1, status1);
      profile2.SetRawInfoWithVerificationStatus(type, value1, status1);
      EXPECT_EQ(profile1.Compare(profile2), 0);

      SCOPED_TRACE(testing::Message() << "Verify the sensitivity to the value");
      profile2.SetRawInfoWithVerificationStatus(type, value2, status1);
      EXPECT_NE(profile1.Compare(profile2), 0);

      SCOPED_TRACE(testing::Message()
                   << "Verify the sensitivity to the status");
      profile2.SetRawInfoWithVerificationStatus(type, value1, status2);
      EXPECT_NE(profile1.Compare(profile2), 0);
    }
  }
}

TEST_F(AutofillProfileTest, IsPresentButInvalid) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_LANDMARK));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_BETWEEN_STREETS));

  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_LANDMARK));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_BETWEEN_STREETS));

  profile.SetRawInfo(ADDRESS_HOME_STATE, u"C");
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90");
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90210");
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"310");
  EXPECT_TRUE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"(310) 310-6000");
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillProfileTest, SetRawInfoPreservesLineBreaks) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     u"123 Super St.\n"
                     u"Apt. #42");
  EXPECT_EQ(
      u"123 Super St.\n"
      u"Apt. #42",
      profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST_F(AutofillProfileTest, SetInfoPreservesLineBreaks) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                  u"123 Super St.\n"
                  u"Apt. #42",
                  "en-US");
  EXPECT_EQ(
      u"123 Super St.\n"
      u"Apt. #42",
      profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST_F(AutofillProfileTest, SetRawInfoDoesntTrimWhitespace) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(EMAIL_ADDRESS, u"\tuser@example.com    ");
  EXPECT_EQ(u"\tuser@example.com    ", profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST_F(AutofillProfileTest, SetRawInfoWorksForLandmark) {
  AutofillProfile profile(AddressCountryCode("MX"));

  profile.SetRawInfo(ADDRESS_HOME_LANDMARK, u"Red tree");
  EXPECT_EQ(u"Red tree", profile.GetRawInfo(ADDRESS_HOME_LANDMARK));
}

TEST_F(AutofillProfileTest, SetRawInfoWorksForBetweenStreets) {
  AutofillProfile profile(AddressCountryCode("MX"));

  profile.SetRawInfo(ADDRESS_HOME_BETWEEN_STREETS, u"Between streets example");
  EXPECT_EQ(u"Between streets example",
            profile.GetRawInfo(ADDRESS_HOME_BETWEEN_STREETS));
}

TEST_F(AutofillProfileTest, SetInfoTrimsWhitespace) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetInfo(EMAIL_ADDRESS, u"\tuser@example.com    ", "en-US");
  EXPECT_EQ(u"user@example.com", profile.GetRawInfo(EMAIL_ADDRESS));
}

// Test that the label is correctly set and retrieved from the profile.
TEST_F(AutofillProfileTest, SetAndGetProfileLabels) {
  AutofillProfile p(i18n_model_definition::kLegacyHierarchyCountryCode);
  EXPECT_EQ(p.profile_label(), std::string());

  p.set_profile_label("my label");
  EXPECT_EQ(p.profile_label(), "my label");
}

TEST_F(AutofillProfileTest, LabelsInAssignmentAndComparisonOperator) {
  AutofillProfile p1(i18n_model_definition::kLegacyHierarchyCountryCode);
  p1.set_profile_label("my label");

  AutofillProfile p2(i18n_model_definition::kLegacyHierarchyCountryCode);
  p2 = p1;

  // Check that the label was assigned correctly to p2.
  EXPECT_EQ(p2.profile_label(), "my label");

  // Now test that the comparison returns false if the label is not the same.
  ASSERT_EQ(p1, p2);
  p2.set_profile_label("another label");
  EXPECT_NE(p1, p2);
}

// Tests that `RecordUseAndLog()` only increments the use count if at least 60
// seconds have passed.
TEST_F(AutofillProfileTest, RecordUseAndLog_Delay) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // AutofillProfile is initialized with a `use_count()` of 1 and a last used
  // date set to the current time.
  ASSERT_EQ(profile.usage_history().use_count(), 1u);
  // 60 seconds pass. `RecordAndLogUse()` increments the use count.
  task_environment().FastForwardBy(base::Seconds(60));
  profile.RecordAndLogUse();
  EXPECT_EQ(profile.usage_history().use_count(), 2u);
  // Not enough time passes.
  task_environment().FastForwardBy(base::Seconds(5));
  profile.RecordAndLogUse();
  EXPECT_EQ(profile.usage_history().use_count(), 2u);
  // Test that waiting times are not added up. 5 + 55 seconds don't suffice.
  task_environment().FastForwardBy(base::Seconds(55));
  profile.RecordAndLogUse();
  EXPECT_EQ(profile.usage_history().use_count(), 2u);
}

TEST_F(AutofillProfileTest, ConvertToAccountProfile) {
  const AutofillProfile kLocalProfile = test::GetFullProfile();
  ASSERT_EQ(kLocalProfile.record_type(),
            AutofillProfile::RecordType::kLocalOrSyncable);
  const AutofillProfile kAccountProfile =
      kLocalProfile.ConvertToAccountProfile();
  EXPECT_EQ(kAccountProfile.record_type(),
            AutofillProfile::RecordType::kAccount);
  EXPECT_EQ(kAccountProfile.initial_creator_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_EQ(kAccountProfile.last_modifier_id(),
            AutofillProfile::kInitialCreatorOrModifierChrome);
  EXPECT_NE(kLocalProfile.guid(), kAccountProfile.guid());
  EXPECT_EQ(kLocalProfile.Compare(kAccountProfile), 0);
}

TEST_F(AutofillProfileTest, ConvertToLocalOrSyncableProfile) {
  const AutofillProfile account_name_email_profile =
      test::AccountNameEmailProfile();
  ASSERT_EQ(account_name_email_profile.record_type(),
            AutofillProfile::RecordType::kAccountNameEmail);
  const AutofillProfile local_or_syncable_profile =
      account_name_email_profile.ConvertToLocalOrSyncableProfile();
  EXPECT_EQ(local_or_syncable_profile.record_type(),
            AutofillProfile::RecordType::kLocalOrSyncable);
  EXPECT_NE(account_name_email_profile.guid(),
            local_or_syncable_profile.guid());
  EXPECT_EQ(account_name_email_profile.Compare(local_or_syncable_profile), 0);
}

TEST_F(AutofillProfileTest, RemoveInaccessibleProfileValues) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillUseINAddressModel};
  // Returns true if at least one field was removed.
  auto RemoveInaccessibleProfileValues = [](AutofillProfile& profile) {
    const FieldTypeSet inaccessible_fields =
        profile.FindInaccessibleProfileValues();
    profile.ClearFields(inaccessible_fields);
    return !inaccessible_fields.empty();
  };

  AutofillProfile actual_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  actual_profile.SetRawInfo(NAME_FIRST, u"Florian");

  // State is uncommon in Bolivia and inaccessible in the settings. Expect it
  // to be removed.
  actual_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"BO");
  AutofillProfile expected_profile = actual_profile;
  actual_profile.SetRawInfo(ADDRESS_HOME_STATE, u"Dummy state");
  EXPECT_TRUE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);

  // There are no ZIP codes in Angola.
  actual_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"AO");
  expected_profile = actual_profile;
  actual_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
  EXPECT_TRUE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);

  // Dependent locality exist in India as part of the street address, which is a
  // settings visible node. These should be preserved despite not being
  // recognized by libaddressinput.
  actual_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"IN");
  actual_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Locality");
  expected_profile = actual_profile;
  EXPECT_FALSE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);

  // If no country is set, the US requirements are used.
  // The US uses both ZIP codes and states.
  actual_profile.ClearFields(
      {ADDRESS_HOME_COUNTRY, ADDRESS_HOME_DEPENDENT_LOCALITY});
  actual_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  actual_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
  expected_profile = actual_profile;
  EXPECT_FALSE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);
}

TEST_F(AutofillProfileTest, GetStorableTypeOf) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  // Test that additional types are mapped to their stored types
  EXPECT_EQ(profile.GetStorableTypeOf(ADDRESS_HOME_LINE1),
            ADDRESS_HOME_STREET_ADDRESS);
  EXPECT_EQ(profile.GetStorableTypeOf(NAME_MIDDLE_INITIAL), NAME_MIDDLE);
  EXPECT_EQ(profile.GetStorableTypeOf(PHONE_HOME_CITY_CODE),
            PHONE_HOME_WHOLE_NUMBER);
  // Test that stored types are returned as-is.
  EXPECT_EQ(profile.GetStorableTypeOf(ADDRESS_HOME_STATE), ADDRESS_HOME_STATE);
  EXPECT_EQ(profile.GetStorableTypeOf(COMPANY_NAME), COMPANY_NAME);
}

struct GetUserVisibleTypesTestCase {
  const FieldTypeSet expected_types;
  const AddressCountryCode country_code;
};

class GetUserVisibleTypesTest
    : public AutofillProfileTest,
      public testing::WithParamInterface<GetUserVisibleTypesTestCase> {};

TEST_P(GetUserVisibleTypesTest, GetUserVisibleTypes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  const GetUserVisibleTypesTestCase& test = GetParam();

  AutofillProfile profile(test.country_code);
  EXPECT_EQ(profile.GetUserVisibleTypes(), test.expected_types);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillProfileTest,
    GetUserVisibleTypesTest,
    testing::Values(GetUserVisibleTypesTestCase(
                        FieldTypeSet({NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                      ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                      ADDRESS_HOME_ZIP, EMAIL_ADDRESS,
                                      PHONE_HOME_WHOLE_NUMBER, COMPANY_NAME}),
                        AddressCountryCode("US")),
                    GetUserVisibleTypesTestCase(
                        FieldTypeSet({NAME_FULL, ALTERNATIVE_FULL_NAME,
                                      ADDRESS_HOME_STREET_ADDRESS,
                                      ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP,
                                      EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER,
                                      COMPANY_NAME}),
                        AddressCountryCode("JP")),
                    GetUserVisibleTypesTestCase(
                        FieldTypeSet({NAME_FULL, ADDRESS_HOME_STREET_ADDRESS,
                                      ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                      ADDRESS_HOME_ZIP, EMAIL_ADDRESS,
                                      PHONE_HOME_WHOLE_NUMBER, COMPANY_NAME}),
                        AddressCountryCode("GB"))),
    [](const testing::TestParamInfo<GetUserVisibleTypesTestCase>& info) {
      return info.param.country_code.value();
    });

// Tests that `AutofillProfile::RecordUseAndLog()` logs days until first usage.
TEST_F(AutofillProfileTest, EmitsDaysUntilFirstUsageProfile) {
  const size_t expect_number_of_days = 237;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  task_environment().FastForwardBy(base::Days(expect_number_of_days));

  base::HistogramTester histogram_tester;
  profile.RecordAndLogUse();
  histogram_tester.ExpectUniqueSample("Autofill.DaysUntilFirstUsage.Profile",
                                      expect_number_of_days, 1);

  profile.RecordAndLogUse();
  EXPECT_EQ(
      histogram_tester.GetAllSamples("Autofill.DaysUntilFirstUsage.Profile")
          .size(),
      1UL);
}

}  // namespace

}  // namespace autofill
