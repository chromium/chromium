// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"

#include <stddef.h>
#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace structured_address {

using AddressComponentTestValues = std::vector<AddressComponentTestValue>;

namespace {

// A test record that contains all entries of the hybrid-structure name tree.
struct NameParserTestRecord {
  std::string full;
  std::string honorific;
  std::string first;
  std::string middle;
  std::string last;
  std::string last_first;
  std::string last_conjunction;
  std::string last_second;
};

// A test record that contains all entries of the hybrid-structure last name
// tree.
struct LastNameParserTestRecord {
  std::string last_name;
  std::string first;
  std::string conjunction;
  std::string second;
};

// Function to test the parsing of a name from the full (unstructured)
// representation into its subcomponents.
void TestNameParsing(const base::string16& full,
                     const base::string16& honorific,
                     const base::string16& first,
                     const base::string16& middle,
                     const base::string16& last,
                     const base::string16& last_first,
                     const base::string16& last_conjunction,
                     const base::string16& last_second) {
  SCOPED_TRACE(full);
  NameFull name;
  name.SetValueForTypeIfPossible(NAME_FULL, full,
                                 VerificationStatus::kObserved);
  name.CompleteFullTree();

  EXPECT_EQ(name.GetValueForType(NAME_FULL), full);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(name.GetValueForType(NAME_HONORIFIC_PREFIX), honorific);
  EXPECT_EQ(name.GetValueForType(NAME_FIRST), first);
  EXPECT_EQ(name.GetValueForType(NAME_MIDDLE), middle);
  EXPECT_EQ(name.GetValueForType(NAME_LAST), last);
  EXPECT_EQ(name.GetValueForType(NAME_LAST_FIRST), last_first);
  EXPECT_EQ(name.GetValueForType(NAME_LAST_CONJUNCTION), last_conjunction);
  EXPECT_EQ(name.GetValueForType(NAME_LAST_SECOND), last_second);
}

// Testing function for parsing a |NAME_LAST| into its subcomponents.
void TestLastNameParsing(const base::string16& last_name,
                         const base::string16& target_first,
                         const base::string16& target_conjunction,
                         const base::string16& target_second) {
  SCOPED_TRACE(last_name);

  NameLast last_name_component(nullptr);
  last_name_component.SetValueForTypeIfPossible(NAME_LAST, last_name,
                                                VerificationStatus::kObserved);

  last_name_component.CompleteFullTree();

  EXPECT_EQ(last_name_component.GetValueForType(NAME_LAST_FIRST), target_first);
  EXPECT_EQ(last_name_component.GetValueForType(NAME_LAST_CONJUNCTION),
            target_conjunction);
  EXPECT_EQ(last_name_component.GetValueForType(NAME_LAST_SECOND),
            target_second);
}

}  // namespace

// Tests the parsing of last names into their tree components:
// * The first part, that is only used in Latinx/Hispanic names.
// * The conjunction, that is optional in Latinx/Hispanic names.
// * The second part, for Latinx/Hispanic and all other last names.
TEST(AutofillStructuredName, ParseLastName) {
  LastNameParserTestRecord last_name_tests[] = {
      // "von" is a known prefix for a surname and should be therefore parsed
      // into the second last name
      {"von Kitzling", "", "", "von Kitzling"},
      {"Bush", "", "", "Bush"},
      {"Picasso", "", "", "Picasso"},
      // Ruiz is a common Spanish name and parsing into first and second last
      // name should be applied. "de la" are known surname prefixes and should
      // be included into the subsequeny token.
      {"Ruiz de la Torro", "Ruiz", "", "de la Torro"},
      {"Ruiz Picasso", "Ruiz", "", "Picasso"},
      // "y" and "i" are known conjunctions.
      {"Ruiz Y Picasso", "Ruiz", "Y", "Picasso"},
      {"Ruiz y Picasso", "Ruiz", "y", "Picasso"},
      {"Ruiz i Picasso", "Ruiz", "i", "Picasso"}};

  for (const auto& last_name_test : last_name_tests) {
    TestLastNameParsing(ASCIIToUTF16(last_name_test.last_name),
                        ASCIIToUTF16(last_name_test.first),
                        ASCIIToUTF16(last_name_test.conjunction),
                        ASCIIToUTF16(last_name_test.second));
  }
}

