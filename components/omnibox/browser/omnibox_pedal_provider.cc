// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include <numeric>

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
typedef base::StringTokenizerT<std::u16string, std::u16string::const_iterator>
    StringTokenizer16;

}  // namespace

OmniboxPedalProvider::OmniboxPedalProvider(AutocompleteProviderClient& client,
                                           bool with_branding)
    : client_(client),
      pedals_(GetPedalImplementations(with_branding)),
      ignore_group_(false, false, 0) {
  LoadPedalConcepts();

  // Cull Pedals with incomplete data; they won't trigger if not enabled,
  // but there's no need to keep them in the collection (iterated frequently).
  base::EraseIf(pedals_, [](const auto& it) {
    const OmniboxPedal::LabelStrings& labels = it.second->GetLabelStrings();
    return labels.hint.empty() || labels.suggestion_contents.empty() ||
           labels.accessibility_hint.empty() ||
           labels.accessibility_suffix.empty();
  });
}

OmniboxPedalProvider::~OmniboxPedalProvider() {}

void OmniboxPedalProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  // Note: SEARCH is used here because the suggestions that Pedals attach to are
  // almost exclusively coming from search suggestions (they could in theory
  // attach to others if the match content were a concept match, but in practice
  // only search suggestions have the relevant text). PEDAL is not used because
  // Pedals are not themselves suggestions produced by an autocomplete provider.
  // This may change. See http://cl/327103601 for context and discussion.
  new_entry.set_provider(metrics::OmniboxEventProto::SEARCH);
  new_entry.set_provider_done(true);

  if (field_trial_triggered_ || field_trial_triggered_in_session_) {
    std::vector<uint32_t> field_trial_hashes;
    OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
    for (uint32_t trial : field_trial_hashes) {
      if (field_trial_triggered_)
        new_entry.mutable_field_trial_triggered()->Add(trial);
      if (field_trial_triggered_in_session_)
        new_entry.mutable_field_trial_triggered_in_session()->Add(trial);
    }
  }
}

void OmniboxPedalProvider::ResetSession() {
  field_trial_triggered_in_session_ = false;
  field_trial_triggered_ = false;
}

size_t OmniboxPedalProvider::EstimateMemoryUsage() const {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(dictionary_);
  total += base::trace_event::EstimateMemoryUsage(ignore_group_);
  total += base::trace_event::EstimateMemoryUsage(pedals_);
  total += base::trace_event::EstimateMemoryUsage(tokenize_characters_);
  return total;
}

OmniboxPedal* OmniboxPedalProvider::FindPedalMatch(
    const std::u16string& match_text) {
  OmniboxPedal::Tokens match_tokens = Tokenize(match_text);
  if (match_tokens.empty()) {
    return nullptr;
  }
  for (const auto& pedal : pedals_) {
    if (pedal.second->IsTriggerMatch(match_tokens)) {
      return pedal.second.get();
    }
  }
  return nullptr;
}

OmniboxPedal* OmniboxPedalProvider::FindReadyPedalMatch(
    const AutocompleteInput& input,
    const std::u16string& match_text) {
  OmniboxPedal* const found = FindPedalMatch(match_text);
  if (found == nullptr || !found->IsReadyToTrigger(input, client_)) {
    return nullptr;
  }

  field_trial_triggered_ = true;
  field_trial_triggered_in_session_ = true;

  // Some users may be in a counterfactual study arm in which the pedal button
  // is not attached to the suggestion, even though it triggered.
  if (base::GetFieldTrialParamByFeatureAsBool(
          omnibox::kOmniboxPedalSuggestions,
          "PedalSuggestionsCounterfactualArm", false)) {
    return nullptr;
  }

  return found;
}

OmniboxPedal::Tokens OmniboxPedalProvider::Tokenize(
    const std::u16string& text) const {
  std::u16string reduced_text = base::i18n::ToLower(text);
  OmniboxPedal::Tokens match_tokens;
  match_tokens.reserve(max_tokens_);
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(reduced_text);
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

  if (concept_data->FindKey("tokenize_each_character")->GetBool()) {
    tokenize_characters_ = u"";
  } else {
    tokenize_characters_ = u" -";
  }

  const auto& dictionary = concept_data->FindKey("dictionary")->GetList();
  dictionary_.reserve(dictionary.size());
  int id = 0;
  for (const auto& token_value : dictionary) {
    std::u16string token;
    token_value.GetAsString(&token);
    dictionary_.insert({token, id});
    ++id;
  }

  const base::Value* ignore_group_value = concept_data->FindKey("ignore_group");
  DCHECK_NE(ignore_group_value, nullptr);
  ignore_group_ = LoadSynonymGroup(*ignore_group_value);

  for (const auto& pedal_value : concept_data->FindKey("pedals")->GetList()) {
    DCHECK(pedal_value.is_dict());
    const int id = pedal_value.FindIntKey("id").value();
    // These IDs are the first and last for batch 2.
    if (id >= static_cast<int>(OmniboxPedalId::RUN_CHROME_SAFETY_CHECK) &&
        id <= static_cast<int>(OmniboxPedalId::CHANGE_GOOGLE_PASSWORD) &&
        !OmniboxFieldTrial::IsPedalsBatch2Enabled()) {
      continue;
    }
    const auto pedal_iter = pedals_.find(static_cast<OmniboxPedalId>(id));
    if (pedal_iter == pedals_.end()) {
      // Data may exist for Pedals that are intentionally not registered; skip.
      continue;
    }
    const base::Value* ui_strings =
        pedal_value.FindDictKey("omnibox_ui_strings");
    if (ui_strings) {
      pedal_iter->second->SetLabelStrings(*ui_strings);
    }
    const std::string* url = pedal_value.FindStringKey("url");
    if (!url->empty()) {
      pedal_iter->second->SetNavigationUrl(GURL(*url));
    }
    for (const auto& group_value : pedal_value.FindKey("groups")->GetList()) {
      pedal_iter->second->AddSynonymGroup(LoadSynonymGroup(group_value));
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
