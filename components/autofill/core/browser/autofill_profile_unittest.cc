// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

namespace {

base::string16 GetLabel(AutofillProfile* profile) {
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(profile);
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, "en-US", &labels);
  return labels[0];
}

void SetupValidatedTestProfile(AutofillProfile& profile) {
  profile.set_guid(base::GenerateGUID());
  profile.set_origin(kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile.SetClientValidityFromBitfieldValue(1984);
  profile.set_is_client_validity_states_updated(true);
}

std::vector<AutofillProfile*> ToRawPointerVector(
    const std::vector<std::unique_ptr<AutofillProfile>>& list) {
  std::vector<AutofillProfile*> result;
  for (const auto& item : list)
    result.push_back(item.get());
  return result;
}

}  // namespace

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST(AutofillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutofillProfile profile0(base::GenerateGUID(), test::kEmptyOrigin);
  // Empty profile - nothing to update.
  base::string16 summary0 = GetLabel(&profile0);
  EXPECT_EQ(base::string16(), summary0);

  // Case 0a/empty name and address, so the first two fields of the rest of the
  // data is used: "Hollywood, CA"
  AutofillProfile profile00(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile00, "", "", "",
      "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA", "91601", "US",
      "16505678910");
  base::string16 summary00 = GetLabel(&profile00);
  EXPECT_EQ(ASCIIToUTF16("Hollywood, CA"), summary00);

  // Case 1: "<address>" without line 2.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "", "", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary1 = GetLabel(&profile1);
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., Hollywood"), summary1);

  // Case 1a: "<address>" with line 2.
  AutofillProfile profile1a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1a, "", "", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary1a = GetLabel(&profile1a);
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., unit 5"), summary1a);

  // Case 2: "<lastname>"
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary2 = GetLabel(&profile2);
  // Summary includes full name, to the maximal extent available.
  EXPECT_EQ(ASCIIToUTF16("Mitchell Morrison, Hollywood"), summary2);

  // Case 3: "<lastname>, <address>"
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "",
      "Hollywood", "CA", "91601", "US", "16505678910");
  base::string16 summary3 = GetLabel(&profile3);
  EXPECT_EQ(ASCIIToUTF16("Mitchell Morrison, 123 Zoo St."), summary3);

  // Case 4: "<firstname>"
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA", "91601", "US",
      "16505678910");
  base::string16 summary4 = GetLabel(&profile4);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, Hollywood"), summary4);

  // Case 5: "<firstname>, <address>"
  AutofillProfile profile5(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary5 = GetLabel(&profile5);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, 123 Zoo St."), summary5);

  // Case 6: "<firstname> <lastname>"
  AutofillProfile profile6(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary6 = GetLabel(&profile6);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, Hollywood"),
            summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutofillProfile profile7(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile7, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "16505678910");
  base::string16 summary7 = GetLabel(&profile7);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, 123 Zoo St."), summary7);

  // Case 7a: "<firstname> <lastname>, <address>" - same as #7, except for
  // e-mail.
  AutofillProfile profile7a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile7a, "Marion", "Mitchell",
    "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
    "Hollywood", "CA", "91601", "US", "16505678910");
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile7);
  profiles.push_back(&profile7a);
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, "en-US", &labels);
  ASSERT_EQ(profiles.size(), labels.size());
  summary7 = labels[0];
  base::string16 summary7a = labels[1];
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., johnwayne@me.xyz"), summary7);
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., marion@me.xyz"), summary7a);
}

TEST(AutofillProfileTest, AdjustInferredLabels) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CA", "91111", "US", "16502111111");
  profiles.push_back(std::make_unique<AutofillProfile>(
      base::GenerateGUID(), "http://www.example.com/"));
  test::SetProfileInfo(profiles[1].get(), "Jane", "", "Doe",
                       "janedoe@tertium.com", "Pluto Inc.", "123 Letha Shore.",
                       "", "Dis", "CA", "91222", "US", "12345678910");
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);

  profiles.push_back(
      std::make_unique<AutofillProfile>(base::GenerateGUID(), kSettingsOrigin));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "johndoe@tertium.com", "Underworld", "666 Erebus St.",
                       "", "Elysium", "CA", "91111", "US", "16502111111");
  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);

  // Profile 0 and 2 inferred label now includes an e-mail.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com"),
            labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@tertium.com"),
            labels[2]);

  profiles.resize(2);

  profiles.push_back(
      std::make_unique<AutofillProfile>(base::GenerateGUID(), std::string()));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CO",  // State is different
                       "91111", "US", "16502111111");

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);

  // Profile 0 and 2 inferred label now includes a state.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO"), labels[2]);

  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[3].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CO",  // State is different for some.
                       "91111", "US",
                       "16504444444");  // Phone is different for some.

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);
  ASSERT_EQ(4U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, 16502111111"),
            labels[2]);
  // This one differs from other ones by unique phone, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, 16504444444"),
            labels[3]);

  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[4].get(), "John", "", "Doe",
                       "johndoe@styx.com",  // E-Mail is different for some.
                       "Underworld", "666 Erebus St.", "", "Elysium",
                       "CO",  // State is different for some.
                       "91111", "US",
                       "16504444444");  // Phone is different for some.

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);
  ASSERT_EQ(5U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@hades.com,"
                         " 16502111111"), labels[2]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@hades.com,"
                         " 16504444444"), labels[3]);
  // This one differs from other ones by unique e-mail, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@styx.com"),
            labels[4]);
}

