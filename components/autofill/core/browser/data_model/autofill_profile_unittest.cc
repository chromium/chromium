// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

namespace autofill {

using structured_address::VerificationStatus;
constexpr VerificationStatus kObserved = VerificationStatus::kObserved;

namespace {

std::u16string GetSuggestionLabel(AutofillProfile* profile) {
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(profile);
  std::vector<std::u16string> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, "en-US", &labels);
  return labels[0];
}

void SetupTestProfile(AutofillProfile& profile) {
  profile.set_guid(base::GenerateGUID());
  profile.set_origin(kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
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
  std::u16string summary0 = GetSuggestionLabel(&profile0);
  EXPECT_EQ(std::u16string(), summary0);

  // Case 0a/empty name and address, so the first two fields of the rest of the
  // data is used: "Hollywood, CA"
  AutofillProfile profile00(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile00, "", "", "", "johnwayne@me.xyz", "Fox", "",
                       "", "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary00 = GetSuggestionLabel(&profile00);
  EXPECT_EQ(u"Hollywood, CA", summary00);

  // Case 1: "<address>" without line 2.
  AutofillProfile profile1(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "", "", "", "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.", "", "Hollywood", "CA", "91601", "US",
                       "16505678910");
  std::u16string summary1 = GetSuggestionLabel(&profile1);
  EXPECT_EQ(u"123 Zoo St., Hollywood", summary1);

  // Case 1a: "<address>" with line 2.
  AutofillProfile profile1a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1a, "", "", "", "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601",
                       "US", "16505678910");
  std::u16string summary1a = GetSuggestionLabel(&profile1a);
  EXPECT_EQ(u"123 Zoo St., unit 5", summary1a);

  // Case 2: "<lastname>"
  AutofillProfile profile2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary2 = GetSuggestionLabel(&profile2);
  // Summary includes full name, to the maximal extent available.
  EXPECT_EQ(u"Mitchell Morrison, Hollywood", summary2);

  // Case 3: "<lastname>, <address>"
  AutofillProfile profile3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary3 = GetSuggestionLabel(&profile3);
  EXPECT_EQ(u"Mitchell Morrison, 123 Zoo St.", summary3);

  // Case 4: "<firstname>"
  AutofillProfile profile4(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Marion", "Mitchell", "", "johnwayne@me.xyz",
                       "Fox", "", "", "Hollywood", "CA", "91601", "US",
                       "16505678910");
  std::u16string summary4 = GetSuggestionLabel(&profile4);
  EXPECT_EQ(u"Marion Mitchell, Hollywood", summary4);

  // Case 5: "<firstname>, <address>"
  AutofillProfile profile5(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "Marion", "Mitchell", "", "johnwayne@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary5 = GetSuggestionLabel(&profile5);
  EXPECT_EQ(u"Marion Mitchell, 123 Zoo St.", summary5);

  // Case 6: "<firstname> <lastname>"
  AutofillProfile profile6(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
                       "91601", "US", "16505678910");
  std::u16string summary6 = GetSuggestionLabel(&profile6);
  EXPECT_EQ(u"Marion Mitchell Morrison, Hollywood", summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutofillProfile profile7(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile7, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::u16string summary7 = GetSuggestionLabel(&profile7);
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St.", summary7);

  // Case 7a: "<firstname> <lastname>, <address>" - same as #7, except for
  // e-mail.
  AutofillProfile profile7a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile7a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "16505678910");
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile7);
  profiles.push_back(&profile7a);
  std::vector<std::u16string> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, "en-US", &labels);
  ASSERT_EQ(profiles.size(), labels.size());
  summary7 = labels[0];
  std::u16string summary7a = labels[1];
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St., johnwayne@me.xyz",
            summary7);
  EXPECT_EQ(u"Marion Mitchell Morrison, 123 Zoo St., marion@me.xyz", summary7a);
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
  std::vector<std::u16string> labels;
  AutofillProfile::CreateDifferentiatingLabels(ToRawPointerVector(profiles),
                                               "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"John Doe, 666 Erebus St.", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);

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
  EXPECT_EQ(u"John Doe, 666 Erebus St., johndoe@hades.com", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., johndoe@tertium.com", labels[2]);

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
  EXPECT_EQ(u"John Doe, 666 Erebus St., CA", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO", labels[2]);

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
  EXPECT_EQ(u"John Doe, 666 Erebus St., CA", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, 16502111111", labels[2]);
  // This one differs from other ones by unique phone, so no need for extra
  // information.
  EXPECT_EQ(u"John Doe, 666 Erebus St., CO, 16504444444", labels[3]);

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

  std::vector<std::u16string> labels;
  for (size_t i = 0; i < std::size(kExpectedLabels); ++i) {
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
  static const char* kExpectedLabels[] = {
      "",
      "Antoine de Saint-Exupéry",
      "Antoine de Saint-Exupéry, 8 Rue de Londres",
      "Antoine de Saint-Exupéry, 8 Rue de Londres, Paris",
      "Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
      "France",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
      "France, antoine@exemple.com",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
      "France, antoine@exemple.com, +33 (0) 1 42 68 53 00",
      "Exemple Inc, Antoine de Saint-Exupéry, 8 Rue de Londres, 75009 Paris, "
      "France, antoine@exemple.com, +33 (0) 1 42 68 53 00",
  };

  std::vector<std::u16string> labels;
  for (size_t i = 0; i < std::size(kExpectedLabels); ++i) {
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
  profiles.back()->SetInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Yeoksam-Dong",
                           "en-US");
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

  std::vector<std::u16string> labels;
  for (size_t i = 0; i < std::size(kExpectedLabels); ++i) {
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

  std::vector<std::u16string> labels;
  for (size_t i = 0; i < std::size(kExpectedLabels); ++i) {
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

  std::vector<std::u16string> labels;
  for (size_t i = 0; i < std::size(kExpectedLabels); ++i) {
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
  std::vector<std::u16string> labels;
  // Two fields at least - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 2, "en-US", &labels);
  EXPECT_EQ(u"John Doe, 666 Erebus St.", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore.", labels[1]);

  // Three fields at least - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 3, "en-US", &labels);
  EXPECT_EQ(u"John Doe, 666 Erebus St., Elysium", labels[0]);
  EXPECT_EQ(u"Jane Doe, 123 Letha Shore., Dis", labels[1]);

  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_CITY);
  suggested_fields.push_back(ADDRESS_HOME_STATE);
  suggested_fields.push_back(ADDRESS_HOME_ZIP);

  // Two fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 2,
                                        "en-US", &labels);
  EXPECT_EQ(u"Elysium 91111", labels[0]);
  EXPECT_EQ(u"Dis 91222", labels[1]);

  // Three fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 3,
                                        "en-US", &labels);
  EXPECT_EQ(u"Elysium, CA 91111", labels[0]);
  EXPECT_EQ(u"Dis, CA 91222", labels[1]);

  // Three fields at least, from suggested fields - but filter reduces available
  // fields to two.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, ADDRESS_HOME_ZIP, 3,
                                        "en-US", &labels);
  EXPECT_EQ(u"Elysium, CA", labels[0]);
  EXPECT_EQ(u"Dis, CA", labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for all NAME* fields...
  suggested_fields.push_back(NAME_MIDDLE);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(u"John Doe", labels[0]);
  EXPECT_EQ(u"Jane Doe", labels[1]);

  // One field at least, from suggested fields - filter the same as suggested
  // field.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_MIDDLE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(std::u16string(), labels[0]);
  EXPECT_EQ(std::u16string(), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for NAME_MIDDLE_INITIAL
  suggested_fields.push_back(NAME_MIDDLE_INITIAL);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 1,
                                        "en-US", &labels);
  EXPECT_EQ(u"John Doe", labels[0]);
  EXPECT_EQ(u"Jane Doe", labels[1]);

  // One field at least, from suggested fields - filter same as the first non-
  // unknown suggested field.
  suggested_fields.clear();
  suggested_fields.push_back(UNKNOWN_TYPE);
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_FULL, 1,
                                        "en-US", &labels);
  EXPECT_EQ(std::u16string(u"666 Erebus St."), labels[0]);
  EXPECT_EQ(std::u16string(u"123 Letha Shore."), labels[1]);

  // No suggested fields, but non-unknown excluded field.
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        NAME_FULL, 1, "en-US", &labels);
  EXPECT_EQ(std::u16string(u"666 Erebus St."), labels[0]);
  EXPECT_EQ(std::u16string(u"123 Letha Shore."), labels[1]);
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
  std::vector<std::u16string> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_LAST, 1,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave.", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave.", labels[1]);

  // Otherwise, we should.
  suggested_fields.push_back(NAME_FIRST);
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_LAST, 1,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., John Doe", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave., Johnny K Doe", labels[1]);
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
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<std::u16string> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, UNKNOWN_TYPE, 2,
                                        "en-US", &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., doe@example.com", labels[0]);
  EXPECT_EQ(u"88 Nowhere Ave., dojo@example.com", labels[1]);
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

  std::vector<std::u16string> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 3, "en-US", &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, doe@example.com, Gogole", labels[0]);
  EXPECT_EQ(u"John Doe, doe@example.com, Ggoole", labels[1]);
  EXPECT_EQ(u"John Doe, john.doe@example.com, Goolge", labels[2]);

  // A field must have a non-empty value for each profile to be considered a
  // distinguishing field.
  profiles[1]->SetRawInfo(ADDRESS_HOME_LINE1, u"88 Nowhere Ave.");
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles), nullptr,
                                        UNKNOWN_TYPE, 1, "en-US", &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(u"John Doe, doe@example.com, Gogole", labels[0]);
  EXPECT_EQ(u"John Doe, 88 Nowhere Ave., doe@example.com, Ggoole", labels[1])
      << labels[1];
  EXPECT_EQ(u"John Doe, john.doe@example.com", labels[2]);
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
  std::vector<std::u16string> labels;
  AutofillProfile::CreateInferredLabels(ToRawPointerVector(profiles),
                                        &suggested_fields, NAME_FULL, 1,
                                        "en-US", &labels);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(u"88 Nowhere Ave., Apt. 42", labels[0]);
}

TEST(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentMiddleNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "M", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Genevieve", "Marie", "Fox", "", "", "", "",
                       "", "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  // When a form has a NAME_FULL field rather than a NAME_MIDDLE field, consider
  // whether one profile's full name can be derived from the other's.
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                             {NAME_FULL}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_FULL}));
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile3, "en-US",
                                             {NAME_FULL}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_FULL}));
  // True because Genevieve M Fox can be derived from Genevieve Marie Fox.
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(comparator, profile3, "en-US",
                                             {NAME_FULL}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_FULL}));

  // When a form has a NAME_MIDDLE field rather than a NAME_FULL field, consider
  // a name's constituent parts.
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                             {NAME_MIDDLE}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_MIDDLE}));
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile3, "en-US",
                                             {NAME_MIDDLE}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_MIDDLE}));
  // False because the middle name M doesn't equal the middle name Marie.
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile3, "en-US",
                                              {NAME_MIDDLE}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_MIDDLE}));
}