// Tests the parsing of full names into their subcomponents.
TEST(AutofillStructuredName, ParseFullName) {
  NameParserTestRecord name_tests[] = {
      // Name starting with a last name, followed by a comma and the first and
      // middle name.
      {"Mueller, Hans Peter", "", "Hans", "Peter", "Mueller", "", "",
       "Mueller"},
      // Same with an honorific prefix an multiple middle names.
      // middle name.
      {"Prof. Mueller, Hans Walter Peter", "Prof.", "Hans", "Walter Peter",
       "Mueller", "", "", "Mueller"},
      // Name that includes a hyphen.
      {"Dr. Hans-Peter Mueller", "Dr.", "Hans-Peter", "", "Mueller", "", "",
       "Mueller"},
      // Name with honorific prefix but without a middle name.
      {"Prof. Albert Einstein", "Prof.", "Albert", "", "Einstein", "", "",
       "Einstein"},
      // Name with honorific prefix and a middle name.
      {"Dr. Richard Phillips Feynman", "Dr.", "Richard", "Phillips", "Feynman",
       "", "", "Feynman"},
      // Name with honorific prefix and multiple middle name.
      {"Dr. Richard Phillips Isaac Feynman", "Dr.", "Richard", "Phillips Isaac",
       "Feynman", "", "", "Feynman"},
      // Hispanic/Latinx name with two surname and a conjunction.
      {"Pablo Diego Ruiz y Picasso", "", "Pablo Diego", "", "Ruiz y Picasso",
       "Ruiz", "y", "Picasso"},
      // Hispanic/Latinx name with two surname and a conjunction with an
      // honorific prefix.
      {"Mr. Pablo Ruiz y Picasso", "Mr.", "Pablo", "", "Ruiz y Picasso", "Ruiz",
       "y", "Picasso"},
      // Name with multiple middle names.
      {"George Walker Junior Bush", "", "George", "Walker Junior", "Bush", "",
       "", "Bush"},
      // Name with a single middle name.
      {"George Walker Bush", "", "George", "Walker", "Bush", "", "", "Bush"},
      // Name without names.
      {"George Bush", "", "George", "", "Bush", "", "", "Bush"},
      // Three character Korean name wit two-character surname.
      {"欧阳龙", "", "龙", "", "欧阳", "", "", "欧阳"},
      // Four character Korean name wit two-character surname.
      {"欧阳龙龙", "", "龙龙", "", "欧阳", "", "", "欧阳"},
      // Full name including given, middle and family names.
      {"Homer Jay Simpson", "", "Homer", "Jay", "Simpson", "", "", "Simpson"},
      // No middle name.
      {"Moe Szyslak", "", "Moe", "", "Szyslak", "", "", "Szyslak"},
      // Common name prefixes parsed into the honorific prefix.
      {"Reverend Timothy Lovejoy", "Reverend", "Timothy", "", "Lovejoy", "", "",
       "Lovejoy"},
      // Only a last name with a preposition.
      {"von Gutenberg", "", "", "", "von Gutenberg", "", "", "von Gutenberg"},
      // Common name suffixes removed.
      {"John Frink Phd", "", "John", "", "Frink", "", "", "Frink"},
      // Only lase name with common name suffixes removed.
      {"Frink Phd", "", "", "", "Frink", "", "", "Frink"},
      // Since "Ma" is a common last name, "Ma" was removed from the suffixes.
      {"John Ma", "", "John", "", "Ma", "", "", "Ma"},
      // Common family name prefixes not considered a middle name.
      {"Milhouse Van Houten", "", "Milhouse", "", "Van Houten", "", "",
       "Van Houten"},
      // Chinese name, Unihan
      {"孫 德明", "", "德明", "", "孫", "", "", "孫"},
      // Chinese name, Unihan, 'IDEOGRAPHIC SPACE'
      {"孫　德明", "", "德明", "", "孫", "", "", "孫"},
      // Korean name, Hangul
      {"홍 길동", "", "길동", "", "홍", "", "", "홍"},
      // Japanese name, Unihan
      {"山田 貴洋", "", "貴洋", "", "山田", "", "", "山田"},
      // In Japanese, foreign names use 'KATAKANA MIDDLE DOT' (U+30FB) as a
      // separator. There is no consensus for the ordering. For now, we use
      // the same ordering as regular Japanese names ("last・first").
      // Foreign name in Japanese, Katakana
      {"ゲイツ・ビル", "", "ビル", "", "ゲイツ", "", "", "ゲイツ"},
      // 'KATAKANA MIDDLE DOT' is occasionally typoed as 'MIDDLE DOT' (U+00B7).
      {"ゲイツ·ビル", "", "ビル", "", "ゲイツ", "", "", "ゲイツ"},
      // CJK names don't usually have a space in the middle, but most of the
      // time, the surname is only one character (in Chinese & Korean).
      {"최성훈", "", "성훈", "", "최", "", "", "최"},  // Korean name, Hangul
      // (Simplified) Chinese name, Unihan
      {"刘翔", "", "翔", "", "刘", "", "", "刘"},
      // (Traditional) Chinese name, Unihan
      {"劉翔", "", "翔", "", "劉", "", "", "劉"},
      // Korean name, Hangul
      {"남궁도", "", "도", "", "남궁", "", "", "남궁"},
      // Korean name, Hangul
      {"황보혜정", "", "혜정", "", "황보", "", "", "황보"},
      // (Traditional) Chinese name, Unihan
      {"歐陽靖", "", "靖", "", "歐陽", "", "", "歐陽"},
      // In Korean, some 2-character surnames are rare/ambiguous, like "강전":
      // "강" is a common surname, and "전" can be part of a given name. In
      // those cases, we assume it's 1/2 for 3-character names, or 2/2 for
      // 4-character names.
      // Korean name, Hangul
      {"강전희", "", "전희", "", "강", "", "", "강"},
      // Korean name, Hangul
      {"황목치승", "", "치승", "", "황목", "", "", "황목"},
      // It occasionally happens that a full name is 2 characters, 1/1.
      // Korean name, Hangul
      {"이도", "", "도", "", "이", "", "", "이"},
      // Chinese name, Unihan
      {"孫文", "", "文", "", "孫", "", "", "孫"}};

  for (const auto& name_test : name_tests) {
    TestNameParsing(base::UTF8ToUTF16(name_test.full),
                    base::UTF8ToUTF16(name_test.honorific),
                    base::UTF8ToUTF16(name_test.first),
                    base::UTF8ToUTF16(name_test.middle),
                    base::UTF8ToUTF16(name_test.last),
                    base::UTF8ToUTF16(name_test.last_first),
                    base::UTF8ToUTF16(name_test.last_conjunction),
                    base::UTF8ToUTF16(name_test.last_second));
  }
}