TEST(AutofillProfileTest, CreateInferredLabelsI18n_CH) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles.back().get(), "H.", "R.", "Giger",
                       "hrgiger@beispiel.com", "Beispiel Inc",
                       "Brandschenkestrasse 110", "", "Zurich", "", "8002",
                       "CH", "+41 44-668-1800");
  profiles.back()->set_language_code("de_CH");
  static const char* kExpectedLabels[] = {
    "",
    "H. R. Giger",
    "H. R. Giger, Brandschenkestrasse 110",
    "H. R. Giger, Brandschenkestrasse 110, Zurich",
    "H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich",
    "Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich",
    "Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
        "Switzerland",
    "Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
        "Switzerland, hrgiger@beispiel.com",
    "Beispiel Inc, H. R. Giger, Brandschenkestrasse 110, CH-8002 Zurich, "
        "Switzerland, hrgiger@beispiel.com, +41 44-668-1800",
  };

  std::vector<base::string16> labels;
  for (size_t i = 0; i < arraysize(kExpectedLabels); ++i) {
    AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                          UNKNOWN_TYPE, i, "en-US", &labels);
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(UTF8ToUTF16(kExpectedLabels[i]), labels.back());
  }
}


TEST(AutofillProfileTest, CreateInferredLabelsI18n_FR) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles.back().get(), "Antoine", "", "de Saint-Exupéry",
                       "antoine@exemple.com", "Exemple Inc", "8 Rue de Londres",
                       "", "Paris", "", "75009", "FR", "+33 (0) 1 42 68 53 00");
  profiles.back()->set_language_code("fr_FR");
  profiles.back()->SetInfo(
      AutofillType(ADDRESS_HOME_SORTING_CODE), UTF8ToUTF16("CEDEX"), "en-US");
  static const char* kExpectedLabels[] = {
      "",
      "Antoine de Saint-Exupéry",
      "Antoine de Saint-Exupéry, 8 Rue de Londres",
      "Antoine de Saint-Exupéry, 8 Rue de Londres, Paris",
      "Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris",
      "Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris CEDEX",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris "
          "CEDEX",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris "
          "CEDEX, France",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris "
          "CEDEX, France, antoine@exemple.com",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris "
          "CEDEX, France, antoine@exemple.com, +33 (0) 1 42 68 53 00",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris "
          "CEDEX, France, antoine@exemple.com, +33 (0) 1 42 68 53 00",
  };

  std::vector<base::string16> labels;
  for (size_t i = 0; i < arraysize(kExpectedLabels); ++i) {
    AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                          UNKNOWN_TYPE, i, "en-US", &labels);
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(UTF8ToUTF16(kExpectedLabels[i]), labels.back());
  }
}

TEST(AutofillProfileTest, CreateInferredLabelsI18n_KR) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles.back().get(), "Park", "", "Jae-sang",
                       "park@yeleul.com", "Yeleul Inc",
                       "Gangnam Finance Center", "152 Teheran-ro", "Gangnam-Gu",
                       "Seoul", "135-984", "KR", "+82-2-531-9000");
  profiles.back()->set_language_code("ko_Latn");
  profiles.back()->SetInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           UTF8ToUTF16("Yeoksam-Dong"), "en-US");
  static const char* kExpectedLabels[] = {
      "",
      "Park Jae-sang",
      "Park Jae-sang, Gangnam Finance Center",
      "Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro",
      "Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong",
      "Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
          "Gangnam-Gu",
      "Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
          "Gangnam-Gu, Seoul",
      "Park Jae-sang, Gangnam Finance Center, 152 Teheran-ro, Yeoksam-Dong, "
          "Gangnam-Gu, Seoul, 135-984",
      "Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
          "Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984",
      "Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
          "Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea",
      "Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
          "Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea, "
          "park@yeleul.com",
      "Park Jae-sang, Yeleul Inc, Gangnam Finance Center, 152 Teheran-ro, "
          "Yeoksam-Dong, Gangnam-Gu, Seoul, 135-984, South Korea, "
          "park@yeleul.com, +82-2-531-9000",
  };

  std::vector<base::string16> labels;
  for (size_t i = 0; i < arraysize(kExpectedLabels); ++i) {
    AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                          UNKNOWN_TYPE, i, "en-US", &labels);
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(UTF8ToUTF16(kExpectedLabels[i]), labels.back());
  }
}