TEST(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentFirstNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Cynthia", "", "Fox", "", "", "", "", "", "",
                       "", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_FULL}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_FULL}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_FIRST}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_FIRST}));
}

TEST(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentLastNames) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fuller", "", "", "", "", "",
                       "", "", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_FULL}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_FULL}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-US",
                                              {NAME_LAST}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-US",
                                              {NAME_LAST}));
}

TEST(AutofillProfileTest,
     IsSubsetOfForFieldSet_DifferentStreetAddressesIgnored) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "274 Main St",
                       "", "", "", "", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "",
                       "274 Main Street", "", "", "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US", {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US", {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS}));
}

TEST(AutofillProfileTest, IsSubsetOfForFieldSet_DifferentNonStreetAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "274 Main St",
                       "", "Northhampton", "", "", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "274 Main St",
                       "", "Sturbridge", "", "", "US", "");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US",
      {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US",
      {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY}));
}

TEST(AutofillProfileTest,
     IsSubsetOfForFieldSet_PostalCodesWithAndWithoutSpaces) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "H3B 2Y5", "CA", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "H3B2Y5", "CA", "");

  const AutofillProfileComparator comparator("en-CA");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(comparator, profile2, "en-CA",
                                             {NAME_FULL, ADDRESS_HOME_ZIP}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(comparator, profile1, "en-CA",
                                             {NAME_FULL, ADDRESS_HOME_ZIP}));
}