// Tests the detection of CJK name characteristics.
TEST(AutofillStructuredName, HasCjkNameCharacteristics) {
  EXPECT_FALSE(HasCjkNameCharacteristics("Peterson"));
  EXPECT_TRUE(HasCjkNameCharacteristics("ㅎ"));
  EXPECT_TRUE(HasCjkNameCharacteristics("房仕龙"));
  EXPECT_TRUE(HasCjkNameCharacteristics("房仕龙龙"));
  EXPECT_TRUE(HasCjkNameCharacteristics("房仕龙"));
  EXPECT_TRUE(HasCjkNameCharacteristics("房仕・龙"));
  EXPECT_FALSE(HasCjkNameCharacteristics("・"));
  EXPECT_FALSE(HasCjkNameCharacteristics("房・仕・龙"));
  // Non-CJK language with only ASCII characters.
  EXPECT_FALSE(HasCjkNameCharacteristics("Homer Jay Simpson"));
  // Non-CJK language with some ASCII characters.
  EXPECT_FALSE(HasCjkNameCharacteristics("Éloïse Paré"));
  // Non-CJK language with no ASCII characters.
  EXPECT_FALSE(HasCjkNameCharacteristics("Σωκράτης"));
  // (Simplified) Chinese name, Unihan.
  EXPECT_TRUE(HasCjkNameCharacteristics("刘翔"));
  // (Simplified) Chinese name, Unihan, with an ASCII space.
  EXPECT_TRUE(HasCjkNameCharacteristics("成 龙"));
  // Korean name, Hangul.
  EXPECT_TRUE(HasCjkNameCharacteristics("송지효"));
  // Korean name, Hangul, with an 'IDEOGRAPHIC SPACE' (U+3000).
  EXPECT_TRUE(HasCjkNameCharacteristics("김　종국"));
  // Japanese name, Unihan.
  EXPECT_TRUE(HasCjkNameCharacteristics("山田貴洋"));
  // Japanese name, Katakana, with a 'KATAKANA MIDDLE DOT' (U+30FB).
  EXPECT_TRUE(HasCjkNameCharacteristics("ビル・ゲイツ"));
  // Japanese name, Katakana, with a 'MIDDLE DOT' (U+00B7) (likely a
  // typo).
  EXPECT_TRUE(HasCjkNameCharacteristics("ビル·ゲイツ"));
  // CJK names don't have a middle name, so a 3-part name is bogus to us.
  EXPECT_FALSE(HasCjkNameCharacteristics("반 기 문"));
}

