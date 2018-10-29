// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/third_party/mozilla/url_parse.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace {

bool IsTrivialClassification(const ACMatchClassifications& classifications) {
  return classifications.empty() ||
      ((classifications.size() == 1) &&
       (classifications.back().style == ACMatchClassification::NONE));
}

// Returns true if one of the |terms_prefixed_by_http_or_https| matches the
// beginning of the URL (sans scheme).  (Recall that
// |terms_prefixed_by_http_or_https|, for the input "http://a b" will be
// ["a"].)  This suggests that the user wants a particular URL with a scheme
// in mind, hence the caller should not consider another URL like this one
// but with a different scheme to be a duplicate.
bool WordMatchesURLContent(
    const std::vector<base::string16>& terms_prefixed_by_http_or_https,
    const GURL& url) {
  size_t prefix_length =
      url.scheme().length() + strlen(url::kStandardSchemeSeparator);
  DCHECK_GE(url.spec().length(), prefix_length);
  const base::string16& formatted_url = url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitNothing,
      net::UnescapeRule::NORMAL, nullptr, nullptr, &prefix_length);
  if (prefix_length == base::string16::npos)
    return false;
  const base::string16& formatted_url_without_scheme =
      formatted_url.substr(prefix_length);
  for (const auto& term : terms_prefixed_by_http_or_https) {
    if (base::StartsWith(formatted_url_without_scheme, term,
                         base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

}  // namespace

// AutocompleteMatch ----------------------------------------------------------

// static
const base::char16 AutocompleteMatch::kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

AutocompleteMatch::AutocompleteMatch()
    : provider(nullptr),
      relevance(0),
      typed_count(-1),
      deletable(false),
      allowed_to_be_default_match(false),
      document_type(DocumentType::NONE),
      swap_contents_and_description(false),
      transition(ui::PAGE_TRANSITION_GENERATED),
      type(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED),
      has_tab_match(false),
      subtype_identifier(0),
      from_previous(false) {}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type)
    : provider(provider),
      relevance(relevance),
      typed_count(-1),
      deletable(deletable),
      allowed_to_be_default_match(false),
      document_type(DocumentType::NONE),
      swap_contents_and_description(false),
      transition(ui::PAGE_TRANSITION_TYPED),
      type(type),
      has_tab_match(false),
      subtype_identifier(0),
      from_previous(false) {}

AutocompleteMatch::AutocompleteMatch(const AutocompleteMatch& match)
    : provider(match.provider),
      relevance(match.relevance),
      typed_count(match.typed_count),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      inline_autocompletion(match.inline_autocompletion),
      allowed_to_be_default_match(match.allowed_to_be_default_match),
      destination_url(match.destination_url),
      stripped_destination_url(match.stripped_destination_url),
      image_dominant_color(match.image_dominant_color),
      image_url(match.image_url),
      document_type(match.document_type),
      contents(match.contents),
      contents_class(match.contents_class),
      description(match.description),
      description_class(match.description_class),
      swap_contents_and_description(match.swap_contents_and_description),
      answer_contents(match.answer_contents),
      answer_type(match.answer_type),
      answer(match.answer),
      transition(match.transition),
      type(match.type),
      has_tab_match(match.has_tab_match),
      subtype_identifier(match.subtype_identifier),
      associated_keyword(match.associated_keyword
                             ? new AutocompleteMatch(*match.associated_keyword)
                             : nullptr),
      keyword(match.keyword),
      pedal(match.pedal),
      from_previous(match.from_previous),
      search_terms_args(
          match.search_terms_args
              ? new TemplateURLRef::SearchTermsArgs(*match.search_terms_args)
              : nullptr),
      additional_info(match.additional_info),
      duplicate_matches(match.duplicate_matches) {}

AutocompleteMatch::~AutocompleteMatch() {
}

AutocompleteMatch& AutocompleteMatch::operator=(
    const AutocompleteMatch& match) {
  if (this == &match)
    return *this;

  provider = match.provider;
  relevance = match.relevance;
  typed_count = match.typed_count;
  deletable = match.deletable;
  fill_into_edit = match.fill_into_edit;
  inline_autocompletion = match.inline_autocompletion;
  allowed_to_be_default_match = match.allowed_to_be_default_match;
  destination_url = match.destination_url;
  stripped_destination_url = match.stripped_destination_url;
  image_dominant_color = match.image_dominant_color;
  image_url = match.image_url;
  document_type = match.document_type;
  contents = match.contents;
  contents_class = match.contents_class;
  description = match.description;
  description_class = match.description_class;
  swap_contents_and_description = match.swap_contents_and_description;
  answer_contents = match.answer_contents;
  answer_type = match.answer_type;
  answer = match.answer;
  transition = match.transition;
  type = match.type;
  has_tab_match = match.has_tab_match;
  subtype_identifier = match.subtype_identifier;
  associated_keyword.reset(
      match.associated_keyword
          ? new AutocompleteMatch(*match.associated_keyword)
          : nullptr);
  keyword = match.keyword;
  pedal = match.pedal;
  from_previous = match.from_previous;
  search_terms_args.reset(
      match.search_terms_args
          ? new TemplateURLRef::SearchTermsArgs(*match.search_terms_args)
          : nullptr);
  additional_info = match.additional_info;
  duplicate_matches = match.duplicate_matches;
  return *this;
}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
// static
const gfx::VectorIcon& AutocompleteMatch::TypeToVectorIcon(
    Type type,
    bool is_bookmark,
    DocumentType document_type) {
  if (is_bookmark)
    return omnibox::kBookmarkIcon;

  switch (type) {
    case Type::URL_WHAT_YOU_TYPED:
    case Type::HISTORY_URL:
    case Type::HISTORY_TITLE:
    case Type::HISTORY_BODY:
    case Type::HISTORY_KEYWORD:
    case Type::NAVSUGGEST:
    case Type::BOOKMARK_TITLE:
    case Type::NAVSUGGEST_PERSONALIZED:
    case Type::CLIPBOARD:
    case Type::PHYSICAL_WEB_DEPRECATED:
    case Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case Type::TAB_SEARCH_DEPRECATED:
      return omnibox::kPageIcon;

    case Type::DOCUMENT_SUGGESTION:
      switch (document_type) {
        case DocumentType::DRIVE_DOCS:
          return omnibox::kDriveDocsIcon;
        case DocumentType::DRIVE_FORMS:
          return omnibox::kDriveFormsIcon;
        case DocumentType::DRIVE_SHEETS:
          return omnibox::kDriveSheetsIcon;
        case DocumentType::DRIVE_SLIDES:
          return omnibox::kDriveSlidesIcon;
        case DocumentType::DRIVE_OTHER:
          return omnibox::kDriveLogoIcon;
        default:
          return omnibox::kPageIcon;
      }

    case Type::SEARCH_WHAT_YOU_TYPED:
    case Type::SEARCH_HISTORY:
    case Type::SEARCH_SUGGEST:
    case Type::SEARCH_SUGGEST_ENTITY:
    case Type::SEARCH_SUGGEST_PERSONALIZED:
    case Type::SEARCH_SUGGEST_PROFILE:
    case Type::SEARCH_OTHER_ENGINE:
    case Type::CONTACT_DEPRECATED:
    case Type::VOICE_SUGGEST:
      return vector_icons::kSearchIcon;

    case Type::EXTENSION_APP_DEPRECATED:
      return omnibox::kExtensionAppIcon;

    case Type::CALCULATOR:
      return omnibox::kCalculatorIcon;

    case Type::SEARCH_SUGGEST_TAIL:
      return omnibox::kBlankIcon;

    case Type::PEDAL:
      return omnibox::kPedalIcon;

    case Type::NUM_TYPES:
      break;
  }
  NOTREACHED();
  static const gfx::VectorIcon dummy = {};
  return dummy;
}
#endif

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& elem1,
                                     const AutocompleteMatch& elem2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  return (elem1.relevance == elem2.relevance) ?
      (elem1.contents < elem2.contents) : (elem1.relevance > elem2.relevance);
}