TEST(AutofillProfileTest, CreateInferredLabelsI18n_JP_Latn) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles.back().get(), "Miku", "", "Hatsune",
                       "miku@rei.com", "Rei Inc", "Roppongi Hills Mori Tower",
                       "6-10-1 Roppongi, Minato-ku", "", "Tokyo", "106-6126",
                       "JP", "+81-3-6384-9000");
  profiles.back()->set_language_code("ja_Latn");
  static const char* kExpectedLabels[] = {
    "",
    "Miku Hatsune",
    "Miku Hatsune, Roppongi Hills Mori Tower",
    "Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku",
    "Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku, "
        "Tokyo",
    "Miku Hatsune, Roppongi Hills Mori Tower, 6-10-1 Roppongi, Minato-ku, "
        "Tokyo, 106-6126",
    "Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
        "Minato-ku, Tokyo, 106-6126",
    "Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
        "Minato-ku, Tokyo, 106-6126, Japan",
    "Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
        "Minato-ku, Tokyo, 106-6126, Japan, miku@rei.com",
    "Miku Hatsune, Rei Inc, Roppongi Hills Mori Tower, 6-10-1 Roppongi, "
        "Minato-ku, Tokyo, 106-6126, Japan, miku@rei.com, +81-3-6384-9000",
  };

  std::vector<base::string16> labels;
  for (size_t i = 0; i < arraysize(kExpectedLabels); ++i) {
    AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                          UNKNOWN_TYPE, i, "en-US", &labels);
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(UTF8ToUTF16(kExpectedLabels[i]), labels.back());
  }
}

TEST(AutofillProfileTest, CreateInferredLabelsI18n_JP_ja) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles.back().get(), "ミク", "", "初音",
                       "miku@rei.com", "例", "港区六本木ヒルズ森タワー",
                       "六本木 6-10-1", "", "東京都", "106-6126", "JP",
                       "03-6384-9000");
  profiles.back()->set_language_code("ja_JP");
  static const char* kExpectedLabels[] = {
      "",
      "初音ミク",
      "港区六本木ヒルズ森タワー初音ミク",
      "港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
      "東京都港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
      "〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1初音ミク",
      "〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク",
      "〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, Japan",
      "〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, Japan, "
      "miku@rei.com",
      "〒106-6126東京都港区六本木ヒルズ森タワー六本木 6-10-1例初音ミク, Japan, "
      "miku@rei.com, 03-6384-9000",
  };

  std::vector<base::string16> labels;
  for (size_t i = 0; i < arraysize(kExpectedLabels); ++i) {
    AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                          UNKNOWN_TYPE, i, "en-US", &labels);
    ASSERT_FALSE(labels.empty());
    EXPECT_EQ(UTF8ToUTF16(kExpectedLabels[i]), labels.back());
  }
}

TEST(AutofillProfileTest, CreateInferredLabels) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe",
                       "johndoe@hades.com", "Underworld", "666 Erebus St.", "",
                       "Elysium", "CA", "91111", "US", "16502111111");
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[1].get(), "Jane", "", "Doe",
                       "janedoe@tertium.com", "Pluto Inc.", "123 Letha Shore.",
                       "", "Dis", "CA", "91222", "US", "12345678910");
  std::vector<base::string16> labels;
  // Two fields at least - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 2, "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);

  // Three fields at least - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 3, "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., Elysium"),
            labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore., Dis"),
            labels[1]);

  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_CITY);
  suggested_fields.push_back(ADDRESS_HOME_STATE);
  suggested_fields.push_back(ADDRESS_HOME_ZIP);

  // Two fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 2,
                                        "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis 91222"), labels[1]);

  // Three fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 3,
                                        "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA 91222"), labels[1]);

  // Three fields at least, from suggested fields - but filter reduces available
  // fields to two.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, ADDRESS_HOME_ZIP, 3,
                                        "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA"), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for all NAME* fields...
  suggested_fields.push_back(NAME_MIDDLE);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe"), labels[1]);

  // One field at least, from suggested fields - filter the same as suggested
  // field.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_MIDDLE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(base::string16(), labels[0]);
  EXPECT_EQ(base::string16(), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for NAME_MIDDLE_INITIAL
  suggested_fields.push_back(NAME_MIDDLE_INITIAL);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe"), labels[1]);

  // One field at least, from suggested fields - filter same as the first non-
  // unknown suggested field.
  suggested_fields.clear();
  suggested_fields.push_back(UNKNOWN_TYPE);
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_FULL, 1,
                                        "en-US", &labels);
  EXPECT_EQ(base::string16(ASCIIToUTF16("666 Erebus St.")), labels[0]);
  EXPECT_EQ(base::string16(ASCIIToUTF16("123 Letha Shore.")), labels[1]);

  // No suggested fields, but non-unknown excluded field.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        NAME_FULL, 1, "en-US", &labels);
  EXPECT_EQ(base::string16(ASCIIToUTF16("666 Erebus St.")), labels[0]);
  EXPECT_EQ(base::string16(ASCIIToUTF16("123 Letha Shore.")), labels[1]);
}