// Test the detection of Hispanic/Latinx name characteristics.
TEST(AutofillStructuredName, HasHispanicLatinxNameCharaceristics) {
  EXPECT_TRUE(HasHispanicLatinxNameCharaceristics("Pablo Ruiz Picasso"));
  EXPECT_FALSE(HasHispanicLatinxNameCharaceristics("Werner Heisenberg"));
  EXPECT_TRUE(HasHispanicLatinxNameCharaceristics("SomeName y SomeOtherName"));
}

// Test the detection of middle name initials.
TEST(AutofillStructuredName, HasMiddleNameInitialsCharacteristics) {
  EXPECT_FALSE(HasMiddleNameInitialsCharacteristics("Diego"));
  EXPECT_FALSE(HasMiddleNameInitialsCharacteristics("d"));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("D"));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("DD"));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("D.D."));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("D. D. D."));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("D-D"));
  EXPECT_TRUE(HasMiddleNameInitialsCharacteristics("D.-D."));
}

// Test the reduction of a name to its initials.
TEST(AutofillStructuredName, ReduceToInitials) {
  EXPECT_EQ(ReduceToInitials(base::ASCIIToUTF16("")), base::ASCIIToUTF16(""));
  EXPECT_EQ(ReduceToInitials(base::ASCIIToUTF16("George")),
            base::ASCIIToUTF16("G"));
  EXPECT_EQ(ReduceToInitials(base::ASCIIToUTF16("George Walker")),
            base::ASCIIToUTF16("GW"));
  EXPECT_EQ(ReduceToInitials(base::ASCIIToUTF16("michael myers")),
            base::ASCIIToUTF16("MM"));
  EXPECT_EQ(ReduceToInitials(base::ASCIIToUTF16("Hans-Peter")),
            base::ASCIIToUTF16("HP"));
}

// Test getting the field type |NAME_MIDDLE_INITIAL|.
TEST(AutofillStructuredName, GetNameMiddleInitial) {
  NameFull full_name;

  full_name.SetValueForTypeIfPossible(NAME_MIDDLE,
                                      base::ASCIIToUTF16("Michael"),
                                      VerificationStatus::kObserved);

  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("M"));

  full_name.SetValueForTypeIfPossible(NAME_MIDDLE,
                                      base::ASCIIToUTF16("Michael Myers"),
                                      VerificationStatus::kObserved);

  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("MM"));

  full_name.SetValueForTypeIfPossible(NAME_MIDDLE,
                                      base::ASCIIToUTF16("george walker"),
                                      VerificationStatus::kObserved);
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("GW"));

  // The the set value already has the characteristics of initials, the value
  // should be returned as it is.
  full_name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("GW"),
                                      VerificationStatus::kObserved);
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("GW"));

  full_name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("G. W."),
                                      VerificationStatus::kObserved);
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("G. W."));

  full_name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("G.-W."),
                                      VerificationStatus::kObserved);
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::ASCIIToUTF16("G.-W."));
}