TEST(AutofillProfileTest,
     IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutSpacesAndPunctuation) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "CA", "+1 (514) 444-5454");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "CA", "15144445454");

  const AutofillProfileComparator comparator("en-CA");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-CA", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-CA", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-CA", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-CA", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST(AutofillProfileTest,
     IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutCodes_US) {
  // Has country and city codes.
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "+1 (508) 444-5454");

  // Has a city code.
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "5084445454");

  // Has neither a country nor a city code.
  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Genevieve", "", "Fox", "", "", "", "", "",
                       "", "", "US", "4445454");

  const AutofillProfileComparator comparator("en-US");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, "en-US", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST(AutofillProfileTest,
     IsSubsetOfForFieldSet_PhoneNumbersWithAndWithoutCodes_BR) {
  // Has country and city codes.
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "5521987650000");

  // Has a city code.
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "21987650000");

  // Has neither a country nor a city code.
  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Thiago", "", "Avila", "", "", "", "", "", "",
                       "", "", "BR", "987650000");

  const AutofillProfileComparator comparator("pt-BR");

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, "pt-BR", {NAME_FULL, PHONE_HOME_WHOLE_NUMBER}));

  EXPECT_TRUE(profile1.IsSubsetOfForFieldSet(
      comparator, profile2, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_TRUE(profile2.IsSubsetOfForFieldSet(
      comparator, profile1, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile1.IsSubsetOfForFieldSet(
      comparator, profile3, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile1, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile2.IsSubsetOfForFieldSet(
      comparator, profile3, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
  EXPECT_FALSE(profile3.IsSubsetOfForFieldSet(
      comparator, profile2, "pt-BR", {NAME_FULL, PHONE_HOME_CITY_AND_NUMBER}));
}

TEST(AutofillProfileTest, TestFinalizeAfterImport) {
  // A profile with just a full name should be finalizeable.
  {
    AutofillProfile profile;
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
    AutofillProfile profile;
    profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Peter Pan",
                                             VerificationStatus::kObserved);
    profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Michael",
                                             VerificationStatus::kObserved);
    EXPECT_FALSE(profile.FinalizeAfterImport());
  }
}

TEST(AutofillProfileTest, SetAndGetRawInfoWithValidationStatus) {
  AutofillProfile profile;
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
  EXPECT_EQ(profile.GetVerificationStatusInt(NAME_FULL), 2);
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), u"full name");

  // Test the working of the wrapper to pass the value by int.
  profile.SetRawInfoWithVerificationStatusInt(NAME_FULL, u"full name", 2);
  EXPECT_EQ(profile.GetVerificationStatusInt(NAME_FULL), 2);
}

TEST(AutofillProfileTest, SetAndGetInfoWithValidationStatus) {
  AutofillProfile profile;
  // An unsupported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(UNKNOWN_TYPE),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(profile.GetVerificationStatus(PHONE_HOME_NUMBER),
            VerificationStatus::kNoStatus);

  // An unassigned supported type should return |kNoStatus|.
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kNoStatus);

  // Set a value with verification status and verify the results.
  profile.SetInfoWithVerificationStatus(AutofillType(NAME_FULL), u"full name",
                                        "en-US",
                                        VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), u"full name");

  // Settings an unknown type should result in false.
  EXPECT_FALSE(profile.SetInfoWithVerificationStatus(
      UNKNOWN_TYPE, u"DM", "en-US", VerificationStatus::kFormatted));

  // Set a value with verification status using and AutofillType and verify the
  // results.
  EXPECT_TRUE(profile.SetInfoWithVerificationStatus(
      AutofillType(NAME_MIDDLE_INITIAL), u"MK", "en-US",
      VerificationStatus::kFormatted));
  EXPECT_EQ(profile.GetVerificationStatus(NAME_MIDDLE_INITIAL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_MIDDLE_INITIAL), u"MK");

  // Set a value with verification status and verify the results.
  EXPECT_TRUE(profile.SetInfoWithVerificationStatus(
      AutofillType(NAME_MIDDLE_INITIAL), u"CS", "en-US",
      VerificationStatus::kFormatted));
  EXPECT_EQ(profile.GetVerificationStatus(NAME_MIDDLE_INITIAL),
            VerificationStatus::kFormatted);
  EXPECT_EQ(profile.GetRawInfo(NAME_MIDDLE_INITIAL), u"CS");
}