// Test that we fall back to using the full name if there are no other
// distinguishing fields, but only if it makes sense given the suggested fields.
TEST(AutofillProfileTest, CreateInferredLabelsFallsBackToFullName) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[1].get(), "Johnny", "K", "Doe",
                       "doe@example.com", "", "88 Nowhere Ave.", "", "", "", "",
                       "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(NAME_LAST);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_LAST, 1,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[1]);

  // Otherwise, we should.
  suggested_fields.push_back(NAME_FIRST);
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_LAST, 1,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., Johnny K Doe"), labels[1]);
}

// Test that we do not show duplicate fields in the labels.
TEST(AutofillProfileTest, CreateInferredLabelsNoDuplicatedFields) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[1].get(), "John", "", "Doe", "dojo@example.com",
                       "", "88 Nowhere Ave.", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(ADDRESS_BILLING_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 2,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., doe@example.com"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., dojo@example.com"), labels[1]);
}

// Make sure that empty fields are not treated as distinguishing fields.
TEST(AutofillProfileTest, CreateInferredLabelsSkipsEmptyFields) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "Gogole", "", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[1].get(), "John", "", "Doe", "doe@example.com",
                       "Ggoole", "", "", "", "", "", "", "");
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[2].get(), "John", "", "Doe",
                       "john.doe@example.com", "Goolge", "", "", "", "", "", "",
                       "");

  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 3, "en-US", &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Ggoole"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com, Goolge"), labels[2]);

  // A field must have a non-empty value for each profile to be considered a
  // distinguishing field.
  profiles[1]->SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("88 Nowhere Ave."));
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 1, "en-US", &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 88 Nowhere Ave., doe@example.com, Ggoole"),
            labels[1]) << labels[1];
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com"), labels[2]);
}

// Test that labels that would otherwise have multiline values are flattened.
TEST(AutofillProfileTest, CreateInferredLabelsFlattensMultiLineValues) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  profiles.push_back(std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                                       test::kEmptyOrigin));
  test::SetProfileInfo(profiles[0].get(), "John", "", "Doe", "doe@example.com",
                       "", "88 Nowhere Ave.", "Apt. 42", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_STREET_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_FULL, 1,
                                        "en-US", &labels);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., Apt. 42"), labels[0]);
}

TEST(AutofillProfileTest, IsSubsetOf) {
  std::unique_ptr<AutofillProfile> a, b;

  // |a| is a subset of |b|.
  a.reset(new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin));
  b.reset(new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin));
  test::SetProfileInfo(a.get(), "Thomas", nullptr, "Jefferson",
                       "declaration_guy@gmail.com", nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr);
  test::SetProfileInfo(b.get(), "Thomas", nullptr, "Jefferson",
                       "declaration_guy@gmail.com", "United States Government",
                       "Monticello", nullptr, "Charlottesville", "Virginia",
                       "22902", nullptr, nullptr);
  EXPECT_TRUE(a->IsSubsetOf(*b, "en-US"));

  // |b| is not a subset of |a|.
  EXPECT_FALSE(b->IsSubsetOf(*a, "en-US"));

  // |a| is a subset of |a|.
  EXPECT_TRUE(a->IsSubsetOf(*a, "en-US"));

  // One field in |b| is different.
  a.reset(new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin));
  b.reset(new AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin));
  test::SetProfileInfo(a.get(), "Thomas", nullptr, "Jefferson",
                       "declaration_guy@gmail.com", nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr);
  test::SetProfileInfo(a.get(), "Thomas", nullptr, "Adams",
                       "declaration_guy@gmail.com", nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_FALSE(a->IsSubsetOf(*b, "en-US"));
}

TEST(AutofillProfileTest, SetRawInfo_UpdateValidityFlag) {
  AutofillProfile a;
  SetupValidatedTestProfile(a);
  EXPECT_TRUE(a.is_client_validity_states_updated());

  a.SetRawInfo(NAME_FULL, ASCIIToUTF16("Alice Munro"));
  // NAME_FULL is NOT validated through the client API (not supported),
  // therefore it should not change the validity flag.
  EXPECT_TRUE(a.is_client_validity_states_updated());

  a.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Ooz"));
  // ADDRESS_HOME_CITY IS validated through the client API, therefore it should
  // change the flag to false.
  EXPECT_FALSE(a.is_client_validity_states_updated());
}

TEST(AutofillProfileTest, MergeDataFrom_DifferentProfile) {
  AutofillProfile a;
  SetupValidatedTestProfile(a);

  // Create an identical profile except that the new profile:
  //   (1) Has a different origin,
  //   (2) Has a different address line 2,
  //   (3) Lacks a company name,
  //   (4) Has a different full name, and
  //   (5) Has a language code.
  AutofillProfile b = a;
  b.set_guid(base::GenerateGUID());
  b.set_origin(kSettingsOrigin);
  b.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Unit 5, area 51"));
  b.SetRawInfo(COMPANY_NAME, base::string16());

  b.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("M."));
  b.SetRawInfo(NAME_FULL, ASCIIToUTF16("Marion M. Morrison"));
  b.set_language_code("en");

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_FALSE(a.is_client_validity_states_updated());
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ(ASCIIToUTF16("Unit 5, area 51"), a.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Fox"), a.GetRawInfo(COMPANY_NAME));
  base::string16 name = a.GetInfo(NAME_FULL, "en-US");
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison"), name);
  EXPECT_EQ("en", a.language_code());
}