TEST(AutofillStructuredName, TestGetSupportedTypes) {
  NameFull full_name;
  ServerFieldTypeSet supported_types;
  full_name.GetSupportedTypes(&supported_types);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  EXPECT_EQ(ServerFieldTypeSet({NAME_FULL, /*NAME_HONORIFIC_PREFIX*/ NAME_FIRST,
                                NAME_MIDDLE, NAME_MIDDLE_INITIAL, NAME_LAST,
                                NAME_LAST_FIRST, NAME_LAST_CONJUNCTION,
                                NAME_LAST_SECOND}),
            supported_types);
}

TEST(AutofillStructuredName, TestSettingMiddleNameInitial) {
  NameFull full_name;
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE), base::string16());

  EXPECT_TRUE(full_name.SetValueForTypeIfPossible(
      NAME_MIDDLE_INITIAL, base::UTF8ToUTF16("M"),
      VerificationStatus::kObserved));
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE_INITIAL),
            base::UTF8ToUTF16("M"));
  EXPECT_EQ(full_name.GetValueForType(NAME_MIDDLE), base::UTF8ToUTF16("M"));
}

TEST(AutofillStructuredName, MergePermutatedNames) {
  NameFull one;
  NameFull two;

  // The first component has an observed substructure of the full name.
  EXPECT_TRUE(one.SetValueForTypeIfPossible(
      NAME_FIRST, base::ASCIIToUTF16("First"), VerificationStatus::kObserved));
  EXPECT_TRUE(one.SetValueForTypeIfPossible(
      NAME_LAST, base::ASCIIToUTF16("Last"), VerificationStatus::kObserved));
  one.CompleteFullTree();

  // The formatted full name has the canonical representation "FIRST LAST".
  EXPECT_EQ(one.GetValueForType(NAME_FULL), base::ASCIIToUTF16("First Last"));
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kFormatted);

  // In contrast, the second component has a verified name in an alternative
  // representation "LAST, FIRST"
  EXPECT_TRUE(two.SetValueForTypeIfPossible(NAME_FULL,
                                            base::ASCIIToUTF16("Last, First"),
                                            VerificationStatus::kUserVerified));
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_TRUE(two.CompleteFullTree());
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);

  EXPECT_EQ(two.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("First"));
  EXPECT_EQ(two.GetValueForType(NAME_LAST), base::ASCIIToUTF16("Last"));

  EXPECT_TRUE(one.MergeWithComponent(two));

  // It is expected that the alternative representation of the second component
  // is merged into the first one, while maintaining the observed substructure.
  EXPECT_EQ(one.GetValueForType(NAME_FULL), base::ASCIIToUTF16("Last, First"));
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("First"));
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetValueForType(NAME_LAST), base::ASCIIToUTF16("Last"));
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
}