// static
void AutocompleteMatch::ClassifyMatchInString(
    const base::string16& find_text,
    const base::string16& text,
    int style,
    ACMatchClassifications* classification) {
  ClassifyLocationInString(text.find(find_text), find_text.length(),
                           text.length(), style, classification);
}

// static
void AutocompleteMatch::ClassifyLocationInString(
    size_t match_location,
    size_t match_length,
    size_t overall_length,
    int style,
    ACMatchClassifications* classification) {
  classification->clear();

  // Don't classify anything about an empty string
  // (AutocompleteMatch::Validate() checks this).
  if (overall_length == 0)
    return;

  // Mark pre-match portion of string (if any).
  if (match_location != 0) {
    classification->push_back(ACMatchClassification(0, style));
  }

  // Mark matching portion of string.
  if (match_location == base::string16::npos) {
    // No match, above classification will suffice for whole string.
    return;
  }
  // Classifying an empty match makes no sense and will lead to validation
  // errors later.
  DCHECK_GT(match_length, 0U);
  classification->push_back(ACMatchClassification(match_location,
      (style | ACMatchClassification::MATCH) & ~ACMatchClassification::DIM));

  // Mark post-match portion of string (if any).
  const size_t after_match(match_location + match_length);
  if (after_match < overall_length) {
    classification->push_back(ACMatchClassification(after_match, style));
  }
}