TEST(AutofillProfileTest, MergeDataFrom_DifferentProfile) {
  AutofillProfile a;
  SetupTestProfile(a);

  // Create an identical profile except that the new profile:
  //   (1) Has a different origin,
  //   (2) Has a different address line 2,
  //   (3) Lacks a company name,
  //   (4) Has a different full name, and
  //   (5) Has a language code.
  AutofillProfile b = a;
  b.set_guid(base::GenerateGUID());
  b.set_origin(kSettingsOrigin);
  b.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LINE2, u"Unit 5, area 51",
      structured_address::VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(
      COMPANY_NAME, std::u16string(),
      structured_address::VerificationStatus::kObserved);

  b.SetRawInfo(NAME_MIDDLE, u"M.");
  b.SetRawInfo(NAME_FULL, u"Marion M. Morrison");
  b.set_language_code("en");
  b.FinalizeAfterImport();
  a.FinalizeAfterImport();

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ("Unit 5, area 51",
            base::UTF16ToUTF8(a.GetRawInfo(ADDRESS_HOME_LINE2)));
  EXPECT_EQ(u"Fox", a.GetRawInfo(COMPANY_NAME));
  std::u16string name = a.GetInfo(NAME_FULL, "en-US");
  EXPECT_EQ(u"Marion Mitchell Morrison", name);
  EXPECT_EQ("en", a.language_code());
}