TEST(AutofillStructuredName, MergeNamesByCombiningSubstructureObservations) {
  NameFull one;
  NameFull two;

  // The first name has an incorrect componentization of the last name, but a
  // correctly observed structure of title, first, middle, last.
  one.SetValueForTypeIfPossible(
      NAME_FULL, base::ASCIIToUTF16("Mr Pablo Diego Ruiz y Picasso"),
      VerificationStatus::kUserVerified);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // one.SetValueForTypeIfPossible(NAME_HONORIFIC_PREFIX,
  // base::ASCIIToUTF16("Mr"),
  //                              VerificationStatus::kObserved);
  one.SetValueForTypeIfPossible(NAME_FIRST, base::ASCIIToUTF16("Pablo Diego"),
                                VerificationStatus::kObserved);
  one.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16(""),
                                VerificationStatus::kObserved);
  one.SetValueForTypeIfPossible(NAME_LAST, base::ASCIIToUTF16("Ruiz y Picasso"),
                                VerificationStatus::kObserved);
  one.SetValueForTypeIfPossible(NAME_LAST_SECOND,
                                base::ASCIIToUTF16("Ruiz y Picasso"),
                                VerificationStatus::kParsed);

  // The second name has a correct componentization of the last name, but an
  // incorrectly parsed structure of title,first,middle,last.
  two.SetValueForTypeIfPossible(
      NAME_FULL, base::ASCIIToUTF16("Mr Pablo Diego Ruiz y Picasso"),
      VerificationStatus::kUserVerified);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // two.SetValueForTypeIfPossible(NAME_HONORIFIC_PREFIX,
  // base::ASCIIToUTF16(""),
  //                              VerificationStatus::kParsed);
  two.SetValueForTypeIfPossible(NAME_FIRST, base::ASCIIToUTF16("Pablo"),
                                VerificationStatus::kParsed);
  two.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("Diego"),
                                VerificationStatus::kParsed);
  two.SetValueForTypeIfPossible(NAME_LAST, base::ASCIIToUTF16("Ruiz y Picasso"),
                                VerificationStatus::kParsed);
  two.SetValueForTypeIfPossible(NAME_LAST_FIRST, base::ASCIIToUTF16("Ruiz"),
                                VerificationStatus::kObserved);
  two.SetValueForTypeIfPossible(NAME_LAST_CONJUNCTION, base::ASCIIToUTF16("y"),
                                VerificationStatus::kObserved);
  two.SetValueForTypeIfPossible(NAME_LAST_SECOND, base::ASCIIToUTF16("Picasso"),
                                VerificationStatus::kObserved);

  // By merging both, it is expected that the title, first, middle, last
  // structure of |one| is maintained, while the substructure of the last name
  // is taken from two.
  NameFull copy_of_one;
  copy_of_one = one;
  EXPECT_TRUE(one.MergeWithComponent(two));

  EXPECT_EQ(one.GetValueForType(NAME_FULL),
            base::ASCIIToUTF16("Mr Pablo Diego Ruiz y Picasso"));
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(one.GetValueForType(NAME_HONORIFIC_PREFIX),
  //          base::ASCIIToUTF16("Mr"));
  EXPECT_EQ(one.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("Pablo Diego"));
  EXPECT_EQ(one.GetValueForType(NAME_MIDDLE), base::ASCIIToUTF16(""));
  EXPECT_EQ(one.GetValueForType(NAME_LAST),
            base::ASCIIToUTF16("Ruiz y Picasso"));
  EXPECT_EQ(one.GetValueForType(NAME_LAST_FIRST), base::ASCIIToUTF16("Ruiz"));
  EXPECT_EQ(one.GetValueForType(NAME_LAST_CONJUNCTION),
            base::ASCIIToUTF16("y"));
  EXPECT_EQ(one.GetValueForType(NAME_LAST_SECOND),
            base::ASCIIToUTF16("Picasso"));

  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(one.GetVerificationStatusForType(NAME_HONORIFIC_PREFIX),
  //          VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST_CONJUNCTION),
            VerificationStatus::kObserved);
  EXPECT_EQ(one.GetVerificationStatusForType(NAME_LAST_SECOND),
            VerificationStatus::kObserved);

  // The merging should work in both directions equally.
  EXPECT_TRUE(two.MergeWithComponent(copy_of_one));

  EXPECT_EQ(two.GetValueForType(NAME_FULL),
            base::ASCIIToUTF16("Mr Pablo Diego Ruiz y Picasso"));
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(two.GetValueForType(NAME_HONORIFIC_PREFIX),
  //          base::ASCIIToUTF16("Mr"));
  EXPECT_EQ(two.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("Pablo Diego"));
  EXPECT_EQ(two.GetValueForType(NAME_MIDDLE), base::ASCIIToUTF16(""));
  EXPECT_EQ(two.GetValueForType(NAME_LAST),
            base::ASCIIToUTF16("Ruiz y Picasso"));
  EXPECT_EQ(two.GetValueForType(NAME_LAST_FIRST), base::ASCIIToUTF16("Ruiz"));
  EXPECT_EQ(two.GetValueForType(NAME_LAST_CONJUNCTION),
            base::ASCIIToUTF16("y"));
  EXPECT_EQ(two.GetValueForType(NAME_LAST_SECOND),
            base::ASCIIToUTF16("Picasso"));

  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // EXPECT_EQ(two.GetVerificationStatusForType(NAME_HONORIFIC_PREFIX),
  //          VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_LAST_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_LAST_CONJUNCTION),
            VerificationStatus::kObserved);
  EXPECT_EQ(two.GetVerificationStatusForType(NAME_LAST_SECOND),
            VerificationStatus::kObserved);
}