// static
AutocompleteMatch::ACMatchClassifications
    AutocompleteMatch::MergeClassifications(
    const ACMatchClassifications& classifications1,
    const ACMatchClassifications& classifications2) {
  // We must return the empty vector only if both inputs are truly empty.
  // The result of merging an empty vector with a single (0, NONE)
  // classification is the latter one-entry vector.
  if (IsTrivialClassification(classifications1))
    return classifications2.empty() ? classifications1 : classifications2;
  if (IsTrivialClassification(classifications2))
    return classifications1;

  ACMatchClassifications output;
  for (auto i = classifications1.begin(), j = classifications2.begin();
       i != classifications1.end();) {
    AutocompleteMatch::AddLastClassificationIfNecessary(&output,
        std::max(i->offset, j->offset), i->style | j->style);
    const size_t next_i_offset = (i + 1) == classifications1.end() ?
        static_cast<size_t>(-1) : (i + 1)->offset;
    const size_t next_j_offset = (j + 1) == classifications2.end() ?
        static_cast<size_t>(-1) : (j + 1)->offset;
    if (next_i_offset >= next_j_offset)
      ++j;
    if (next_j_offset >= next_i_offset)
      ++i;
  }

  return output;
}

// static
std::string AutocompleteMatch::ClassificationsToString(
    const ACMatchClassifications& classifications) {
  std::string serialized_classifications;
  for (size_t i = 0; i < classifications.size(); ++i) {
    if (i)
      serialized_classifications += ',';
    serialized_classifications +=
        base::NumberToString(classifications[i].offset) + ',' +
        base::IntToString(classifications[i].style);
  }
  return serialized_classifications;
}

// static
ACMatchClassifications AutocompleteMatch::ClassificationsFromString(
    const std::string& serialized_classifications) {
  ACMatchClassifications classifications;
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      serialized_classifications, ",", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  DCHECK(!(tokens.size() & 1));  // The number of tokens should be even.
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int classification_offset = 0;
    int classification_style = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &classification_offset) ||
        !base::StringToInt(tokens[i + 1], &classification_style)) {
      NOTREACHED();
      return classifications;
    }
    classifications.push_back(ACMatchClassification(classification_offset,
                                                    classification_style));
  }
  return classifications;
}

// static
void AutocompleteMatch::AddLastClassificationIfNecessary(
    ACMatchClassifications* classifications,
    size_t offset,
    int style) {
  DCHECK(classifications);
  if (classifications->empty() || classifications->back().style != style) {
    DCHECK(classifications->empty() ||
           (offset > classifications->back().offset));
    classifications->push_back(ACMatchClassification(offset, style));
  }
}

// static
bool AutocompleteMatch::HasMatchStyle(
    const ACMatchClassifications& classifications) {
  for (const auto& it : classifications) {
    if (it.style & AutocompleteMatch::ACMatchClassification::MATCH)
      return true;
  }
  return false;
}

// static
base::string16 AutocompleteMatch::SanitizeString(const base::string16& text) {
  // NOTE: This logic is mirrored by |sanitizeString()| in
  // omnibox_custom_bindings.js.
  base::string16 result;
  base::TrimWhitespace(text, base::TRIM_LEADING, &result);
  base::RemoveChars(result, kInvalidChars, &result);
  return result;
}

// static
bool AutocompleteMatch::IsSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_HISTORY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE ||
         type == AutocompleteMatchType::CALCULATOR ||
         type == AutocompleteMatchType::VOICE_SUGGEST ||
         IsSpecializedSearchType(type);
}

// static
bool AutocompleteMatch::IsSpecializedSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
}

// static
TemplateURL* AutocompleteMatch::GetTemplateURLWithKeyword(
    TemplateURLService* template_url_service,
    const base::string16& keyword,
    const std::string& host) {
  return const_cast<TemplateURL*>(GetTemplateURLWithKeyword(
      static_cast<const TemplateURLService*>(template_url_service), keyword,
      host));
}

