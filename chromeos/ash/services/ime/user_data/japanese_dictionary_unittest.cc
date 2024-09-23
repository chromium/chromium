// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data/japanese_dictionary.h"

#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/japanese_dictionary.pb.h"
#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_dictionary.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {

using chromeos_input::JapaneseDictionary;
using mojom::JapaneseDictionaryEntry;
using mojom::JpPosType;

void SetEntry(JapaneseDictionary::Entry& entry,
              const std::string& key,
              const std::string& value,
              JapaneseDictionary::PosType pos,
              const std::string& comment) {
  entry.set_key(key);
  entry.set_value(value);
  entry.set_comment(comment);
  entry.set_pos(pos);
}

TEST(JapaneseDictionaryTest, MakeMojomJapaneseDictionary) {
  chromeos_input::JapaneseDictionary dict;
  dict.set_id(1);
  dict.set_name("Dictionary 1 ください");
  SetEntry(*dict.add_entries(), /*key=*/"noPos", /*value=*/"品詞なし",
           /*pos_type=*/JapaneseDictionary::NO_POS,
           /*comment=*/"notes:品詞なし");
  SetEntry(*dict.add_entries(), /*key=*/"noun", /*value=*/"名詞",
           /*pos_type=*/JapaneseDictionary::NOUN, /*comment=*/"notes:名詞");
  SetEntry(*dict.add_entries(), /*key=*/"abbreviation", /*value=*/"短縮よみ",
           /*pos_type=*/JapaneseDictionary::ABBREVIATION,
           /*comment=*/"notes:短縮よみ");
  SetEntry(*dict.add_entries(), /*key=*/"suggestionOnly",
           /*value=*/"サジェストのみ",
           /*pos_type=*/JapaneseDictionary::SUGGESTION_ONLY,
           /*comment=*/"notes:サジェストのみ");
  SetEntry(*dict.add_entries(), /*key=*/"properNoun", /*value=*/"固有名詞",
           /*pos_type=*/JapaneseDictionary::PROPER_NOUN,
           /*comment=*/"notes:固有名詞");
  SetEntry(*dict.add_entries(), /*key=*/"personalName", /*value=*/"人名",
           /*pos_type=*/JapaneseDictionary::PERSONAL_NAME,
           /*comment=*/"notes:人名");
  SetEntry(*dict.add_entries(), /*key=*/"familyName", /*value=*/"姓",
           /*pos_type=*/JapaneseDictionary::FAMILY_NAME,
           /*comment=*/"notes:姓");
  SetEntry(*dict.add_entries(), /*key=*/"firstName", /*value=*/"名",
           /*pos_type=*/JapaneseDictionary::FIRST_NAME,
           /*comment=*/"notes:名");
  SetEntry(*dict.add_entries(), /*key=*/"organizationName", /*value=*/"組織",
           /*pos_type=*/JapaneseDictionary::ORGANIZATION_NAME,
           /*comment=*/"notes:組織");
  SetEntry(*dict.add_entries(), /*key=*/"placeName", /*value=*/"地名",
           /*pos_type=*/JapaneseDictionary::PLACE_NAME,
           /*comment=*/"notes:地名");
  SetEntry(*dict.add_entries(), /*key=*/"saIrregularConjugationNoun",
           /*value=*/"名詞サ変",
           /*pos_type=*/JapaneseDictionary::SA_IRREGULAR_CONJUGATION_NOUN,
           /*comment=*/"notes:名詞サ変");
  SetEntry(*dict.add_entries(), /*key=*/"adjectiveVerbalNoun",
           /*value=*/"名詞形動",
           /*pos_type=*/JapaneseDictionary::ADJECTIVE_VERBAL_NOUN,
           /*comment=*/"notes:名詞形動");
  SetEntry(*dict.add_entries(), /*key=*/"number", /*value=*/"数",
           /*pos_type=*/JapaneseDictionary::NUMBER, /*comment=*/"notes:数");
  SetEntry(*dict.add_entries(), /*key=*/"alphabet", /*value=*/"アルファベッ",
           /*pos_type=*/JapaneseDictionary::ALPHABET,
           /*comment=*/"notes:アルファベット");
  SetEntry(*dict.add_entries(), /*key=*/"symbol", /*value=*/"記号",
           /*pos_type=*/JapaneseDictionary::SYMBOL, /*comment=*/"notes:記号");
  SetEntry(*dict.add_entries(), /*key=*/"emoticon", /*value=*/"顔文字",
           /*pos_type=*/JapaneseDictionary::EMOTICON,
           /*comment=*/"notes:顔文字");
  SetEntry(*dict.add_entries(), /*key=*/"adverb", /*value=*/"副詞",
           /*pos_type=*/JapaneseDictionary::ADVERB, /*comment=*/"notes:副詞");
  SetEntry(*dict.add_entries(), /*key=*/"prenounAdjectival", /*value=*/"連体詞",
           /*pos_type=*/JapaneseDictionary::PRENOUN_ADJECTIVAL,
           /*comment=*/"notes:連体詞");
  SetEntry(*dict.add_entries(), /*key=*/"conjunction", /*value=*/"接続詞",
           /*pos_type=*/JapaneseDictionary::CONJUNCTION,
           /*comment=*/"notes:接続詞");
  SetEntry(*dict.add_entries(), /*key=*/"interjection", /*value=*/"感動詞",
           /*pos_type=*/JapaneseDictionary::INTERJECTION,
           /*comment=*/"notes:感動詞");
  SetEntry(*dict.add_entries(), /*key=*/"prefix", /*value=*/"接頭語",
           /*pos_type=*/JapaneseDictionary::PREFIX,
           /*comment=*/"notes:接頭語");
  SetEntry(*dict.add_entries(), /*key=*/"counterSuffix", /*value=*/"助数詞",
           /*pos_type=*/JapaneseDictionary::COUNTER_SUFFIX,
           /*comment=*/"notes:助数詞");
  SetEntry(*dict.add_entries(), /*key=*/"genericSuffix", /*value=*/"接尾一般",
           /*pos_type=*/JapaneseDictionary::GENERIC_SUFFIX,
           /*comment=*/"notes:接尾一般");
  SetEntry(*dict.add_entries(), /*key=*/"personNameSuffix",
           /*value=*/"接尾人名",
           /*pos_type=*/JapaneseDictionary::PERSON_NAME_SUFFIX,
           /*comment=*/"notes:接尾人名");
  SetEntry(*dict.add_entries(), /*key=*/"placeNameSuffix", /*value=*/"接尾地名",
           /*pos_type=*/JapaneseDictionary::PLACE_NAME_SUFFIX,
           /*comment=*/"notes:接尾地名");
  SetEntry(*dict.add_entries(), /*key=*/"waGroup1Verb",
           /*value=*/"動詞ワ行五段",
           /*pos_type=*/JapaneseDictionary::WA_GROUP1_VERB,
           /*comment=*/"notes:動詞ワ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"kaGroup1Verb",
           /*value=*/"動詞カ行五段",
           /*pos_type=*/JapaneseDictionary::KA_GROUP1_VERB,
           /*comment=*/"notes:動詞カ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"saGroup1Verb",
           /*value=*/"動詞サ行五段",
           /*pos_type=*/JapaneseDictionary::SA_GROUP1_VERB,
           /*comment=*/"notes:動詞サ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"taGroup1Verb",
           /*value=*/"動詞タ行五段",
           /*pos_type=*/JapaneseDictionary::TA_GROUP1_VERB,
           /*comment=*/"notes:動詞タ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"naGroup1Verb",
           /*value=*/"動詞ナ行五段",
           /*pos_type=*/JapaneseDictionary::NA_GROUP1_VERB,
           /*comment=*/"notes:動詞ナ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"maGroup1Verb",
           /*value=*/"動詞マ行五段",
           /*pos_type=*/JapaneseDictionary::MA_GROUP1_VERB,
           /*comment=*/"notes:動詞マ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"raGroup1Verb",
           /*value=*/"動詞ラ行五段",
           /*pos_type=*/JapaneseDictionary::RA_GROUP1_VERB,
           /*comment=*/"notes:動詞ラ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"gaGroup1Verb",
           /*value=*/"動詞ガ行五段",
           /*pos_type=*/JapaneseDictionary::GA_GROUP1_VERB,
           /*comment=*/"notes:動詞ガ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"baGroup1Verb",
           /*value=*/"動詞バ行五段",
           /*pos_type=*/JapaneseDictionary::BA_GROUP1_VERB,
           /*comment=*/"notes:動詞バ行五段");
  SetEntry(*dict.add_entries(), /*key=*/"haGroup1Verb",
           /*value=*/"動詞ハ行四段",
           /*pos_type=*/JapaneseDictionary::HA_GROUP1_VERB,
           /*comment=*/"notes:動詞ハ行四段");
  SetEntry(*dict.add_entries(), /*key=*/"group2Verb", /*value=*/"動詞一段",
           /*pos_type=*/JapaneseDictionary::GROUP2_VERB,
           /*comment=*/"notes:動詞一段");
  SetEntry(*dict.add_entries(), /*key=*/"kuruGroup3Verb", /*value=*/"動詞カ変",
           /*pos_type=*/JapaneseDictionary::KURU_GROUP3_VERB,
           /*comment=*/"notes:動詞カ変");
  SetEntry(*dict.add_entries(), /*key=*/"suruGroup3Verb", /*value=*/"動詞サ変",
           /*pos_type=*/JapaneseDictionary::SURU_GROUP3_VERB,
           /*comment=*/"notes:動詞サ変");
  SetEntry(*dict.add_entries(), /*key=*/"zuruGroup3Verb", /*value=*/"動詞ザ変",
           /*pos_type=*/JapaneseDictionary::ZURU_GROUP3_VERB,
           /*comment=*/"notes:動詞ザ変");
  SetEntry(*dict.add_entries(), /*key=*/"ruGroup3Verb", /*value=*/"動詞ラ変",
           /*pos_type=*/JapaneseDictionary::RU_GROUP3_VERB,
           /*comment=*/"notes:動詞ラ変");
  SetEntry(*dict.add_entries(), /*key=*/"adjective", /*value=*/"形容詞",
           /*pos_type=*/JapaneseDictionary::ADJECTIVE,
           /*comment=*/"notes:形容詞");
  SetEntry(*dict.add_entries(), /*key=*/"sentenceEndingParticle",
           /*value=*/"終助詞",
           /*pos_type=*/JapaneseDictionary::SENTENCE_ENDING_PARTICLE,
           /*comment=*/"notes:終助詞");
  SetEntry(*dict.add_entries(), /*key=*/"punctuation", /*value=*/"句読点",
           /*pos_type=*/JapaneseDictionary::PUNCTUATION,
           /*comment=*/"notes:句読点");
  SetEntry(*dict.add_entries(), /*key=*/"freeStandingWord", /*value=*/"独立語",
           /*pos_type=*/JapaneseDictionary::FREE_STANDING_WORD,
           /*comment=*/"notes:独立語");
  SetEntry(*dict.add_entries(), /*key=*/"suppressionWord", /*value=*/"抑制単語",
           /*pos_type=*/JapaneseDictionary::SUPPRESSION_WORD,
           /*comment=*/"notes:抑制単語");

  mojom::JapaneseDictionaryPtr result = MakeMojomJapaneseDictionary(dict);

  mojom::JapaneseDictionaryPtr expected = mojom::JapaneseDictionary::New();
  expected->id = 1;
  expected->name = "Dictionary 1 ください";
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"noPos", /*value=*/"品詞なし", /*pos_type=*/JpPosType::kNoPos,
      /*comment=*/"notes:品詞なし"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"noun", /*value=*/"名詞", /*pos_type=*/JpPosType::kNoun,
      /*comment=*/"notes:名詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"abbreviation", /*value=*/"短縮よみ",
      /*pos_type=*/JpPosType::kAbbreviation, /*comment=*/"notes:短縮よみ"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"suggestionOnly",
      /*value=*/"サジェストのみ", /*pos_type=*/JpPosType::kSuggestionOnly,
      /*comment=*/"notes:サジェストのみ"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"properNoun", /*value=*/"固有名詞",
      /*pos_type=*/JpPosType::kProperNoun, /*comment=*/"notes:固有名詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"personalName", /*value=*/"人名",
      /*pos_type=*/JpPosType::kPersonalName, /*comment=*/"notes:人名"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"familyName", /*value=*/"姓", /*pos_type=*/JpPosType::kFamilyName,
      /*comment=*/"notes:姓"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"firstName", /*value=*/"名", /*pos_type=*/JpPosType::kFirstName,
      /*comment=*/"notes:名"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"organizationName", /*value=*/"組織",
      /*pos_type=*/JpPosType::kOrganizationName, /*comment=*/"notes:組織"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"placeName", /*value=*/"地名", /*pos_type=*/JpPosType::kPlaceName,
      /*comment=*/"notes:地名"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"saIrregularConjugationNoun", /*value=*/"名詞サ変",
      /*pos_type=*/JpPosType::kSaIrregularConjugationNoun,
      /*comment=*/"notes:名詞サ変"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"adjectiveVerbalNoun", /*value=*/"名詞形動",
      /*pos_type=*/JpPosType::kAdjectiveVerbalNoun,
      /*comment=*/"notes:名詞形動"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"number", /*value=*/"数", /*pos_type=*/JpPosType::kNumber,
      /*comment=*/"notes:数"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"alphabet",
      /*value=*/"アルファベッ", /*pos_type=*/JpPosType::kAlphabet,
      /*comment=*/"notes:アルファベット"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"symbol", /*value=*/"記号", /*pos_type=*/JpPosType::kSymbol,
      /*comment=*/"notes:記号"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"emoticon", /*value=*/"顔文字", /*pos_type=*/JpPosType::kEmoticon,
      /*comment=*/"notes:顔文字"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"adverb", /*value=*/"副詞", /*pos_type=*/JpPosType::kAdverb,
      /*comment=*/"notes:副詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"prenounAdjectival", /*value=*/"連体詞",
      /*pos_type=*/JpPosType::kPrenounAdjectival,
      /*comment=*/"notes:連体詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"conjunction", /*value=*/"接続詞",
      /*pos_type=*/JpPosType::kConjunction, /*comment=*/"notes:接続詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"interjection", /*value=*/"感動詞",
      /*pos_type=*/JpPosType::kInterjection, /*comment=*/"notes:感動詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"prefix", /*value=*/"接頭語", /*pos_type=*/JpPosType::kPrefix,
      /*comment=*/"notes:接頭語"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"counterSuffix", /*value=*/"助数詞",
      /*pos_type=*/JpPosType::kCounterSuffix, /*comment=*/"notes:助数詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"genericSuffix", /*value=*/"接尾一般",
      /*pos_type=*/JpPosType::kGenericSuffix, /*comment=*/"notes:接尾一般"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"personNameSuffix", /*value=*/"接尾人名",
      /*pos_type=*/JpPosType::kPersonNameSuffix,
      /*comment=*/"notes:接尾人名"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"placeNameSuffix", /*value=*/"接尾地名",
      /*pos_type=*/JpPosType::kPlaceNameSuffix,
      /*comment=*/"notes:接尾地名"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"waGroup1Verb", /*value=*/"動詞ワ行五段",
      /*pos_type=*/JpPosType::kWaGroup1Verb,
      /*comment=*/"notes:動詞ワ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"kaGroup1Verb", /*value=*/"動詞カ行五段",
      /*pos_type=*/JpPosType::kKaGroup1Verb,
      /*comment=*/"notes:動詞カ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"saGroup1Verb", /*value=*/"動詞サ行五段",
      /*pos_type=*/JpPosType::kSaGroup1Verb,
      /*comment=*/"notes:動詞サ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"taGroup1Verb", /*value=*/"動詞タ行五段",
      /*pos_type=*/JpPosType::kTaGroup1Verb,
      /*comment=*/"notes:動詞タ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"naGroup1Verb", /*value=*/"動詞ナ行五段",
      /*pos_type=*/JpPosType::kNaGroup1Verb,
      /*comment=*/"notes:動詞ナ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"maGroup1Verb", /*value=*/"動詞マ行五段",
      /*pos_type=*/JpPosType::kMaGroup1Verb,
      /*comment=*/"notes:動詞マ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"raGroup1Verb", /*value=*/"動詞ラ行五段",
      /*pos_type=*/JpPosType::kRaGroup1Verb,
      /*comment=*/"notes:動詞ラ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"gaGroup1Verb", /*value=*/"動詞ガ行五段",
      /*pos_type=*/JpPosType::kGaGroup1Verb,
      /*comment=*/"notes:動詞ガ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"baGroup1Verb", /*value=*/"動詞バ行五段",
      /*pos_type=*/JpPosType::kBaGroup1Verb,
      /*comment=*/"notes:動詞バ行五段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"haGroup1Verb", /*value=*/"動詞ハ行四段",
      /*pos_type=*/JpPosType::kHaGroup1Verb,
      /*comment=*/"notes:動詞ハ行四段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"group2Verb", /*value=*/"動詞一段",
      /*pos_type=*/JpPosType::kGroup2Verb, /*comment=*/"notes:動詞一段"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"kuruGroup3Verb", /*value=*/"動詞カ変",
      /*pos_type=*/JpPosType::kKuruGroup3Verb,
      /*comment=*/"notes:動詞カ変"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"suruGroup3Verb", /*value=*/"動詞サ変",
      /*pos_type=*/JpPosType::kSuruGroup3Verb, /*comment=*/"notes:動詞サ変"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"zuruGroup3Verb", /*value=*/"動詞ザ変",
      /*pos_type=*/JpPosType::kZuruGroup3Verb, /*comment=*/"notes:動詞ザ変"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"ruGroup3Verb", /*value=*/"動詞ラ変",
      /*pos_type=*/JpPosType::kRuGroup3Verb, /*comment=*/"notes:動詞ラ変"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"adjective", /*value=*/"形容詞",
      /*pos_type=*/JpPosType::kAdjective,
      /*comment=*/"notes:形容詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"sentenceEndingParticle", /*value=*/"終助詞",
      /*pos_type=*/JpPosType::kSentenceEndingParticle,
      /*comment=*/"notes:終助詞"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"punctuation", /*value=*/"句読点",
      /*pos_type=*/JpPosType::kPunctuation, /*comment=*/"notes:句読点"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"freeStandingWord", /*value=*/"独立語",
      /*pos_type=*/JpPosType::kFreeStandingWord, /*comment=*/"notes:独立語"));
  expected->entries.push_back(JapaneseDictionaryEntry::New(
      /*key=*/"suppressionWord", /*value=*/"抑制単語",
      /*pos_type=*/JpPosType::kSuppressionWord,
      /*comment=*/"notes:抑制単語"));
  EXPECT_EQ(result, expected);
}

TEST(JapaneseDictionaryTest, MakeProtoJpDictEntry) {
  JapaneseDictionary::Entry result =
      MakeProtoJpDictEntry(*JapaneseDictionaryEntry::New(
          /*key=*/"firstName", /*value=*/"value",
          /*pos_type=*/JpPosType::kFirstName,
          /*comment=*/"notes"));

  EXPECT_EQ(result.key(), "firstName");
  EXPECT_EQ(result.value(), "value");
  EXPECT_EQ(result.pos(), JapaneseDictionary::FIRST_NAME);
  EXPECT_EQ(result.comment(), "notes");
}

}  // namespace
}  // namespace ash::ime
                        