TEST(AutofillProfileTest, MergeDataFrom_SameProfile) {
  AutofillProfile a;
  SetupValidatedTestProfile(a);

  // The profile has no full name yet. Merge will add it.
  AutofillProfile b = a;
  b.set_guid(base::GenerateGUID());
  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_FALSE(a.is_client_validity_states_updated());
  EXPECT_EQ(1u, a.use_count());

  // pretend that the profile is re-validated.
  a.set_is_client_validity_states_updated(true);

  // Now the profile is fully populated. Merging it again has no effect (except
  // for usage statistics).
  AutofillProfile c = a;
  c.set_guid(base::GenerateGUID());
  c.set_use_count(3);
  EXPECT_FALSE(a.MergeDataFrom(c, "en-US"));
  // Merge has not modified anything, the validation should not changed.
  EXPECT_TRUE(a.is_client_validity_states_updated());
  EXPECT_EQ(3u, a.use_count());
}

TEST(AutofillProfileTest, OverwriteName_AddNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("Marion Mitchell Morrison"));

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::ASCIIToUTF16("Marion Mitchell Morrison"),
            a.GetRawInfo(NAME_FULL));
}

// Tests that OverwriteName overwrites the name parts if they have different
// case.
TEST(AutofillProfileTest, OverwriteName_DifferentCase) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  b.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  b.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
}

TEST(AutofillProfileTest, AssignmentOperator) {
  AutofillProfile a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");

  // Result of assignment should be logically equal to the original profile.
  AutofillProfile b(base::GenerateGUID(), test::kEmptyOrigin);
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = *&a;  // The *& defeats Clang's -Wself-assign warning.
  EXPECT_TRUE(a == b);
}

TEST(AutofillProfileTest, Copy) {
  AutofillProfile a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");

  // Clone should be logically equal to the original.
  AutofillProfile b(a);
  EXPECT_TRUE(a == b);
}

TEST(AutofillProfileTest, Compare) {
  AutofillProfile a(base::GenerateGUID(), std::string());
  AutofillProfile b(base::GenerateGUID(), std::string());

  // Empty profiles are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(base::GenerateGUID());
  b.set_guid(base::GenerateGUID());
  EXPECT_EQ(0, a.Compare(b));

  // Origins don't count.
  a.set_origin("apple");
  b.set_origin("banana");
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
  a.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
               ASCIIToUTF16("line one\nline two"));
  test::SetProfileInfo(&b, "John", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  b.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
               ASCIIToUTF16("line one\nline two\nline three"));
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

TEST(AutofillProfileTest, IsPresentButInvalid) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90"));
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90210"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("310"));
  EXPECT_TRUE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("(310) 310-6000"));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));
}

TEST(AutofillProfileTest, SetRawInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     ASCIIToUTF16("123 Super St.\n"
                                  "Apt. #42"));
  EXPECT_EQ(ASCIIToUTF16("123 Super St.\n"
                         "Apt. #42"),
            profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                  ASCIIToUTF16("123 Super St.\n"
                               "Apt. #42"),
                  "en-US");
  EXPECT_EQ(ASCIIToUTF16("123 Super St.\n"
                         "Apt. #42"),
            profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetRawInfoDoesntTrimWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("\tuser@example.com    "));
  EXPECT_EQ(ASCIIToUTF16("\tuser@example.com    "),
            profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoTrimsWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("\tuser@example.com    "),
                  "en-US");
  EXPECT_EQ(ASCIIToUTF16("user@example.com"),
            profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, FullAddress) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");

  AutofillType full_address(HTML_TYPE_FULL_ADDRESS, HTML_MODE_NONE);
  base::string16 formatted_address(ASCIIToUTF16(
      "Marion Mitchell Morrison\n"
      "Fox\n"
      "123 Zoo St.\n"
      "unit 5\n"
      "Hollywood, CA 91601"));
  EXPECT_EQ(formatted_address, profile.GetInfo(full_address, "en-US"));
  // This should fail and leave the profile unchanged.
  EXPECT_FALSE(profile.SetInfo(full_address, ASCIIToUTF16("foobar"), "en-US"));
  EXPECT_EQ(formatted_address, profile.GetInfo(full_address, "en-US"));

  // Some things can be missing...
  profile.SetInfo(ADDRESS_HOME_LINE2, base::string16(), "en-US");
  profile.SetInfo(EMAIL_ADDRESS, base::string16(), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison\n"
                         "Fox\n"
                         "123 Zoo St.\n"
                         "Hollywood, CA 91601"),
            profile.GetInfo(full_address, "en-US"));

  // ...but nothing comes out if a required field is missing.
  profile.SetInfo(ADDRESS_HOME_STATE, base::string16(), "en-US");
  EXPECT_TRUE(profile.GetInfo(full_address, "en-US").empty());

  // Restore the state but remove country. This should also fail.
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"), "en-US");
  EXPECT_FALSE(profile.GetInfo(full_address, "en-US").empty());
  profile.SetInfo(ADDRESS_HOME_COUNTRY, base::string16(), "en-US");
  EXPECT_TRUE(profile.GetInfo(full_address, "en-US").empty());
}