// static
const TemplateURL* AutocompleteMatch::GetTemplateURLWithKeyword(
    const TemplateURLService* template_url_service,
    const base::string16& keyword,
    const std::string& host) {
  if (template_url_service == nullptr)
    return nullptr;
  const TemplateURL* template_url =
      keyword.empty() ? nullptr
                      : template_url_service->GetTemplateURLForKeyword(keyword);
  return (template_url || host.empty()) ?
      template_url : template_url_service->GetTemplateURLForHost(host);
}

// static
GURL AutocompleteMatch::GURLToStrippedGURL(
    const GURL& url,
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service,
    const base::string16& keyword,
    const std::string& additional_query_params) {
  if (!url.is_valid())
    return url;

  GURL stripped_destination_url = url;

  // If the destination URL looks like it was generated from a TemplateURL,
  // remove all substitutions other than the search terms.  This allows us
  // to eliminate cases like past search URLs from history that differ only
  // by some obscure query param from each other or from the search/keyword
  // provider matches.
  const TemplateURL* template_url = GetTemplateURLWithKeyword(
      template_url_service, keyword, stripped_destination_url.host());
  if (template_url != nullptr &&
      template_url->SupportsReplacement(
          template_url_service->search_terms_data())) {
    base::string16 search_terms;
    if (template_url->ExtractSearchTermsFromURL(
        stripped_destination_url,
        template_url_service->search_terms_data(),
        &search_terms)) {
      TemplateURLRef::SearchTermsArgs search_terms_args(search_terms);
      if (!additional_query_params.empty())
        search_terms_args.additional_query_params = additional_query_params;
      stripped_destination_url =
          GURL(template_url->url_ref().ReplaceSearchTerms(
              search_terms_args, template_url_service->search_terms_data()));
    }
  }

  // |replacements| keeps all the substitutions we're going to make to
  // from {destination_url} to {stripped_destination_url}.  |need_replacement|
  // is a helper variable that helps us keep track of whether we need
  // to apply the replacement.
  bool needs_replacement = false;
  GURL::Replacements replacements;

  // Remove the www. prefix from the host.
  static const char prefix[] = "www.";
  static const size_t prefix_len = arraysize(prefix) - 1;
  std::string host = stripped_destination_url.host();
  if (host.compare(0, prefix_len, prefix) == 0) {
    replacements.SetHostStr(base::StringPiece(host).substr(prefix_len));
    needs_replacement = true;
  }

  // Replace https protocol with http, as long as the user didn't explicitly
  // specify one of the two.
  if (stripped_destination_url.SchemeIs(url::kHttpsScheme) &&
      (input.terms_prefixed_by_http_or_https().empty() ||
       !WordMatchesURLContent(
           input.terms_prefixed_by_http_or_https(), url))) {
    replacements.SetScheme(url::kHttpScheme,
                           url::Component(0, strlen(url::kHttpScheme)));
    needs_replacement = true;
  }

  if (!input.parts().ref.is_nonempty() && url.has_ref()) {
    replacements.ClearRef();
    needs_replacement = true;
  }

  if (needs_replacement)
    stripped_destination_url = stripped_destination_url.ReplaceComponents(
        replacements);
  return stripped_destination_url;
}

// static
void AutocompleteMatch::GetMatchComponents(
    const GURL& url,
    const std::vector<MatchPosition>& match_positions,
    bool* match_in_scheme,
    bool* match_in_subdomain,
    bool* match_after_host) {
  DCHECK(match_in_scheme);
  DCHECK(match_in_subdomain);
  DCHECK(match_after_host);

  size_t domain_length =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url.host_piece(),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
          .size();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  size_t host_pos = parsed.CountCharactersBefore(url::Parsed::HOST, false);

  // We must add an extra character to exclude the '/' delimiter that prefixes
  // every path. We have to do this because the |include_delimiter| parameter
  // passed to url::Parsed::CountCharactersBefore has no effect for the PATH.
  size_t path_pos = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  bool has_subdomain =
      domain_length > 0 && domain_length < url.host_piece().length();
  // Subtract an extra character from the domain start to exclude the '.'
  // delimiter between subdomain and domain.
  size_t subdomain_end =
      has_subdomain ? host_pos + url.host_piece().length() - domain_length - 1
                    : std::string::npos;

  for (auto& position : match_positions) {
    // Only flag |match_in_scheme| if the match starts at the very beginning.
    if (position.first == 0 && parsed.scheme.is_nonempty())
      *match_in_scheme = true;

    // Subdomain matches must begin before the domain, and end somewhere within
    // the host or later.
    if (has_subdomain && position.first < subdomain_end &&
        position.second > host_pos && parsed.host.is_nonempty()) {
      *match_in_subdomain = true;
    }

    if (position.second > path_pos &&
        (parsed.path.is_nonempty() || parsed.query.is_nonempty() ||
         parsed.ref.is_nonempty())) {
      *match_after_host = true;
    }
  }
}

