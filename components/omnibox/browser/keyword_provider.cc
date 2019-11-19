// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/keyword_provider.h"

#include <algorithm>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Helper functor for Start(), for sorting keyword matches by quality.
class CompareQuality {
 public:
  // A keyword is of higher quality when a greater fraction of the important
  // part of it has been typed, that is, when the meaningful keyword length is
  // shorter.
  //
  // TODO(pkasting): Most recent and most frequent keywords are probably
  // better rankings than the fraction of the keyword typed.  We should
  // always put any exact matches first no matter what, since the code in
  // Start() assumes this (and it makes sense).
  bool operator()(
      const TemplateURLService::TURLAndMeaningfulLength t_url_match1,
      const TemplateURLService::TURLAndMeaningfulLength t_url_match2) const {
    return t_url_match1.second < t_url_match2.second;
  }
};

// Helper for KeywordProvider::Start(), for ending keyword mode unless
// explicitly told otherwise.
class ScopedEndExtensionKeywordMode {
 public:
  explicit ScopedEndExtensionKeywordMode(KeywordExtensionsDelegate* delegate);
  ~ScopedEndExtensionKeywordMode();

  void StayInKeywordMode();

 private:
  KeywordExtensionsDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEndExtensionKeywordMode);
};

ScopedEndExtensionKeywordMode::ScopedEndExtensionKeywordMode(
    KeywordExtensionsDelegate* delegate)
    : delegate_(delegate) {
}

ScopedEndExtensionKeywordMode::~ScopedEndExtensionKeywordMode() {
  if (delegate_)
    delegate_->MaybeEndExtensionKeywordMode();
}

void ScopedEndExtensionKeywordMode::StayInKeywordMode() {
  delegate_ = nullptr;
}

}  // namespace

KeywordProvider::KeywordProvider(AutocompleteProviderClient* client,
                                 AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_KEYWORD),
      listener_(listener),
      model_(client->GetTemplateURLService()),
      extensions_delegate_(client->GetKeywordExtensionsDelegate(this)) {}

// static
base::string16 KeywordProvider::SplitKeywordFromInput(
    const base::string16& input,
    bool trim_leading_whitespace,
    base::string16* remaining_input) {
  // Find end of first token.  The AutocompleteController has trimmed leading
  // whitespace, so we need not skip over that.
  const size_t first_white(input.find_first_of(base::kWhitespaceUTF16));
  DCHECK_NE(0U, first_white);
  if (first_white == base::string16::npos)
    return input;  // Only one token provided.

  // Set |remaining_input| to everything after the first token.
  DCHECK(remaining_input != nullptr);
  const size_t remaining_start = trim_leading_whitespace ?
      input.find_first_not_of(base::kWhitespaceUTF16, first_white) :
      first_white + 1;

  if (remaining_start < input.length())
    remaining_input->assign(input.begin() + remaining_start, input.end());

  // Return first token as keyword.
  return input.substr(0, first_white);
}

// static
base::string16 KeywordProvider::SplitReplacementStringFromInput(
    const base::string16& input,
    bool trim_leading_whitespace) {
  // The input may contain leading whitespace, strip it.
  base::string16 trimmed_input;
  base::TrimWhitespace(input, base::TRIM_LEADING, &trimmed_input);

  // And extract the replacement string.
  base::string16 remaining_input;
  SplitKeywordFromInput(trimmed_input, trim_leading_whitespace,
      &remaining_input);
  return remaining_input;
}