TEST(AutofillProfileTest, SaveAdditionalInfo_Name_AddingNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("Marion Mitchell Morrison"));

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::ASCIIToUTF16("Marion Mitchell Morrison"),
            a.GetRawInfo(NAME_FULL));
}

TEST(AutofillProfileTest, SaveAdditionalInfo_Name_KeepNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));
  a.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("Marion Mitchell Morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FULL, base::ASCIIToUTF16(""));

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::ASCIIToUTF16("Marion Mitchell Morrison"),
            a.GetRawInfo(NAME_FULL));
}

// Tests the merging of two similar profiles results in the second profile's
// non-empty fields overwriting the initial profiles values.
TEST(AutofillProfileTest,
     SaveAdditionalInfo_Name_DifferentCaseAndDiacriticsNoNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("morrison"));
  a.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("marion mitchell morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FIRST, UTF8ToUTF16("Märion"));
  b.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16("Mitchéll"));
  b.SetRawInfo(NAME_LAST,UTF8ToUTF16("Morrison"));
  b.SetRawInfo(NAME_FULL, UTF8ToUTF16(""));

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  // The first, middle and last names should have their first letter in
  // uppercase and have acquired diacritics.
  EXPECT_EQ(UTF8ToUTF16("Märion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(UTF8ToUTF16("Mitchéll"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(UTF8ToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(UTF8ToUTF16("Märion Mitchéll Morrison"),
            a.GetRawInfo(NAME_FULL));
}

// Tests that no loss of information happens when SavingAdditionalInfo with a
// profile with an empty name part.
TEST(AutofillProfileTest, SaveAdditionalInfo_Name_LossOfInformation) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));

  AutofillProfile b = a;
  b.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16(""));

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
}

// Tests that merging two complementary profiles for names results in a profile
// with a complete name.
TEST(AutofillProfileTest, SaveAdditionalInfo_Name_ComplementaryInformation) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, base::ASCIIToUTF16("Marion"));
  a.SetRawInfo(NAME_MIDDLE, base::ASCIIToUTF16("Mitchell"));
  a.SetRawInfo(NAME_LAST, base::ASCIIToUTF16("Morrison"));

  AutofillProfile b;
  b.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("Marion Mitchell Morrison"));

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  // The first, middle and last names should be kept and name full should be
  // added.
  EXPECT_EQ(base::ASCIIToUTF16("Marion"), a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::ASCIIToUTF16("Mitchell"), a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::ASCIIToUTF16("Morrison"), a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::ASCIIToUTF16("Marion Mitchell Morrison"),
            a.GetRawInfo(NAME_FULL));
}

TEST(AutofillProfileTest, IsAnInvalidPhoneNumber) {
  {
    AutofillProfile profile;
    // When all fields are unvalidated, none of them is an invalid phone type.
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(NAME_FULL));

    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }

  {
    AutofillProfile profile;
    profile.SetValidityState(PHONE_HOME_CITY_AND_NUMBER,
                             AutofillProfile::INVALID, AutofillProfile::CLIENT);

    // It's based on the server side validation.
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(ADDRESS_HOME_LINE1));

    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }

  {
    AutofillProfile profile;
    profile.SetValidityState(PHONE_HOME_CITY_CODE, AutofillProfile::INVALID,
                             AutofillProfile::SERVER);
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(ADDRESS_HOME_LINE2));

    EXPECT_EQ(true, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(true, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }
  {
    AutofillProfile profile;
    profile.SetValidityState(PHONE_BILLING_COUNTRY_CODE,
                             AutofillProfile::INVALID, AutofillProfile::SERVER);
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(ADDRESS_HOME_LINE2));

    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(true, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }
  {
    AutofillProfile profile;
    profile.SetValidityState(PHONE_BILLING_NUMBER, AutofillProfile::EMPTY,
                             AutofillProfile::SERVER);
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_CITY_CODE));

    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }
  {
    AutofillProfile profile;
    profile.SetValidityState(PHONE_BILLING_WHOLE_NUMBER, AutofillProfile::VALID,
                             AutofillProfile::SERVER);
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_COUNTRY_CODE));

    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(false, profile.IsAnInvalidPhoneNumber(PHONE_BILLING_NUMBER));
    EXPECT_EQ(false,
              profile.IsAnInvalidPhoneNumber(PHONE_BILLING_WHOLE_NUMBER));
  }
}