TEST(AutofillProfileTest, MergeDataFrom_SameProfile) {
  AutofillProfile a;
  SetupTestProfile(a);

  // The profile has no full name yet. Merge will add it.
  AutofillProfile b = a;
  // For the new structured profiles, the profile must be altered for the
  // merging to have an effect. The verification status of the full name is set
  // to user verified.
  b.SetRawInfoWithVerificationStatus(NAME_FULL, b.GetRawInfo(NAME_FULL),
                                     VerificationStatus::kUserVerified);
  b.set_guid(base::GenerateGUID());
  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  // Merge has modified profile a, the validation is not updated.
  EXPECT_EQ(1u, a.use_count());

  // Now the profile is fully populated. Merging it again has no effect (except
  // for usage statistics).
  AutofillProfile c = a;
  c.set_guid(base::GenerateGUID());
  c.set_use_count(3);
  EXPECT_FALSE(a.MergeDataFrom(c, "en-US"));
  // Merge has not modified anything.
  EXPECT_EQ(3u, a.use_count());
}

TEST(AutofillProfileTest, OverwriteName_AddNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");

  AutofillProfile b = a;
  a.FinalizeAfterImport();

  b.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"Marion Mitchell Morrison",
      structured_address::VerificationStatus::kUserVerified);
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.MergeDataFrom(b, "en-US"));
  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Marion Mitchell Morrison", a.GetRawInfo(NAME_FULL));
}

// Tests that OverwriteName overwrites the name parts if they have different
// case.
TEST(AutofillProfileTest, OverwriteName_DifferentCase) {
  AutofillProfile a;
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

TEST(AutofillProfileTest, AssignmentOperator) {
  AutofillProfile a(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison", "marion@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "12345678910");

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
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison", "marion@me.xyz",
                       "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
                       "91601", "US", "12345678910");

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
  a.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"line one\nline two");
  test::SetProfileInfo(&b, "John", nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  b.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"line one\nline two\nline three");
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

// For each structured profile tokens, test the comparison operator for both the
// value and the status.
TEST(AutofillProfileTest, Compare_StructuredTypes) {
  // Those types do store a verification status.
  ServerFieldTypeSet structured_types{
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
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_DEPENDENT_STREET_NAME,
      ADDRESS_HOME_PREMISE_NAME,
      ADDRESS_HOME_SUBPREMISE,
  };

  // Those values are legal for all tokens.
  const std::u16string value1 = u"DE";
  const std::u16string value2 = u"US";

  const VerificationStatus status1 = VerificationStatus::kObserved;
  const VerificationStatus status2 = VerificationStatus::kParsed;

  ASSERT_NE(value1, value2);
  ASSERT_NE(status1, status2);

  for (auto type : structured_types) {
    // Create two empty profiles to test the tokens individually.
    AutofillProfile profile1;
    AutofillProfile profile2;

    SCOPED_TRACE(testing::Message()
                 << "Testing the Compare method for the type: "
                 << AutofillType(type).ToString());

    SCOPED_TRACE(testing::Message()
                 << "Verify the corrext result for identical values");
    profile1.SetRawInfoWithVerificationStatus(type, value1, status1);
    profile2.SetRawInfoWithVerificationStatus(type, value1, status1);
    EXPECT_EQ(profile1.Compare(profile2), 0);

    SCOPED_TRACE(testing::Message() << "Verify the sensitivity to the value");
    profile2.SetRawInfoWithVerificationStatus(type, value2, status1);
    EXPECT_NE(profile1.Compare(profile2), 0);

    SCOPED_TRACE(testing::Message() << "Verify the sensitivity to the status");
    profile2.SetRawInfoWithVerificationStatus(type, value1, status2);
    EXPECT_NE(profile1.Compare(profile2), 0);
  }
}

TEST(AutofillProfileTest, IsPresentButInvalid) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

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

