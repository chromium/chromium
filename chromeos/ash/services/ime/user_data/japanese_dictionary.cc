// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data/japanese_dictionary.h"

#include "base/containers/fixed_flat_map.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/japanese_dictionary.pb.h"
#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_dictionary.mojom.h"

namespace ash::ime {
namespace {
using chromeos_input::JapaneseDictionary;

struct PosTypePair {
  JapaneseDictionary::PosType proto;
  mojom::JpPosType mojom;
};

constexpr std::array<PosTypePair, 45> kPosTypes = {{
    {.proto = JapaneseDictionary::NO_POS, .mojom = mojom::JpPosType::kNoPos},
    {.proto = JapaneseDictionary::NOUN, .mojom = mojom::JpPosType::kNoun},
    {.proto = JapaneseDictionary::ABBREVIATION,
     .mojom = mojom::JpPosType::kAbbreviation},
    {.proto = JapaneseDictionary::SUGGESTION_ONLY,
     .mojom = mojom::JpPosType::kSuggestionOnly},
    {.proto = JapaneseDictionary::PROPER_NOUN,
     .mojom = mojom::JpPosType::kProperNoun},
    {.proto = JapaneseDictionary::PERSONAL_NAME,
     .mojom = mojom::JpPosType::kPersonalName},
    {.proto = JapaneseDictionary::FAMILY_NAME,
     .mojom = mojom::JpPosType::kFamilyName},
    {.proto = JapaneseDictionary::FIRST_NAME,
     .mojom = mojom::JpPosType::kFirstName},
    {.proto = JapaneseDictionary::ORGANIZATION_NAME,
     .mojom = mojom::JpPosType::kOrganizationName},
    {.proto = JapaneseDictionary::PLACE_NAME,
     .mojom = mojom::JpPosType::kPlaceName},
    {.proto = JapaneseDictionary::SA_IRREGULAR_CONJUGATION_NOUN,
     .mojom = mojom::JpPosType::kSaIrregularConjugationNoun},
    {.proto = JapaneseDictionary::ADJECTIVE_VERBAL_NOUN,
     .mojom = mojom::JpPosType::kAdjectiveVerbalNoun},
    {.proto = JapaneseDictionary::NUMBER, .mojom = mojom::JpPosType::kNumber},
    {.proto = JapaneseDictionary::ALPHABET,
     .mojom = mojom::JpPosType::kAlphabet},
    {.proto = JapaneseDictionary::SYMBOL, .mojom = mojom::JpPosType::kSymbol},
    {.proto = JapaneseDictionary::EMOTICON,
     .mojom = mojom::JpPosType::kEmoticon},
    {.proto = JapaneseDictionary::ADVERB, .mojom = mojom::JpPosType::kAdverb},
    {.proto = JapaneseDictionary::PRENOUN_ADJECTIVAL,
     .mojom = mojom::JpPosType::kPrenounAdjectival},
    {.proto = JapaneseDictionary::CONJUNCTION,
     .mojom = mojom::JpPosType::kConjunction},
    {.proto = JapaneseDictionary::INTERJECTION,
     .mojom = mojom::JpPosType::kInterjection},
    {.proto = JapaneseDictionary::PREFIX, .mojom = mojom::JpPosType::kPrefix},
    {.proto = JapaneseDictionary::COUNTER_SUFFIX,
     .mojom = mojom::JpPosType::kCounterSuffix},
    {.proto = JapaneseDictionary::GENERIC_SUFFIX,
     .mojom = mojom::JpPosType::kGenericSuffix},
    {.proto = JapaneseDictionary::PERSON_NAME_SUFFIX,
     .mojom = mojom::JpPosType::kPersonNameSuffix},
    {.proto = JapaneseDictionary::PLACE_NAME_SUFFIX,
     .mojom = mojom::JpPosType::kPlaceNameSuffix},
    {.proto = JapaneseDictionary::WA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kWaGroup1Verb},
    {.proto = JapaneseDictionary::KA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kKaGroup1Verb},
    {.proto = JapaneseDictionary::SA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kSaGroup1Verb},
    {.proto = JapaneseDictionary::TA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kTaGroup1Verb},
    {.proto = JapaneseDictionary::NA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kNaGroup1Verb},
    {.proto = JapaneseDictionary::MA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kMaGroup1Verb},
    {.proto = JapaneseDictionary::RA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kRaGroup1Verb},
    {.proto = JapaneseDictionary::GA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kGaGroup1Verb},
    {.proto = JapaneseDictionary::BA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kBaGroup1Verb},
    {.proto = JapaneseDictionary::HA_GROUP1_VERB,
     .mojom = mojom::JpPosType::kHaGroup1Verb},
    {.proto = JapaneseDictionary::GROUP2_VERB,
     .mojom = mojom::JpPosType::kGroup2Verb},
    {.proto = JapaneseDictionary::KURU_GROUP3_VERB,
     .mojom = mojom::JpPosType::kKuruGroup3Verb},
    {.proto = JapaneseDictionary::SURU_GROUP3_VERB,
     .mojom = mojom::JpPosType::kSuruGroup3Verb},
    {.proto = JapaneseDictionary::ZURU_GROUP3_VERB,
     .mojom = mojom::JpPosType::kZuruGroup3Verb},
    {.proto = JapaneseDictionary::RU_GROUP3_VERB,
     .mojom = mojom::JpPosType::kRuGroup3Verb},
    {.proto = JapaneseDictionary::ADJECTIVE,
     .mojom = mojom::JpPosType::kAdjective},
    {.proto = JapaneseDictionary::SENTENCE_ENDING_PARTICLE,
     .mojom = mojom::JpPosType::kSentenceEndingParticle},
    {.proto = JapaneseDictionary::PUNCTUATION,
     .mojom = mojom::JpPosType::kPunctuation},
    {.proto = JapaneseDictionary::FREE_STANDING_WORD,
     .mojom = mojom::JpPosType::kFreeStandingWord},
    {.proto = JapaneseDictionary::SUPPRESSION_WORD,
     .mojom = mojom::JpPosType::kSuppressionWord},
}};

mojom::JapaneseDictionaryEntryPtr MakeEntry(
    chromeos_input::JapaneseDictionary::Entry proto) {
  mojom::JapaneseDictionaryEntryPtr entry =
      mojom::JapaneseDictionaryEntry::New();

  entry->key = proto.key();
  entry->value = proto.value();
  entry->comment = proto.comment();

  const auto& it = std::find_if(
      kPosTypes.begin(), kPosTypes.end(),
      [&proto](const PosTypePair& val) { return val.proto == proto.pos(); });

  if (it != kPosTypes.end()) {
    entry->pos = it->mojom;
  }

  return entry;
}

}  // namespace

mojom::JapaneseDictionaryPtr MakeMojomJapaneseDictionary(
    chromeos_input::JapaneseDictionary proto) {
  mojom::JapaneseDictionaryPtr result = mojom::JapaneseDictionary::New();
  result->id = proto.id();
  result->name = proto.name();
  for (const chromeos_input::JapaneseDictionary::Entry& entry :
       proto.entries()) {
    result->entries.push_back(MakeEntry(entry));
  }
  return result;
}

chromeos_input::JapaneseDictionary::Entry MakeProtoJpDictEntry(
    const mojom::JapaneseDictionaryEntry& mojom_entry) {
  chromeos_input::JapaneseDictionary::Entry result;
  result.set_key(mojom_entry.key);
  result.set_value(mojom_entry.value);
  result.set_comment(mojom_entry.comment);

  const auto& it = std::find_if(kPosTypes.begin(), kPosTypes.end(),
                                [&mojom_entry](const PosTypePair& val) {
                                  return val.mojom == mojom_entry.pos;
                                });

  if (it != kPosTypes.end()) {
    result.set_pos(it->proto);
  }

  return result;
}

}  // namespace ash::ime