// static
const TemplateURL* KeywordProvider::GetSubstitutingTemplateURLForInput(
    TemplateURLService* model,
    AutocompleteInput* input) {
  if (!input->allow_exact_keyword_match())
    return nullptr;

  DCHECK(model);
  base::string16 keyword, remaining_input;
  if (!ExtractKeywordFromInput(*input, model, &keyword, &remaining_input))
    return nullptr;

  const TemplateURL* template_url = model->GetTemplateURLForKeyword(keyword);
  if (template_url &&
      template_url->SupportsReplacement(model->search_terms_data())) {
    // Adjust cursor position iff it was set before, otherwise leave it as is.
    size_t cursor_position = base::string16::npos;
    // The adjustment assumes that the keyword was stripped from the beginning
    // of the original input.
    if (input->cursor_position() != base::string16::npos &&
        !remaining_input.empty() &&
        base::EndsWith(input->text(), remaining_input,
                       base::CompareCase::SENSITIVE)) {
      int offset = input->text().length() - input->cursor_position();
      // The cursor should never be past the last character or before the
      // first character.
      DCHECK_GE(offset, 0);
      DCHECK_LE(offset, static_cast<int>(input->text().length()));
      if (offset <= 0) {
        // Normalize the cursor to be exactly after the last character.
        cursor_position = remaining_input.length();
      } else {
        // If somehow the cursor was before the remaining text, set it to 0,
        // otherwise adjust it relative to the remaining text.
        cursor_position = offset > static_cast<int>(remaining_input.length()) ?
            0u : remaining_input.length() - offset;
      }
    }
    input->UpdateText(remaining_input, cursor_position, input->parts());
    return template_url;
  }

  return nullptr;
}

base::string16 KeywordProvider::GetKeywordForText(
    const base::string16& text) const {
  TemplateURLService* url_service = GetTemplateURLService();
  if (!url_service)
    return base::string16();

  const base::string16 keyword(CleanUserInputKeyword(url_service, text));

  if (keyword.empty())
    return keyword;

  // Don't provide a keyword if it doesn't support replacement.
  const TemplateURL* const template_url =
      url_service->GetTemplateURLForKeyword(keyword);
  if (!template_url ||
      !template_url->SupportsReplacement(url_service->search_terms_data()))
    return base::string16();

  // Don't provide a keyword for inactive/disabled extension keywords.
  if ((template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) &&
      extensions_delegate_ &&
      !extensions_delegate_->IsEnabledExtension(template_url->GetExtensionId()))
    return base::string16();

  return keyword;
}

AutocompleteMatch KeywordProvider::CreateVerbatimMatch(
    const base::string16& text,
    const base::string16& keyword,
    const AutocompleteInput& input) {
  // A verbatim match is allowed to be the default match when appropriate.
  return CreateAutocompleteMatch(
      GetTemplateURLService()->GetTemplateURLForKeyword(keyword),
      keyword.length(), input, keyword.length(),
      SplitReplacementStringFromInput(text, true),
      input.allow_exact_keyword_match(), 0, false);
}

void KeywordProvider::DeleteMatch(const AutocompleteMatch& match) {
  const base::string16& suggestion_text = match.contents;

  const auto pred = [&match](const AutocompleteMatch& i) {
    return i.keyword == match.keyword &&
           i.fill_into_edit == match.fill_into_edit;
  };
  base::EraseIf(matches_, pred);

  base::string16 keyword, remaining_input;
  if (!ExtractKeywordFromInput(
          keyword_input_, GetTemplateURLService(), &keyword, &remaining_input))
    return;
  const TemplateURL* const template_url =
      GetTemplateURLService()->GetTemplateURLForKeyword(keyword);

  if ((template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) &&
      extensions_delegate_ &&
      extensions_delegate_->IsEnabledExtension(
          template_url->GetExtensionId())) {
    extensions_delegate_->DeleteSuggestion(template_url, suggestion_text);
  }
}

void KeywordProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  TRACE_EVENT0("omnibox", "KeywordProvider::Start");
  // This object ensures we end keyword mode if we exit the function without
  // toggling keyword mode to on.
  ScopedEndExtensionKeywordMode keyword_mode_toggle(extensions_delegate_.get());

  matches_.clear();

  if (!minimal_changes) {
    done_ = true;

    // Input has changed. Increment the input ID so that we can discard any
    // stale extension suggestions that may be incoming.
    if (extensions_delegate_)
      extensions_delegate_->IncrementInputId();
  }

  if (input.from_omnibox_focus())
    return;

  GetTemplateURLService();
  DCHECK(model_);
  // Split user input into a keyword and some query input.
  //
  // We want to suggest keywords even when users have started typing URLs, on
  // the assumption that they might not realize they no longer need to go to a
  // site to be able to search it.  So we call CleanUserInputKeyword() to strip
  // any initial scheme and/or "www.".  NOTE: Any heuristics or UI used to
  // automatically/manually create keywords will need to be in sync with
  // whatever we do here!
  //
  // TODO(pkasting): http://crbug/347744 If someday we remember usage frequency
  // for keywords, we might suggest keywords that haven't even been partially
  // typed, if the user uses them enough and isn't obviously typing something
  // else.  In this case we'd consider all input here to be query input.
  base::string16 keyword, remaining_input;
  if (!ExtractKeywordFromInput(input, model_, &keyword,
                               &remaining_input))
    return;

  keyword_input_ = input;

  // Get the best matches for this keyword.
  //
  // NOTE: We could cache the previous keywords and reuse them here in the
  // |minimal_changes| case, but since we'd still have to recalculate their
  // relevances and we can just recreate the results synchronously anyway, we
  // don't bother.
  TemplateURLService::TURLsAndMeaningfulLengths matches;
  model_->AddMatchingKeywords(
      keyword, !remaining_input.empty(), &matches);
  if (!OmniboxFieldTrial::KeywordRequiresPrefixMatch()) {
    model_->AddMatchingDomainKeywords(
        keyword, !remaining_input.empty(), &matches);
  }

  for (auto i(matches.begin()); i != matches.end();) {
    const TemplateURL* template_url = i->first;

    // Prune any extension keywords that are disallowed in incognito mode (if
    // we're incognito), or disabled.
    if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION &&
        extensions_delegate_ &&
        !extensions_delegate_->IsEnabledExtension(
            template_url->GetExtensionId())) {
      i = matches.erase(i);
      continue;
    }

    // Prune any substituting keywords if there is no substitution.
    if (template_url->SupportsReplacement(
            model_->search_terms_data()) &&
        remaining_input.empty() &&
        !input.allow_exact_keyword_match()) {
      i = matches.erase(i);
      continue;
    }

    ++i;
  }
  if (matches.empty())
    return;
  std::sort(matches.begin(), matches.end(), CompareQuality());

  // Limit to one exact or three inexact matches, and mark them up for display
  // in the autocomplete popup.
  // Any exact match is going to be the highest quality match, and thus at the
  // front of our vector.
  if (matches.front().first->keyword() == keyword) {
    const TemplateURL* template_url = matches.front().first;
    const size_t meaningful_keyword_length = matches.front().second;
    const bool is_extension_keyword =
        template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;

    // Only create an exact match if |remaining_input| is empty or if
    // this is an extension keyword.  If |remaining_input| is a
    // non-empty non-extension keyword (i.e., a regular keyword that
    // supports replacement and that has extra text following it),
    // then SearchProvider creates the exact (a.k.a. verbatim) match.
    if (!remaining_input.empty() && !is_extension_keyword)
      return;

    // TODO(pkasting): We should probably check that if the user explicitly
    // typed a scheme, that scheme matches the one in |template_url|.

    // When creating an exact match (either for the keyword itself, no
    // remaining query or an extension keyword, possibly with remaining
    // input), allow the match to be the default match when appropriate.
    // For exactly-typed non-substituting keywords, it's always appropriate.
    matches_.push_back(CreateAutocompleteMatch(
        template_url, meaningful_keyword_length, input, keyword.length(),
        remaining_input,
        input.allow_exact_keyword_match() ||
            !template_url->SupportsReplacement(model_->search_terms_data()),
        -1, false));

    // Having extension-provided suggestions appear outside keyword mode can
    // be surprising, so only query for suggestions when in keyword mode.
    if (is_extension_keyword && extensions_delegate_ &&
        input.allow_exact_keyword_match()) {
      if (extensions_delegate_->Start(input, minimal_changes, template_url,
                                      remaining_input))
        keyword_mode_toggle.StayInKeywordMode();
    }
  } else {
    for (TemplateURLService::TURLsAndMeaningfulLengths::const_iterator i(
             matches.begin());
         (i != matches.end()) && (matches_.size() < provider_max_matches_);
         ++i) {
      // Skip keywords that we've already added.  It's possible we may have
      // retrieved the same keyword twice.  For example, the keyword
      // "abc.abc.com" may be retrieved for the input "abc" from the full
      // keyword matching and the domain matching passes.
      ACMatches::const_iterator duplicate = std::find_if(
          matches_.begin(), matches_.end(),
          [&i] (const AutocompleteMatch& m) {
            return m.keyword == i->first->keyword();
          });
      if (duplicate == matches_.end()) {
        matches_.push_back(CreateAutocompleteMatch(
            i->first, i->second, input, keyword.length(), remaining_input,
            false, -1, false));
      }
    }
  }
}

