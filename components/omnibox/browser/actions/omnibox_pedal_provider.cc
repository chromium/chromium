// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"

#include <numeric>
#include <string_view>
#include <unordered_map>

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
typedef base::StringTokenizerT<std::u16string, std::u16string::const_iterator>
    StringTokenizer16;

// This is a hard upper bound on the number of tokens that will be processed.
// The value 61 was determined from the original body of translation data,
// but translators gradually increased the length of strings. Each time
// the limit is exceeded, the translation pipeline breaks, so 100 provides
// some cushion; but it shouldn't be unbounded as that would adversely
// affect performance. Pedals are checked on each keystroke in the omnibox.
constexpr size_t kMaxTokens = 100;

// All characters in this string get removed from text before processing.
// U+200F is a RTL marker punctuation character that seems to throw
// off some triggers in 'ar'.
const char16_t kRemoveChars[] = {0x200F, 0};

}  // namespace

size_t EstimateMemoryUsage(scoped_refptr<OmniboxPedal> pedal) {
  // Consider the ref-counted Pedals to be part of the provider's memory usage.
  return pedal->EstimateMemoryUsage();
}

OmniboxPedalProvider::OmniboxPedalProvider(
    AutocompleteProviderClient& client,
    std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals)
    : client_(client),
      pedals_(std::move(pedals)),
      ignore_group_(false, false, 0),
      match_tokens_(kMaxTokens) {
  LoadPedalConcepts();

  // Cull Pedals with incomplete data; they won't trigger if not enabled,
  // but there's no need to keep them in the collection (iterated frequently).
  std::erase_if(pedals_, [](const auto& it) {
    const OmniboxPedal::LabelStrings& labels = it.second->GetLabelStrings();
    return labels.hint.empty() || labels.suggestion_contents.empty() ||
           labels.accessibility_hint.empty() ||
           labels.accessibility_suffix.empty();
  });
}

OmniboxPedalProvider::~OmniboxPedalProvider() = default;

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
  Tokenize(match_tokens_, match_text);
  if (match_tokens_.Size() == 0) {
    return nullptr;
  }

  // Note the ignore group is the only one that does full container
  // element erasure. This is necessary to prevent stop words from
  // breaking meaningful token sequences. For example, in the case
  // "make the most of chrome features", "the" must be fully
  // removed so as to not break detection of sequence "make the most of"
  // where "the" is removed by preprocessing. It becomes
  // "make most of" and would not match sequence "make _ most of"
  // where "the" was merely consumed instead of fully removed.
  if (ignore_group_.EraseMatchesIn(match_tokens_, true) &&
      match_tokens_.Size() == 0) {
    // Only ignored tokens were present, and all tokens were erased. No match.
    return nullptr;
  }

  for (const auto& pedal : pedals_) {
    // This restores link validity after above EraseMatchesIn call and prepares
    // |match_tokens_| for the next check after iteration.
    match_tokens_.ResetLinks();
    if (pedal.second->IsConceptMatch(match_tokens_)) {
      return pedal.second.get();
    }
  }
  return nullptr;
}

OmniboxPedal* OmniboxPedalProvider::FindReadyPedalMatch(
    const AutocompleteInput& input,
    const std::u16string& match_text) {
  OmniboxPedal* const found = FindPedalMatch(match_text);
  if (found == nullptr || !found->IsReadyToTrigger(input, *client_)) {
    return nullptr;
  }

  return found;
}