// static
url_formatter::FormatUrlTypes AutocompleteMatch::GetFormatTypes(
    bool preserve_scheme,
    bool preserve_subdomain,
    bool preserve_after_host) {
  auto format_types = url_formatter::kFormatUrlOmitDefaults;
  if (preserve_scheme) {
    format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  } else {
    format_types |= url_formatter::kFormatUrlOmitHTTPS;
  }

  if (!preserve_subdomain) {
    format_types |= url_formatter::kFormatUrlOmitTrivialSubdomains;
  }

  if (!preserve_after_host &&
      base::FeatureList::IsEnabled(
          omnibox::kUIExperimentElideSuggestionUrlAfterHost)) {
    format_types |= url_formatter::kFormatUrlExperimentalElideAfterHost;
  }

  return format_types;
}

void AutocompleteMatch::ComputeStrippedDestinationURL(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  stripped_destination_url = GURLToStrippedGURL(
      destination_url, input, template_url_service, keyword,
      search_terms_args ? search_terms_args->additional_query_params : "");
}

void AutocompleteMatch::GetKeywordUIState(
    TemplateURLService* template_url_service,
    base::string16* keyword,
    bool* is_keyword_hint) const {
  *is_keyword_hint = associated_keyword != nullptr;
  keyword->assign(*is_keyword_hint ? associated_keyword->keyword :
      GetSubstitutingExplicitlyInvokedKeyword(template_url_service));
}

base::string16 AutocompleteMatch::GetSubstitutingExplicitlyInvokedKeyword(
    TemplateURLService* template_url_service) const {
  if (!ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_KEYWORD) ||
      template_url_service == nullptr) {
    return base::string16();
  }

  const TemplateURL* t_url = GetTemplateURL(template_url_service, false);
  return (t_url &&
          t_url->SupportsReplacement(
              template_url_service->search_terms_data())) ?
      keyword : base::string16();
}

TemplateURL* AutocompleteMatch::GetTemplateURL(
    TemplateURLService* template_url_service,
    bool allow_fallback_to_destination_host) const {
  return GetTemplateURLWithKeyword(
      template_url_service, keyword,
      allow_fallback_to_destination_host ?
          destination_url.host() : std::string());
}

GURL AutocompleteMatch::ImageUrl() const {
  return answer ? answer->image_url() : GURL(image_url);
}

void AutocompleteMatch::ApplyPedal() {
  // TODO(orinj): It may make more sense to start from a clean slate and
  // apply only the bits of state relevant to the Pedal, rather than
  // eliminating parts of an existing match that are no longer useful.

  type = Type::PEDAL;
  destination_url = pedal->GetNavigationUrl();

  // Normally this is computed by the match using a TemplateURLService
  // but Pedal URLs are not typical and unknown, and we don't want them to
  // be deduped, e.g. after stripping a query parameter that may do something
  // meaningful like indicate the viewable scope of a settings page.  So here
  // we keep the URL exactly as the Pedal specifies it.
  stripped_destination_url = destination_url;

  // Note: Always use empty classifications for empty text and non-empty
  // classifications for non-empty text.
  contents = pedal->GetLabelStrings().suggestion_contents;
  contents_class = {ACMatchClassification(0, ACMatchClassification::NONE)};
  description.clear();
  description_class.clear();
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const std::string& value) {
  DCHECK(!property.empty());
  DCHECK(!value.empty());
  additional_info[property] = value;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             int value) {
  RecordAdditionalInfo(property, base::IntToString(value));
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             base::Time value) {
  RecordAdditionalInfo(
      property, base::StringPrintf("%d hours ago",
                                   (base::Time::Now() - value).InHours()));
}

std::string AutocompleteMatch::GetAdditionalInfo(
    const std::string& property) const {
  auto i(additional_info.find(property));
  return (i == additional_info.end()) ? std::string() : i->second;
}

