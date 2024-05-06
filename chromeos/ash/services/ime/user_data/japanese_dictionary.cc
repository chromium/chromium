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

constexpr auto kPosTypes = base::MakeFixedFlatMap<JapaneseDictionary::PosType,
                                                  mojom::JpPosType>({
    {JapaneseDictionary::NO_POS, mojom::JpPosType::kNoPos},
    {JapaneseDictionary::NOUN, mojom::JpPosType::kNoun},
    {JapaneseDictionary::ABBREVIATION, mojom::JpPosType::kAbbreviation},
    {JapaneseDictionary::SUGGESTION_ONLY, mojom::JpPosType::kSuggestionOnly},
    {JapaneseDictionary::PROPER_NOUN, mojom::JpPosType::kProperNoun},
    {JapaneseDictionary::PERSONAL_NAME, mojom::JpPosType::kPersonalName},
    {JapaneseDictionary::FAMILY_NAME, mojom::JpPosType::kFamilyName},
    {JapaneseDictionary::FIRST_NAME, mojom::JpPosType::kFirstName},
    {JapaneseDictionary::ORGANIZATION_NAME,
     mojom::JpPosType::kOrganizationName},
    {JapaneseDictionary::PLACE_NAME, mojom::JpPosType::kPlaceName},
    {JapaneseDictionary::SA_IRREGULAR_CONJUGATION_NOUN,
     mojom::JpPosType::kSaIrregularConjugationNoun},
    {JapaneseDictionary::ADJECTIVE_VERBAL_NOUN,
     mojom::JpPosType::kAdjectiveVerbalNoun},
    {JapaneseDictionary::NUMBER, mojom::JpPosType::kNumber},
    {JapaneseDictionary::ALPHABET, mojom::JpPosType::kAlphabet},
    {JapaneseDictionary::SYMBOL, mojom::JpPosType::kSymbol},
    {JapaneseDictionary::EMOTICON, mojom::JpPosType::kEmoticon},
    {JapaneseDictionary::ADVERB, mojom::JpPosType::kAdverb},
    {JapaneseDictionary::PRENOUN_ADJECTIVAL,
     mojom::JpPosType::kPrenounAdjectival},
    {JapaneseDictionary::CONJUNCTION, mojom::JpPosType::kConjunction},
    {JapaneseDictionary::INTERJECTION, mojom::JpPosType::kInterjection},
    {JapaneseDictionary::PREFIX, mojom::JpPosType::kPrefix},
    {JapaneseDictionary::COUNTER_SUFFIX, mojom::JpPosType::kCounterSuffix},
    {JapaneseDictionary::GENERIC_SUFFIX, mojom::JpPosType::kGenericSuffix},
    {JapaneseDictionary::PERSON_NAME_SUFFIX,
     mojom::JpPosType::kPersonNameSuffix},
    {JapaneseDictionary::PLACE_NAME_SUFFIX, mojom::JpPosType::kPlaceNameSuffix},
    {JapaneseDictionary::WA_GROUP1_VERB, mojom::JpPosType::kWaGroup1Verb},
    {JapaneseDictionary::KA_GROUP1_VERB, mojom::JpPosType::kKaGroup1Verb},
    {JapaneseDictionary::SA_GROUP1_VERB, mojom::JpPosType::kSaGroup1Verb},
    {JapaneseDictionary::TA_GROUP1_VERB, mojom::JpPosType::kTaGroup1Verb},
    {JapaneseDictionary::NA_GROUP1_VERB, mojom::JpPosType::kNaGroup1Verb},
    {JapaneseDictionary::MA_GROUP1_VERB, mojom::JpPosType::kMaGroup1Verb},
    {JapaneseDictionary::RA_GROUP1_VERB, mojom::JpPosType::kRaGroup1Verb},
    {JapaneseDictionary::GA_GROUP1_VERB, mojom::JpPosType::kGaGroup1Verb},
    {JapaneseDictionary::BA_GROUP1_VERB, mojom::JpPosType::kBaGroup1Verb},
    {JapaneseDictionary::HA_GROUP1_VERB, mojom::JpPosType::kHaGroup1Verb},
    {JapaneseDictionary::GROUP2_VERB, mojom::JpPosType::kGroup2Verb},
    {JapaneseDictionary::KURU_GROUP3_VERB, mojom::JpPosType::kKuruGroup3Verb},
    {JapaneseDictionary::SURU_GROUP3_VERB, mojom::JpPosType::kSuruGroup3Verb},
    {JapaneseDictionary::ZURU_GROUP3_VERB, mojom::JpPosType::kZuruGroup3Verb},
    {JapaneseDictionary::RU_GROUP3_VERB, mojom::JpPosType::kRuGroup3Verb},
    {JapaneseDictionary::ADJECTIVE, mojom::JpPosType::kAdjective},
    {JapaneseDictionary::SENTENCE_ENDING_PARTICLE,
     mojom::JpPosType::kSentenceEndingParticle},
    {JapaneseDictionary::PUNCTUATION, mojom::JpPosType::kPunctuation},
    {JapaneseDictionary::FREE_STANDING_WORD,
     mojom::JpPosType::kFreeStandingWord},
    {JapaneseDictionary::SUPPRESSION_WORD, mojom::JpPosType::kSuppressionWord},
});

mojom::JapaneseDictionaryEntryPtr MakeEntry(
    chromeos_input::JapaneseDictionary::Entry proto) {
  mojom::JapaneseDictionaryEntryPtr entry =
      mojom::JapaneseDictionaryEntry::New();

  entry->key = proto.key();
  entry->value = proto.value();
  entry->comment = proto.comment();

  if (const auto& it = kPosTypes.find(proto.pos()); it != kPosTypes.end()) {
    entry->pos = it->second;
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

}  // namespace ash::ime