TEST(AutofillStructuredName, TestCopyConstructuror) {
  NameFull orginal;
  // The first name has an incorrect componentization of the last name, but
  // a correctly observed structure of title, first, middle, last.
  orginal.SetValueForTypeIfPossible(
      NAME_FULL, base::ASCIIToUTF16("Mr Pablo Diego Ruiz y Picasso"),
      VerificationStatus::kUserVerified);
  orginal.SetValueForTypeIfPossible(NAME_HONORIFIC_PREFIX,
                                    base::ASCIIToUTF16("Mr"),
                                    VerificationStatus::kObserved);
  orginal.SetValueForTypeIfPossible(NAME_FIRST,
                                    base::ASCIIToUTF16("Pablo Diego"),
                                    VerificationStatus::kObserved);
  orginal.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16(""),
                                    VerificationStatus::kObserved);
  orginal.SetValueForTypeIfPossible(NAME_LAST,
                                    base::ASCIIToUTF16("Ruiz y Picasso"),
                                    VerificationStatus::kObserved);
  orginal.SetValueForTypeIfPossible(NAME_LAST_SECOND,
                                    base::ASCIIToUTF16("Ruiz y Picasso"),
                                    VerificationStatus::kParsed);

  NameFull copy = orginal;
  EXPECT_EQ(orginal, copy);
}

TEST(AutofillStructuredName,
     MigrationFromLegacyStructure_WithFullName_Unverified) {
  NameFull name;
  name.SetValueForTypeIfPossible(NAME_FULL,
                                 base::ASCIIToUTF16("Thomas Neo Anderson"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_FIRST, base::ASCIIToUTF16("Thomas"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("Neo"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_LAST, base::ASCIIToUTF16("Anderson"),
                                 VerificationStatus::kNoStatus);

  name.MigrateLegacyStructure(false);

  // Since the full name is set and the profile is not verified it is promoted
  // to observed. All other tokens are reset.
  EXPECT_EQ(name.GetValueForType(NAME_FULL),
            base::ASCIIToUTF16("Thomas Neo Anderson"));
  EXPECT_EQ(name.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("Thomas"));
  EXPECT_EQ(name.GetValueForType(NAME_MIDDLE), base::ASCIIToUTF16("Neo"));
  EXPECT_EQ(name.GetValueForType(NAME_LAST), base::ASCIIToUTF16("Anderson"));
  EXPECT_EQ(name.GetValueForType(NAME_LAST_SECOND),
            base::ASCIIToUTF16("Anderson"));

  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kObserved);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_LAST_SECOND),
            VerificationStatus::kParsed);
}

TEST(AutofillStructuredName,
     MigrationFromLegacyStructure_WithFullName_Verified) {
  NameFull name;
  name.SetValueForTypeIfPossible(NAME_FULL,
                                 base::ASCIIToUTF16("Thomas Neo Anderson"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_FIRST, base::ASCIIToUTF16("Thomas"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("Neo"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_LAST, base::ASCIIToUTF16("Anderson"),
                                 VerificationStatus::kNoStatus);

  name.MigrateLegacyStructure(true);

  // Since the full name is set and the profile is verified, it is promoted to
  // kUserVerified. All other tokens are reset.
  EXPECT_EQ(name.GetValueForType(NAME_FULL),
            base::ASCIIToUTF16("Thomas Neo Anderson"));
  EXPECT_EQ(name.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("Thomas"));
  EXPECT_EQ(name.GetValueForType(NAME_MIDDLE), base::ASCIIToUTF16("Neo"));
  EXPECT_EQ(name.GetValueForType(NAME_LAST), base::ASCIIToUTF16("Anderson"));
  EXPECT_EQ(name.GetValueForType(NAME_LAST_SECOND),
            base::ASCIIToUTF16("Anderson"));

  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kUserVerified);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kParsed);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_LAST_SECOND),
            VerificationStatus::kParsed);
}