TEST(AutofillProfileTest, SetRawInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     u"123 Super St.\n"
                     u"Apt. #42");
  EXPECT_EQ(
      u"123 Super St.\n"
      u"Apt. #42",
      profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetInfo(ADDRESS_HOME_STREET_ADDRESS,
                  u"123 Super St.\n"
                  u"Apt. #42",
                  "en-US");
  EXPECT_EQ(
      u"123 Super St.\n"
      u"Apt. #42",
      profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetRawInfoDoesntTrimWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetRawInfo(EMAIL_ADDRESS, u"\tuser@example.com    ");
  EXPECT_EQ(u"\tuser@example.com    ", profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoTrimsWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);

  profile.SetInfo(EMAIL_ADDRESS, u"\tuser@example.com    ", "en-US");
  EXPECT_EQ(u"user@example.com", profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, FullAddress) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  AutofillType full_address(HtmlFieldType::kFullAddress, HtmlFieldMode::kNone);
  std::u16string formatted_address(
      u"Marion Mitchell Morrison\n"
      u"Fox\n"
      u"123 Zoo St.\n"
      u"unit 5\n"
      u"Hollywood, CA 91601");
  EXPECT_EQ(formatted_address, profile.GetInfo(full_address, "en-US"));
  // This should fail and leave the profile unchanged.
  EXPECT_FALSE(profile.SetInfo(full_address, u"foobar", "en-US"));
  EXPECT_EQ(formatted_address, profile.GetInfo(full_address, "en-US"));

  // Some things can be missing...
  profile.SetInfo(ADDRESS_HOME_LINE2, std::u16string(), "en-US");
  profile.SetInfo(EMAIL_ADDRESS, std::u16string(), "en-US");
  EXPECT_EQ(
      u"Marion Mitchell Morrison\n"
      u"Fox\n"
      u"123 Zoo St.\n"
      u"Hollywood, CA 91601",
      profile.GetInfo(full_address, "en-US"));

  // ...but nothing comes out if a required field is missing.
  profile.SetInfo(ADDRESS_HOME_STATE, std::u16string(), "en-US");
  EXPECT_TRUE(profile.GetInfo(full_address, "en-US").empty());

  // Restore the state but remove country. This should also fail.
  profile.SetInfo(ADDRESS_HOME_STATE, u"CA", "en-US");
  EXPECT_FALSE(profile.GetInfo(full_address, "en-US").empty());
  profile.SetInfo(ADDRESS_HOME_COUNTRY, std::u16string(), "en-US");
  EXPECT_TRUE(profile.GetInfo(full_address, "en-US").empty());
}

TEST(AutofillProfileTest, SaveAdditionalInfo_Verified_MergeStructure) {
  AutofillProfile a;
  a.SetRawInfoWithVerificationStatus(NAME_FULL, u"Marion Mitchell Morrison",
                                     VerificationStatus::kUserVerified);
  a.FinalizeAfterImport();
  ASSERT_FALSE(a.IsVerified());
  a.set_origin(autofill::kSettingsOrigin);
  ASSERT_TRUE(a.IsVerified());

  EXPECT_EQ(a.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(a.GetVerificationStatus(NAME_FIRST), VerificationStatus::kParsed);
  EXPECT_EQ(a.GetVerificationStatus(NAME_MIDDLE), VerificationStatus::kParsed);
  EXPECT_EQ(a.GetVerificationStatus(NAME_LAST), VerificationStatus::kParsed);
  EXPECT_EQ(a.GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(a.GetRawInfo(NAME_MIDDLE), u"Mitchell");
  EXPECT_EQ(a.GetRawInfo(NAME_LAST), u"Morrison");

  AutofillProfile b;
  b.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Mitchell",
                                     VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Marion",
                                     VerificationStatus::kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison",
                                     VerificationStatus::kObserved);
  b.FinalizeAfterImport();
  ASSERT_FALSE(b.IsVerified());

  a.SaveAdditionalInfo(b, "en-US");

  // After merging, the full name is presvered, but the substructure changed.
  EXPECT_EQ(a.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(a.GetVerificationStatus(NAME_FIRST), VerificationStatus::kObserved);
  EXPECT_EQ(a.GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(a.GetVerificationStatus(NAME_LAST), VerificationStatus::kObserved);
  EXPECT_EQ(a.GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(a.GetRawInfo(NAME_FIRST), u"Mitchell");
  EXPECT_EQ(a.GetRawInfo(NAME_MIDDLE), u"Marion");
  EXPECT_EQ(a.GetRawInfo(NAME_LAST), u"Morrison");
}

TEST(AutofillProfileTest, SaveAdditionalInfo_Name_AddingNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");
  a.FinalizeAfterImport();

  AutofillProfile b = a;

  b.SetRawInfo(NAME_FULL, u"Marion Mitchell Morrison");
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Marion Mitchell Morrison", a.GetRawInfo(NAME_FULL));
}

TEST(AutofillProfileTest, SaveAdditionalInfo_Name_KeepNameFull) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");
  a.SetRawInfo(NAME_FULL, u"Marion Mitchell Morrison");

  AutofillProfile b = a;
  b.SetRawInfo(NAME_FULL, u"");

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Marion Mitchell Morrison", a.GetRawInfo(NAME_FULL));
}

// Tests the merging of two similar profiles results in the second profile's
// non-empty fields overwriting the initial profiles values.
TEST(AutofillProfileTest,
     SaveAdditionalInfo_Name_DifferentCaseAndDiacriticsNoNameFull) {
  AutofillProfile a;

  a.SetRawInfoWithVerificationStatus(NAME_FIRST, u"marion", kObserved);
  a.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"mitchell", kObserved);
  a.SetRawInfoWithVerificationStatus(NAME_LAST, u"morrison", kObserved);
  a.SetRawInfoWithVerificationStatus(NAME_FULL, u"marion mitchell morrison",
                                     kObserved);

  AutofillProfile b = a;
  a.FinalizeAfterImport();

  b.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Märion", kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Mitchéll", kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison", kObserved);
  b.SetRawInfoWithVerificationStatus(NAME_FULL, u"", kObserved);
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  // The first, middle and last names should have their first letter in
  // uppercase and have acquired diacritics.
  EXPECT_EQ(u"Märion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchéll", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  // In the merging logic the observed lower-case value should remain
  // because the upper-case-diacritic version is only formatted.
  EXPECT_EQ(u"marion mitchell morrison", a.GetRawInfo(NAME_FULL));
}

// Tests that no loss of information happens when SavingAdditionalInfo with a
// profile with an empty name part.
TEST(AutofillProfileTest, SaveAdditionalInfo_Name_LossOfInformation) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");
  a.FinalizeAfterImport();
  AutofillProfile b = a;
  b.SetRawInfo(NAME_MIDDLE, u"");

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
}

// Tests that merging two complementary profiles for names results in a profile
// with a complete name.
TEST(AutofillProfileTest, SaveAdditionalInfo_Name_ComplementaryInformation) {
  AutofillProfile a;

  a.SetRawInfo(NAME_FIRST, u"Marion");
  a.SetRawInfo(NAME_MIDDLE, u"Mitchell");
  a.SetRawInfo(NAME_LAST, u"Morrison");
  a.FinalizeAfterImport();
  AutofillProfile b;

  b.SetRawInfo(NAME_FULL, u"Marion Mitchell Morrison");
  b.FinalizeAfterImport();

  EXPECT_TRUE(a.SaveAdditionalInfo(b, "en-US"));

  // The first, middle and last names should be kept and name full should be
  // added.
  EXPECT_EQ(u"Marion", a.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Mitchell", a.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(u"Morrison", a.GetRawInfo(NAME_LAST));
  EXPECT_EQ(u"Marion Mitchell Morrison", a.GetRawInfo(NAME_FULL));
}

// Test that the label is correctly set and retrieved from the profile.
TEST(AutofillProfileTest, SetAndGetProfileLabels) {
  AutofillProfile p;
  EXPECT_EQ(p.profile_label(), std::string());

  p.set_profile_label("my label");
  EXPECT_EQ(p.profile_label(), "my label");
}

TEST(AutofillProfileTest, LabelsInAssignmentAndComparisonOperator) {
  AutofillProfile p1;
  p1.set_profile_label("my label");

  AutofillProfile p2;
  p2 = p1;

  // Check that the label was assigned correctly to p2.
  EXPECT_EQ(p2.profile_label(), "my label");

  // Now test that the comparison returns false if the label is not the same.
  ASSERT_EQ(p1, p2);
  p2.set_profile_label("another label");
  EXPECT_NE(p1, p2);
}

// Test that the state to disallow confirmable merges is correctly set and
// retrieved from the profile.
TEST(AutofillProfileTest, SetAndGetProfileDisallowConfirmableMergestate) {
  AutofillProfile p;
  EXPECT_EQ(p.disallow_settings_visible_updates(), false);

  p.set_disallow_settings_visible_updates(true);
  EXPECT_EQ(p.disallow_settings_visible_updates(), true);
}

TEST(AutofillProfileTest, LockStateInAssignmentAndComparisonOperator) {
  AutofillProfile p1;
  p1.set_disallow_settings_visible_updates(true);

  AutofillProfile p2;
  EXPECT_EQ(p2.disallow_settings_visible_updates(), false);

  p2 = p1;

  // Check that the lock state was assigned correctly to p2.
  EXPECT_EQ(p2.disallow_settings_visible_updates(), true);

  // Now test that the comparison returns false if the lock state is not the
  // same.
  ASSERT_EQ(p1, p2);
  p2.set_disallow_settings_visible_updates(false);
  EXPECT_NE(p1, p2);
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

// Tests that the profile is only deletable if it is not verified.
TEST(AutofillProfileTest, IsDeletable) {
  // Set up an arbitrary time, as setup the current time to just above the
  // threshold later than that time.
  const base::Time kArbitraryTime = base::Time::FromDoubleT(25000000000);
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime + kDisusedDataModelDeletionTimeDelta +
                    base::Days(1));

  // Created a profile that has not been used since over the deletion threshold.
  AutofillProfile profile = test::GetFullProfile();
  profile.set_use_date(kArbitraryTime);

  // Make sure it's deletable.
  EXPECT_TRUE(profile.IsDeletable());

  // Set the profile as being verified.
  profile.set_origin("Not empty");
  ASSERT_TRUE(profile.IsVerified());

  // Make sure it's not deletable.
  EXPECT_FALSE(profile.IsDeletable());
}

// Tests that the |HasStructuredData| returns whether the profile has structured
// data or not.
TEST(AutofillProfileTest, HasStructuredData) {
  AutofillProfile profile;
  profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"marion mitchell morrison", kObserved);
  EXPECT_FALSE(profile.HasStructuredData());

  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"marion", kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"mitchell", kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"morrison", kObserved);
  EXPECT_TRUE(profile.HasStructuredData());
}

TEST(AutofillProfileTest, RemoveInaccessibleProfileValues) {
  // Returns true if at least one field was removed.
  auto RemoveInaccessibleProfileValues = [](AutofillProfile& profile) {
    const ServerFieldTypeSet inaccessible_fields =
        profile.FindInaccessibleProfileValues();
    profile.ClearFields(inaccessible_fields);
    return !inaccessible_fields.empty();
  };

  AutofillProfile actual_profile;
  actual_profile.SetRawInfo(NAME_FIRST, u"Florian");

  // State is uncommon in Germany and inaccessible in the settings. Expect it
  // to be removed.
  actual_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  AutofillProfile expected_profile = actual_profile;
  actual_profile.SetRawInfo(ADDRESS_HOME_STATE, u"Bayern");
  EXPECT_TRUE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);

  // There are no ZIP codes in Angola.
  actual_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"AO");
  expected_profile = actual_profile;
  actual_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
  EXPECT_TRUE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);

  // If no country is set, the US requirements are used.
  // The US uses both ZIP codes and states.
  actual_profile.ClearFields({ADDRESS_HOME_COUNTRY});
  actual_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  actual_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
  expected_profile = actual_profile;
  EXPECT_FALSE(RemoveInaccessibleProfileValues(actual_profile));
  EXPECT_EQ(actual_profile.Compare(expected_profile), 0);
}

TEST(AutofillProfileTest, GetNonEmptyRawTypes) {
  AutofillProfile profile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Marion", nullptr, "Morrison",
                       "johnwayne@me.xyz", nullptr, "123 Zoo St.", nullptr,
                       "Hollywood", "CA", "91601", "US", "14155678910");

  std::vector<ServerFieldType> expected_raw_types{NAME_FIRST,
                                                  NAME_LAST,
                                                  NAME_FULL,
                                                  EMAIL_ADDRESS,
                                                  PHONE_HOME_WHOLE_NUMBER,
                                                  ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_CITY,
                                                  ADDRESS_HOME_STATE,
                                                  ADDRESS_HOME_ZIP,
                                                  ADDRESS_HOME_COUNTRY,
                                                  ADDRESS_HOME_STREET_ADDRESS,
                                                  ADDRESS_HOME_STREET_NAME,
                                                  ADDRESS_HOME_HOUSE_NUMBER,
                                                  NAME_LAST_SECOND};

  ServerFieldTypeSet non_empty_raw_types;
  profile.GetNonEmptyRawTypes(&non_empty_raw_types);

  EXPECT_THAT(non_empty_raw_types,
              testing::UnorderedElementsAreArray(expected_raw_types));
}

}  // namespace autofill