void KeywordProvider::Stop(bool clear_cached_results,
                           bool due_to_user_inactivity) {
  done_ = true;
  // Only end an extension's request if the user did something to explicitly
  // cancel it; mere inactivity shouldn't terminate long-running extension
  // operations since the user likely explicitly requested them.
  if (extensions_delegate_ && !due_to_user_inactivity)
    extensions_delegate_->MaybeEndExtensionKeywordMode();
}

KeywordProvider::~KeywordProvider() {}

// static
bool KeywordProvider::ExtractKeywordFromInput(
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service,
    base::string16* keyword,
    base::string16* remaining_input) {
  if ((input.type() == metrics::OmniboxInputType::EMPTY))
    return false;

  DCHECK(template_url_service);
  *keyword = CleanUserInputKeyword(
      template_url_service,
      SplitKeywordFromInput(input.text(), true, remaining_input));
  return !keyword->empty();
}

// static
int KeywordProvider::CalculateRelevance(metrics::OmniboxInputType type,
                                        bool complete,
                                        bool sufficiently_complete,
                                        bool supports_replacement,
                                        bool prefer_keyword,
                                        bool allow_exact_keyword_match) {
  if (!complete) {
    const int sufficiently_complete_score =
        OmniboxFieldTrial::KeywordScoreForSufficientlyCompleteMatch();
    // If we have a special score to apply for sufficiently-complete matches,
    // do so.
    if (sufficiently_complete && (sufficiently_complete_score > -1))
      return sufficiently_complete_score;
    return (type == metrics::OmniboxInputType::URL) ? 700 : 450;
  }
  if (!supports_replacement)
    return 1500;
  return SearchProvider::CalculateRelevanceForKeywordVerbatim(
      type, allow_exact_keyword_match, prefer_keyword);
}

AutocompleteMatch KeywordProvider::CreateAutocompleteMatch(
    const TemplateURL* template_url,
    const size_t meaningful_keyword_length,
    const AutocompleteInput& input,
    size_t prefix_length,
    const base::string16& remaining_input,
    bool allowed_to_be_default_match,
    int relevance,
    bool deletable) {
  DCHECK(template_url);
  const bool supports_replacement =
      template_url->url_ref().SupportsReplacement(
          GetTemplateURLService()->search_terms_data());

  // Create an edit entry of "[keyword] [remaining input]".  This is helpful
  // even when [remaining input] is empty, as the user can select the popup
  // choice and immediately begin typing in query input.
  const base::string16& keyword = template_url->keyword();
  const bool keyword_complete = (prefix_length == keyword.length());
  const bool sufficiently_complete =
      (prefix_length >= meaningful_keyword_length);
  if (relevance < 0) {
    relevance =
        CalculateRelevance(input.type(), keyword_complete,
                           sufficiently_complete,
                           // When the user wants keyword matches to take
                           // preference, score them highly regardless of
                           // whether the input provides query text.
                           supports_replacement, input.prefer_keyword(),
                           input.allow_exact_keyword_match());
  }

  AutocompleteMatch match(this, relevance, deletable,
                          supports_replacement
                              ? AutocompleteMatchType::SEARCH_OTHER_ENGINE
                              : AutocompleteMatchType::HISTORY_KEYWORD);
  match.allowed_to_be_default_match = allowed_to_be_default_match;
  match.fill_into_edit = keyword;
  if (!remaining_input.empty() || supports_replacement)
    match.fill_into_edit.push_back(L' ');
  match.fill_into_edit.append(remaining_input);
  // If we wanted to set |result.inline_autocompletion| correctly, we'd need
  // CleanUserInputKeyword() to return the amount of adjustment it's made to
  // the user's input.  Because right now inexact keyword matches can't score
  // more highly than a "what you typed" match from one of the other providers,
  // we just don't bother to do this, and leave inline autocompletion off.

  // Create destination URL and popup entry content by substituting user input
  // into keyword templates.
  FillInURLAndContents(remaining_input, template_url, &match);

  match.keyword = keyword;
  match.from_keyword = true;
  match.transition = ui::PAGE_TRANSITION_KEYWORD;

  return match;
}