TEST(AutofillStructuredName, MigrationFromLegacyStructure_WithoutFullName) {
  NameFull name;
  // The first name has an incorrect componentization of the last name, but
  // a correctly observed structure of title, first, middle, last.
  name.SetValueForTypeIfPossible(NAME_FULL, base::ASCIIToUTF16(""),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_FIRST, base::ASCIIToUTF16("Thomas"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_MIDDLE, base::ASCIIToUTF16("Neo"),
                                 VerificationStatus::kNoStatus);
  name.SetValueForTypeIfPossible(NAME_LAST, base::ASCIIToUTF16("Anderson"),
                                 VerificationStatus::kNoStatus);

  name.MigrateLegacyStructure(false);

  // Since the full name is not set, the substructure is set to observed.
  // This is an edge case that normally should not happen.
  // Also, it is ignored that the profile might be verified because a verified
  // profile should contain a full name (or potentially no name).
  EXPECT_EQ(name.GetValueForType(NAME_FULL), base::ASCIIToUTF16(""));
  EXPECT_EQ(name.GetValueForType(NAME_FIRST), base::ASCIIToUTF16("Thomas"));
  EXPECT_EQ(name.GetValueForType(NAME_MIDDLE), base::ASCIIToUTF16("Neo"));
  EXPECT_EQ(name.GetValueForType(NAME_LAST), base::ASCIIToUTF16("Anderson"));

  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FULL),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(name.GetVerificationStatusForType(NAME_LAST),
            VerificationStatus::kObserved);
}

TEST(AutofillStructuredName, MergeSubsetLastname) {
  NameFull name;
  NameFull subset_name;
  name.SetMergeModeForTesting(kRecursivelyMergeSingleTokenSubset |
                              kRecursivelyMergeTokenEquivalentValues);

  AddressComponentTestValues name_values = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson y Smith",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues subset_name_values = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_FIRST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_SECOND,
       .value = "Smith",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues expectation = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_FIRST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_CONJUNCTION,
       .value = "y",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST_SECOND,
       .value = "Smith",
       .status = VerificationStatus::kObserved},
  };

  SetTestValues(&name, name_values);
  SetTestValues(&subset_name, subset_name_values);

  EXPECT_TRUE(name.IsMergeableWithComponent(subset_name));
  EXPECT_TRUE(name.MergeWithComponent(subset_name));

  VerifyTestValues(&name, name_values);
}

TEST(AutofillStructuredName, MergeSubsetLastname2) {
  NameFull name;
  NameFull subset_name;
  name.SetMergeModeForTesting(kRecursivelyMergeSingleTokenSubset |
                              kRecursivelyMergeTokenEquivalentValues);

  AddressComponentTestValues name_values = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues subset_name_values = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  AddressComponentTestValues expectation = {
      {.type = NAME_FIRST,
       .value = "Thomas",
       .status = VerificationStatus::kObserved},
      {.type = NAME_MIDDLE,
       .value = "Neo",
       .status = VerificationStatus::kObserved},
      {.type = NAME_LAST,
       .value = "Anderson",
       .status = VerificationStatus::kObserved},
  };

  SetTestValues(&name, name_values);
  SetTestValues(&subset_name, subset_name_values);

  EXPECT_TRUE(name.IsMergeableWithComponent(subset_name));
  EXPECT_TRUE(name.MergeWithComponent(subset_name));

  VerifyTestValues(&name, name_values);
}

}  // namespace structured_address
}  // namespace autofill