bool AutocompleteMatch::IsVerbatimType() const {
  const bool is_keyword_verbatim_match =
      (type == AutocompleteMatchType::SEARCH_OTHER_ENGINE &&
       provider != nullptr &&
       provider->type() == AutocompleteProvider::TYPE_SEARCH);
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
      type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
      is_keyword_verbatim_match;
}

bool AutocompleteMatch::SupportsDeletion() const {
  if (deletable)
    return true;

  for (auto it(duplicate_matches.begin()); it != duplicate_matches.end();
       ++it) {
    if (it->deletable)
      return true;
  }
  return false;
}

AutocompleteMatch
AutocompleteMatch::GetMatchWithContentsAndDescriptionPossiblySwapped() const {
  AutocompleteMatch copy(*this);
  if (copy.swap_contents_and_description) {
    std::swap(copy.contents, copy.description);
    std::swap(copy.contents_class, copy.description_class);
    // Clear bit to prevent accidentally performing the swap again.
    copy.swap_contents_and_description = false;
  }
  return copy;
}

void AutocompleteMatch::InlineTailPrefix(const base::string16& common_prefix) {
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    contents = common_prefix + contents;
    // Shift existing styles.
    for (ACMatchClassification& classification : contents_class)
      classification.offset += common_prefix.size();
    // Prefix with invisible text.
    contents_class.insert(
        contents_class.begin(),
        ACMatchClassification(0, ACMatchClassification::INVISIBLE));
  }
}

size_t AutocompleteMatch::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(fill_into_edit);
  res += base::trace_event::EstimateMemoryUsage(inline_autocompletion);
  res += base::trace_event::EstimateMemoryUsage(destination_url);
  res += base::trace_event::EstimateMemoryUsage(stripped_destination_url);
  res += base::trace_event::EstimateMemoryUsage(image_dominant_color);
  res += base::trace_event::EstimateMemoryUsage(image_url);
  res += base::trace_event::EstimateMemoryUsage(contents);
  res += base::trace_event::EstimateMemoryUsage(contents_class);
  res += base::trace_event::EstimateMemoryUsage(description);
  res += base::trace_event::EstimateMemoryUsage(description_class);
  res += base::trace_event::EstimateMemoryUsage(answer_contents);
  res += base::trace_event::EstimateMemoryUsage(answer_type);
  res += sizeof(int);
  if (answer)
    res += base::trace_event::EstimateMemoryUsage(answer.value());
  else
    res += sizeof(SuggestionAnswer);
  res += base::trace_event::EstimateMemoryUsage(associated_keyword);
  res += base::trace_event::EstimateMemoryUsage(keyword);
  res += base::trace_event::EstimateMemoryUsage(search_terms_args);
  res += base::trace_event::EstimateMemoryUsage(additional_info);
  res += base::trace_event::EstimateMemoryUsage(duplicate_matches);

  return res;
}

bool AutocompleteMatch::IsExceptedFromLineReversal() const {
  return !!answer && answer->type() == SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
}

bool AutocompleteMatch::ShouldShowTabMatch() const {
  // TODO(orinj): If side button Pedal presentation mode is not kept,
  // the simpler logic (with no pedal checks) can be restored, and if it is
  // kept then some minor refactoring (or at least renaming) is in order.
  return (has_tab_match && !associated_keyword) ||
         (pedal && pedal->ShouldPresentButton());
}

#if DCHECK_IS_ON()
void AutocompleteMatch::Validate() const {
  ValidateClassifications(contents, contents_class);
  ValidateClassifications(description, description_class);
}

void AutocompleteMatch::ValidateClassifications(
    const base::string16& text,
    const ACMatchClassifications& classifications) const {
  if (text.empty()) {
    DCHECK(classifications.empty());
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(!classifications.empty()) << "No classification for \"" << text << '"';
  DCHECK_EQ(0U, classifications[0].offset)
      << "Classification misses beginning for \"" << text << '"';
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (auto i(classifications.begin() + 1); i != classifications.end(); ++i) {
    const char* provider_name = provider ? provider->GetName() : "None";
    DCHECK_GT(i->offset, last_offset)
        << " Classification for \"" << text << "\" with offset of " << i->offset
        << " is unsorted in relation to last offset of " << last_offset
        << ". Provider: " << provider_name << ".";
    DCHECK_LT(i->offset, text.length())
        << " Classification of [" << i->offset << "," << text.length()
        << "] is out of bounds for \"" << text << "\". Provider: "
        << provider_name << ".";
    last_offset = i->offset;
  }
}
#endif  // DCHECK_IS_ON()