void OmniboxPedalProvider::Tokenize(OmniboxPedal::TokenSequence& out_tokens,
                                    const std::u16string& text) const {
  // Note that FoldCase (not ToLower) is used here and elsewhere in this code.
  // See base/i18n/case_conversion.h for advice about unicode case handling.
  // FoldCase is equivalent to lower-casing for ASCII/English, but provides
  // more consistent (canonical) handling in other languages as well.
  std::u16string reduced_text = base::i18n::FoldCase(text);
  base::RemoveChars(reduced_text, kRemoveChars, &reduced_text);
  out_tokens.Clear();
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(reduced_text);
    size_t left = 0;
    while (!char_iter.end()) {
      char_iter.Advance();
      size_t right = char_iter.array_pos();
      if (right > left) {
        const auto token = reduced_text.substr(left, right - left);
        const auto iter = dictionary_.find(token);
        if (iter == dictionary_.end() || out_tokens.Size() >= kMaxTokens) {
          // No Pedal can possibly match because we found a token not
          // present in the token dictionary, or the text has too many tokens.
          out_tokens.Clear();
          break;
        } else {
          out_tokens.Add(iter->second);
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
      if (iter == dictionary_.end() || out_tokens.Size() >= kMaxTokens) {
        // No Pedal can possibly match because we found a token not
        // present in the token dictionary, or the text has too many tokens.
        out_tokens.Clear();
        break;
      } else {
        out_tokens.Add(iter->second);
      }
    }
  }
}

void OmniboxPedalProvider::TokenizeAndExpandDictionary(
    OmniboxPedal::TokenSequence& out_tokens,
    const std::u16string& token_sequence_string) {
  out_tokens.Clear();
  if (tokenize_characters_.empty()) {
    // Tokenize on Unicode character boundaries when we have no delimiters.
    base::i18n::UTF16CharIterator char_iter(token_sequence_string);
    size_t left = 0;
    while (!char_iter.end()) {
      char_iter.Advance();
      size_t right = char_iter.array_pos();
      if (right > left) {
        const std::u16string raw_token =
            token_sequence_string.substr(left, right - left);
        const std::u16string token = base::i18n::FoldCase(raw_token);
        const auto iter = dictionary_.find(token);
        if (iter == dictionary_.end()) {
          // Token not in dictionary; expand dictionary.
          out_tokens.Add(dictionary_.size());
          dictionary_.insert({token, dictionary_.size()});
        } else {
          // Token in dictionary; add existing token identifier to sequence.
          out_tokens.Add(iter->second);
        }
        left = right;
      } else {
        break;
      }
    }
  } else {
    // Delimiters will neatly divide the string into tokens.
    StringTokenizer16 tokenizer(token_sequence_string, tokenize_characters_);
    while (tokenizer.GetNext()) {
      std::u16string raw_token = tokenizer.token();
      std::u16string_view trimmed_token =
          base::TrimWhitespace(raw_token, base::TrimPositions::TRIM_ALL);
      std::u16string token = base::i18n::FoldCase(trimmed_token);
      const auto iter = dictionary_.find(token);
      if (iter == dictionary_.end()) {
        // Token not in dictionary; expand dictionary.
        out_tokens.Add(dictionary_.size());
        dictionary_.insert({std::move(token), dictionary_.size()});
      } else {
        // Token in dictionary; add existing token identifier to sequence.
        out_tokens.Add(iter->second);
      }
    }
  }
}

void OmniboxPedalProvider::LoadPedalConcepts() {
  // The locale is a two-letter language code, possibly followed by a dash and
  // country code. English locales include "en", "en-US", and "en-GB" while
  // non-English locales never start with "en".
  const std::string locale = base::i18n::GetConfiguredLocale();
  const std::string language_code = locale.substr(0, 2);

  // According to the pedals localization data, only a few languages
  // were set to tokenize each character, so those are checked directly here.
  // Note, zh-CN was set to tokenize each character but zh-TW was not so the
  // full locale is checked for that exceptional case.
  if (language_code == "ja" || (language_code == "zh" && locale != "zh-TW")) {
    tokenize_characters_ = u"";
  } else {
    tokenize_characters_ = u" -";
  }

  ignore_group_ = LoadSynonymGroupString(
      false, false, l10n_util::GetStringUTF16(IDS_OMNIBOX_PEDALS_IGNORE_GROUP));
  if (tokenize_characters_.empty()) {
    // Translation console sourced data has lots of spaces, but in practice
    // the ignore group doesn't include a single space sequence. Rather than
    // burden l10n with getting this nuance in the data precisely specified,
    // we simply hardcode to ignore spaces. This applies for all languages
    // that don't tokenize on spaces (see `tokenize_characters_` above).
    ignore_group_.AddSynonym(
        OmniboxPedal::TokenSequence(std::vector<int>({dictionary_[u" "]})));
  }
  ignore_group_.SortSynonyms();

  for (auto& entry : pedals_) {
    OmniboxPedal* pedal = entry.second.get();
    OmniboxPedal::TokenSequence verbatim_sequence(0);
    TokenizeAndExpandDictionary(verbatim_sequence,
                                pedal->GetLabelStrings().hint);
    ignore_group_.EraseMatchesIn(verbatim_sequence, true);
    pedal->AddVerbatimSequence(std::move(verbatim_sequence));

    std::vector<OmniboxPedal::SynonymGroupSpec> specs =
        pedal->SpecifySynonymGroups(language_code == "en");
    DCHECK(!specs.empty());
    for (const auto& spec : specs) {
      // Note, group strings are not preprocessed; they are the raw outputs
      // from translators in the localization pipeline, so we need to remove
      // ignore group sequences and validate remaining data. The groups
      // are sorted *after* erasing the ignore group to ensure no synonym
      // token sequences are made shorter than sequences later in the order,
      // which would break an invariant expected by the matching algorithm.
      OmniboxPedal::SynonymGroup group =
          LoadSynonymGroupString(spec.required, spec.match_once,
                                 l10n_util::GetStringUTF16(spec.message_id));
      group.EraseIgnoreGroup(ignore_group_);
      group.SortSynonyms();
      if (group.IsValid()) {
        pedal->AddSynonymGroup(std::move(group));
      }
    }
  }

  // Give all pedals a final chance to override/mutate based on feature flags.
  for (auto& entry : pedals_) {
    entry.second->OnLoaded();
  }
}

OmniboxPedal::SynonymGroup OmniboxPedalProvider::LoadSynonymGroupString(
    bool required,
    bool match_once,
    std::u16string synonyms_csv) {
  base::RemoveChars(synonyms_csv, kRemoveChars, &synonyms_csv);
  OmniboxPedal::SynonymGroup group(required, match_once, 0);
  // Note, 'ar' language uses '،' instead of ',' to delimit synonyms and
  // in some cases the 'ja' language data uses '、' to delimit synonyms.
  StringTokenizer16 tokenizer(synonyms_csv, u",،、");
  while (tokenizer.GetNext()) {
    OmniboxPedal::TokenSequence sequence(0);
    // In some languages where whitespace is significant but not a token
    // delimiter, we want to trim and normalize whitespace that might be
    // added by translators for reading convenience in translation console.
    TokenizeAndExpandDictionary(
        sequence, base::CollapseWhitespace(tokenizer.token(), false));
    // This DCHECK should only trigger in case of extra long translation
    // phrases which would need to be fixed in the translation data itself
    // for best efficiency, or by adjusting `kMaxTokens` if really necessary.
    DCHECK_LE(sequence.Size(), kMaxTokens);
    group.AddSynonym(std::move(sequence));
  }
  return group;
}