void KeywordProvider::FillInURLAndContents(
    const base::string16& remaining_input,
    const TemplateURL* element,
    AutocompleteMatch* match) const {
  DCHECK(!element->short_name().empty());
  const TemplateURLRef& element_ref = element->url_ref();
  DCHECK(element_ref.IsValid(GetTemplateURLService()->search_terms_data()));
  if (remaining_input.empty()) {
    // Allow extension keyword providers to accept empty string input. This is
    // useful to allow extensions to do something in the case where no input is
    // entered.
    if (element_ref.SupportsReplacement(
            GetTemplateURLService()->search_terms_data()) &&
        (element->type() != TemplateURL::OMNIBOX_API_EXTENSION)) {
      // No query input; return a generic, no-destination placeholder.
      match->contents.assign(
          l10n_util::GetStringUTF16(IDS_EMPTY_KEYWORD_VALUE));
      match->contents_class.emplace_back(0, ACMatchClassification::DIM);
    } else {
      // Keyword or extension that has no replacement text (aka a shorthand for
      // a URL).
      match->destination_url = GURL(element->url());
      match->contents.assign(element->short_name());
      if (!element->short_name().empty())
        match->contents_class.emplace_back(0, ACMatchClassification::MATCH);
    }
  } else {
    // Create destination URL by escaping user input and substituting into
    // keyword template URL.  The escaping here handles whitespace in user
    // input, but we rely on later canonicalization functions to do more
    // fixup to make the URL valid if necessary.
    DCHECK(element_ref.SupportsReplacement(
        GetTemplateURLService()->search_terms_data()));
    TemplateURLRef::SearchTermsArgs search_terms_args(remaining_input);
    search_terms_args.append_extra_query_params_from_command_line =
        element == GetTemplateURLService()->GetDefaultSearchProvider();
    match->destination_url = GURL(element_ref.ReplaceSearchTerms(
        search_terms_args, GetTemplateURLService()->search_terms_data()));
    match->contents = remaining_input;
    match->contents_class.emplace_back(0, ACMatchClassification::NONE);
  }
}

TemplateURLService* KeywordProvider::GetTemplateURLService() const {
  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  model_->Load();
  return model_;
}

// static
base::string16 KeywordProvider::CleanUserInputKeyword(
    const TemplateURLService* template_url_service,
    const base::string16& keyword) {
  DCHECK(template_url_service);
  base::string16 result(base::i18n::ToLower(keyword));
  base::TrimWhitespace(result, base::TRIM_ALL, &result);
  // If this keyword is found with no additional cleaning of input, return it.
  if (template_url_service->GetTemplateURLForKeyword(result) != nullptr)
    return result;

  // If keyword is not found, try removing a "http" or "https" scheme if any.
  url::Component scheme_component;
  if (url::ExtractScheme(base::UTF16ToUTF8(result).c_str(),
                         static_cast<int>(result.length()),
                         &scheme_component) &&
      (!result.compare(0, scheme_component.end(),
                       base::ASCIIToUTF16(url::kHttpScheme)) ||
       !result.compare(0, scheme_component.end(),
                       base::ASCIIToUTF16(url::kHttpsScheme)))) {
    // Remove the scheme and the trailing ':'.
    result.erase(0, scheme_component.end() + 1);
    if (template_url_service->GetTemplateURLForKeyword(result) != nullptr)
      return result;
    // Many schemes usually have "//" after them, so strip it too.
    const base::string16 after_scheme(base::ASCIIToUTF16("//"));
    if (result.compare(0, after_scheme.length(), after_scheme) == 0)
      result.erase(0, after_scheme.length());
    if (template_url_service->GetTemplateURLForKeyword(result) != nullptr)
      return result;
  }

  // Remove leading "www.", if any, and again try to find a matching keyword.
  result = url_formatter::StripWWW(result);
  if (template_url_service->GetTemplateURLForKeyword(result) != nullptr)
    return result;

  // Remove trailing "/", if any.
  if (!result.empty() && result.back() == '/')
    result.pop_back();
  return result;
}
