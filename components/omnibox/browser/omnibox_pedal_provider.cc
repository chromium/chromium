// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/json/json_reader.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"
#include "components/omnibox/resources/grit/omnibox_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
typedef base::StringTokenizerT<base::string16, base::string16::const_iterator>
    StringTokenizer16;

}  // namespace

OmniboxPedalProvider::OmniboxPedalProvider(AutocompleteProviderClient& client)
    : client_(client),
      pedals_(GetPedalImplementations()),
      ignore_group_(false, false, 0) {
  LoadPedalConcepts();
}

OmniboxPedalProvider::~OmniboxPedalProvider() {}

OmniboxPedal* OmniboxPedalProvider::FindPedalMatch(
    const base::string16& match_text) const {
  OmniboxPedal::Tokens match_tokens = Tokenize(match_text);
  if (match_tokens.empty()) {
    return nullptr;
  }
  for (const auto& pedal : pedals_) {
    if (pedal.second->IsTriggerMatch(match_tokens) &&
        pedal.second->IsReadyToTrigger(client_)) {
      return pedal.second.get();
    }
  }
  return nullptr;
}

OmniboxPedal::Tokens OmniboxPedalProvider::Tokenize(
    const base::string16& text) const {
  base::string16 reduced_text = base::i18n::ToLower(text);
  OmniboxPedal::Tokens match_tokens;
  match_tokens.reserve(max_tokens_);
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(&reduced_text);
    int32_t left = 0;
    while (!char_iter.end()) {
      char_iter.Advance();
      int32_t right = char_iter.array_pos();
      if (right > left) {
        const auto token = reduced_text.substr(left, right - left);
        const auto iter = dictionary_.find(token);
        if (iter == dictionary_.end() || match_tokens.size() >= max_tokens_) {
          // No Pedal can possibly match because we found a token not
          // present in the token dictionary, or the text has too many tokens.
          return OmniboxPedal::Tokens();
        } else {
          match_tokens.push_back(iter->second);
        }
        left = right;
      } else {
        break;
      }
    }
  } else {
    // Delimiters will neatly divide the string into tokens.
    StringTokenizer16 tokenizer(reduced_text, tokenize_characters_);
    while (tokenizer.GetNext()) {
      const auto iter = dictionary_.find(tokenizer.token());
      if (iter == dictionary_.end() || match_tokens.size() >= max_tokens_) {
        // No Pedal can possibly match because we found a token not
        // present in the token dictionary, or the text has too many tokens.
        return OmniboxPedal::Tokens();
      } else {
        match_tokens.push_back(iter->second);
      }
    }
  }

  ignore_group_.EraseMatchesIn(&match_tokens);

  return match_tokens;
}

void OmniboxPedalProvider::LoadPedalConcepts() {
  // Load concept data then parse to base::Value in order to construct Pedals.
  std::string uncompressed_data =
      ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
          IDR_OMNIBOX_PEDAL_CONCEPTS);
  const auto concept_data = base::JSONReader::Read(uncompressed_data);

  DCHECK(concept_data);
  DCHECK(concept_data->is_dict());

  const int data_version = concept_data->FindKey("data_version")->GetInt();
  CHECK_EQ(data_version, OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION);

  max_tokens_ = concept_data->FindKey("max_tokens")->GetInt();
  // It is conceivable that some language may need more here, but the goal is
  // to sanity check input since it is trusted and used for vector reserve.
  DCHECK_LT(max_tokens_, size_t{64});

  concept_data->FindKey("tokenize_characters")
      ->GetAsString(&tokenize_characters_);

  const auto& dictionary = concept_data->FindKey("dictionary")->GetList();
  dictionary_.reserve(dictionary.size());
  int id = 0;
  for (const auto& token_value : dictionary) {
    base::string16 token;
    token_value.GetAsString(&token);
    dictionary_.insert({token, id});
    ++id;
  }

  const base::Value* ignore_group_value = concept_data->FindKey("ignore_group");
  DCHECK_NE(ignore_group_value, nullptr);
  ignore_group_ = LoadSynonymGroup(*ignore_group_value);

  for (const auto& pedal_value : concept_data->FindKey("pedals")->GetList()) {
    DCHECK(pedal_value.is_dict());
    const OmniboxPedalId pedal_id =
        static_cast<OmniboxPedalId>(pedal_value.FindKey("id")->GetInt());
    const auto pedal = pedals_.find(pedal_id);
    if (pedal == pedals_.end()) {
      CHECK(false) << "OmniboxPedalId " << static_cast<int>(pedal_id)
                   << " not found. Are all data-referenced implementations "
                      "added to provider?";
    }
    for (const auto& group_value : pedal_value.FindKey("groups")->GetList()) {
      pedal->second->AddSynonymGroup(LoadSynonymGroup(group_value));
    }
  }
}

OmniboxPedal::SynonymGroup OmniboxPedalProvider::LoadSynonymGroup(
    const base::Value& group_value) const {
  DCHECK(group_value.is_dict());
  const bool required = group_value.FindKey("required")->GetBool();
  const bool single = group_value.FindKey("single")->GetBool();
  const auto& synonyms = group_value.FindKey("synonyms")->GetList();
  OmniboxPedal::SynonymGroup synonym_group(required, single, synonyms.size());
  for (const auto& synonyms_value : synonyms) {
    DCHECK(synonyms_value.is_list());
    const auto& synonyms_value_list = synonyms_value.GetList();
    OmniboxPedal::Tokens synonym_all_tokens;
    synonym_all_tokens.reserve(synonyms_value_list.size());
    for (const auto& token_index_value : synonyms_value_list) {
      synonym_all_tokens.push_back(token_index_value.GetInt());
    }
    synonym_group.AddSynonym(std::move(synonym_all_tokens));
  }
  return synonym_group;
}