TEST(AutofillProfileTest, ValidityStatesClients) {
  AutofillProfile profile;

  // The default validity state should be UNVALIDATED.
  EXPECT_EQ(
      AutofillProfile::UNVALIDATED,
      profile.GetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::CLIENT));

  // Make sure setting the validity state works.
  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, ValidityStatesServer) {
  AutofillProfile profile;

  // The default validity state should be UNVALIDATED.
  EXPECT_EQ(
      AutofillProfile::UNVALIDATED,
      profile.GetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::SERVER));

  // Make sure setting the validity state works.
  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::SERVER);
  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::SERVER);
  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::SERVER);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::SERVER));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::SERVER));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::SERVER));
}

TEST(AutofillProfileTest, ValidityStates_ClientUnsupportedTypes) {
  AutofillProfile profile;

  // The validity state of unsupported types should be UNSUPPORTED.
  EXPECT_EQ(
      AutofillProfile::UNSUPPORTED,
      profile.GetValidityState(ADDRESS_HOME_LINE1, AutofillProfile::CLIENT));

  // Make sure setting the validity state of an unsupported type does nothing.
  profile.SetValidityState(ADDRESS_HOME_LINE1, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_LINE2, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(PHONE_HOME_CITY_AND_NUMBER,
                           AutofillProfile::UNVALIDATED,
                           AutofillProfile::AutofillProfile::CLIENT);
  EXPECT_EQ(AutofillProfile::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_LINE1,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::UNSUPPORTED,
            profile.GetValidityState(ADDRESS_HOME_LINE2,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(PHONE_HOME_CITY_AND_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_Country) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b01
  EXPECT_EQ(1, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b10
  EXPECT_EQ(2, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b11
  EXPECT_EQ(3, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_State) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b0100
  EXPECT_EQ(4, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b1000
  EXPECT_EQ(8, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b1100
  EXPECT_EQ(12, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_Zip) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b010000
  EXPECT_EQ(16, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b100000
  EXPECT_EQ(32, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b110000
  EXPECT_EQ(48, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_City) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b01000000
  EXPECT_EQ(64, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b10000000
  EXPECT_EQ(128, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b11000000
  EXPECT_EQ(192, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_DependentLocality) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b0100000000
  EXPECT_EQ(256, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b1000000000
  EXPECT_EQ(512, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b1100000000
  EXPECT_EQ(768, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_Email) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(EMAIL_ADDRESS, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b010000000000
  EXPECT_EQ(1024, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(EMAIL_ADDRESS, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b100000000000
  EXPECT_EQ(2048, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(EMAIL_ADDRESS, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b110000000000
  EXPECT_EQ(3072, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_Phone) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b01000000000000
  EXPECT_EQ(4096, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b10000000000000
  EXPECT_EQ(8192, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b11000000000000
  EXPECT_EQ(12288, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, GetClientValidityBitfieldValue_Mixed) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::UNVALIDATED,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           AutofillProfile::UNVALIDATED,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(EMAIL_ADDRESS, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b01110011010010
  EXPECT_EQ(7378, profile.GetClientValidityBitfieldValue());

  profile.SetValidityState(ADDRESS_HOME_COUNTRY, AutofillProfile::EMPTY,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_STATE, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_ZIP, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_CITY, AutofillProfile::VALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                           AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(EMAIL_ADDRESS, AutofillProfile::UNVALIDATED,
                           AutofillProfile::AutofillProfile::CLIENT);
  profile.SetValidityState(PHONE_HOME_WHOLE_NUMBER, AutofillProfile::INVALID,
                           AutofillProfile::AutofillProfile::CLIENT);
  // 0b11001110101101
  EXPECT_EQ(13229, profile.GetClientValidityBitfieldValue());
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_Country) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b01
  profile.SetClientValidityFromBitfieldValue(1);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b10
  profile.SetClientValidityFromBitfieldValue(2);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b11
  profile.SetClientValidityFromBitfieldValue(3);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_State) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b0100
  profile.SetClientValidityFromBitfieldValue(4);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b1000
  profile.SetClientValidityFromBitfieldValue(8);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b1100
  profile.SetClientValidityFromBitfieldValue(12);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_Zip) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b010000
  profile.SetClientValidityFromBitfieldValue(16);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_ZIP,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b100000
  profile.SetClientValidityFromBitfieldValue(32);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_ZIP,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b110000
  profile.SetClientValidityFromBitfieldValue(48);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_ZIP,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_City) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b01000000
  profile.SetClientValidityFromBitfieldValue(64);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b10000000
  profile.SetClientValidityFromBitfieldValue(128);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b11000000
  profile.SetClientValidityFromBitfieldValue(192);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest,
     SetClientValidityFromBitfieldValue_DependentLocality) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b0100000000
  profile.SetClientValidityFromBitfieldValue(256);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b1000000000
  profile.SetClientValidityFromBitfieldValue(512);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b1100000000
  profile.SetClientValidityFromBitfieldValue(768);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_Email) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b010000000000
  profile.SetClientValidityFromBitfieldValue(1024);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(EMAIL_ADDRESS,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b100000000000
  profile.SetClientValidityFromBitfieldValue(2048);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(EMAIL_ADDRESS,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b110000000000
  profile.SetClientValidityFromBitfieldValue(3072);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_Phone) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b01000000000000
  profile.SetClientValidityFromBitfieldValue(4096);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b10000000000000
  profile.SetClientValidityFromBitfieldValue(8192);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b11000000000000
  profile.SetClientValidityFromBitfieldValue(12288);
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, SetClientValidityFromBitfieldValue_Mixed) {
  AutofillProfile profile;

  // By default all validity statuses should be set to UNVALIDATED, thus the
  // bitfield value should be empty.
  EXPECT_EQ(0, profile.GetClientValidityBitfieldValue());

  // 0b01110011010010
  profile.SetClientValidityFromBitfieldValue(7378);
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_ZIP,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(EMAIL_ADDRESS,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));

  // 0b11001110101101
  profile.SetClientValidityFromBitfieldValue(13229);
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_STATE,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_ZIP,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_CITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(EMAIL_ADDRESS,
                                     AutofillProfile::AutofillProfile::CLIENT));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER,
                                     AutofillProfile::AutofillProfile::CLIENT));
}

TEST(AutofillProfileTest, GetMetadata) {
  AutofillProfile local_profile = test::GetFullProfile();
  local_profile.set_use_count(2);
  local_profile.set_use_date(base::Time::FromDoubleT(25));
  local_profile.set_has_converted(false);
  AutofillMetadata local_metadata = local_profile.GetMetadata();
  EXPECT_EQ(local_profile.guid(), local_metadata.id);
  EXPECT_EQ(local_profile.has_converted(), local_metadata.has_converted);
  EXPECT_EQ(local_profile.use_count(), local_metadata.use_count);
  EXPECT_EQ(local_profile.use_date(), local_metadata.use_date);

  AutofillProfile server_profile = test::GetServerProfile();
  server_profile.set_use_count(10);
  server_profile.set_use_date(base::Time::FromDoubleT(100));
  server_profile.set_has_converted(true);
  AutofillMetadata server_metadata = server_profile.GetMetadata();
  EXPECT_EQ(server_profile.server_id(), server_metadata.id);
  EXPECT_EQ(server_profile.has_converted(), server_metadata.has_converted);
  EXPECT_EQ(server_profile.use_count(), server_metadata.use_count);
  EXPECT_EQ(server_profile.use_date(), server_metadata.use_date);
}

TEST(AutofillProfileTest, SetMetadata_MatchingId) {
  AutofillProfile local_profile = test::GetFullProfile();
  AutofillMetadata local_metadata;
  local_metadata.id = local_profile.guid();
  local_metadata.use_count = 100;
  local_metadata.use_date = base::Time::FromDoubleT(50);
  local_metadata.has_converted = true;
  EXPECT_TRUE(local_profile.SetMetadata(local_metadata));
  EXPECT_EQ(local_metadata.id, local_profile.guid());
  EXPECT_EQ(local_metadata.has_converted, local_profile.has_converted());
  EXPECT_EQ(local_metadata.use_count, local_profile.use_count());
  EXPECT_EQ(local_metadata.use_date, local_profile.use_date());

  AutofillProfile server_profile = test::GetServerProfile();
  AutofillMetadata server_metadata;
  server_metadata.id = server_profile.server_id();
  server_metadata.use_count = 100;
  server_metadata.use_date = base::Time::FromDoubleT(50);
  server_metadata.has_converted = true;
  EXPECT_TRUE(server_profile.SetMetadata(server_metadata));
  EXPECT_EQ(server_metadata.id, server_profile.server_id());
  EXPECT_EQ(server_metadata.has_converted, server_profile.has_converted());
  EXPECT_EQ(server_metadata.use_count, server_profile.use_count());
  EXPECT_EQ(server_metadata.use_date, server_profile.use_date());
}

TEST(AutofillProfileTest, SetMetadata_NotMatchingId) {
  AutofillProfile local_profile = test::GetFullProfile();
  AutofillMetadata local_metadata;
  local_metadata.id = "WrongId";
  local_metadata.use_count = 100;
  local_metadata.use_date = base::Time::FromDoubleT(50);
  local_metadata.has_converted = true;
  EXPECT_FALSE(local_profile.SetMetadata(local_metadata));
  EXPECT_NE(local_metadata.id, local_profile.guid());
  EXPECT_NE(local_metadata.has_converted, local_profile.has_converted());
  EXPECT_NE(local_metadata.use_count, local_profile.use_count());
  EXPECT_NE(local_metadata.use_date, local_profile.use_date());

  AutofillProfile server_profile = test::GetServerProfile();
  AutofillMetadata server_metadata;
  server_metadata.id = "WrongId";
  server_metadata.use_count = 100;
  server_metadata.use_date = base::Time::FromDoubleT(50);
  server_metadata.has_converted = true;
  EXPECT_FALSE(server_profile.SetMetadata(server_metadata));
  EXPECT_NE(server_metadata.id, server_profile.guid());
  EXPECT_NE(server_metadata.has_converted, server_profile.has_converted());
  EXPECT_NE(server_metadata.use_count, server_profile.use_count());
  EXPECT_NE(server_metadata.use_date, server_profile.use_date());
}

}  // namespace autofill
