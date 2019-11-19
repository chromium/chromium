// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
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
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
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

// static
const char* const AutocompleteMatch::kDocumentTypeStrings[]{
    "none",        "drive_docs", "drive_forms", "drive_sheets", "drive_slides",
    "drive_image", "drive_pdf",  "drive_video", "drive_other"};

static_assert(
    base::size(AutocompleteMatch::kDocumentTypeStrings) ==
        static_cast<int>(AutocompleteMatch::DocumentType::DOCUMENT_TYPE_SIZE),
    "Sizes of AutocompleteMatch::kDocumentTypeStrings and "
    "AutocompleteMatch::DocumentType don't match.");

// static
const char* AutocompleteMatch::DocumentTypeString(DocumentType type) {
  return kDocumentTypeStrings[static_cast<int>(type)];
}

// AutocompleteMatch ----------------------------------------------------------

// static
const base::char16 AutocompleteMatch::kInvalidChars[] = {
  '\n', '\r', '\t',
  0x2028,  // Line separator
  0x2029,  // Paragraph separator
  0
};

// static
const char AutocompleteMatch::kEllipsis[] = "... ";

// static
size_t AutocompleteMatch::next_family_id_;

AutocompleteMatch::AutocompleteMatch()
    : transition(ui::PAGE_TRANSITION_GENERATED) {}

AutocompleteMatch::AutocompleteMatch(AutocompleteProvider* provider,
                                     int relevance,
                                     bool deletable,
                                     Type type)
    : provider(provider),
      relevance(relevance),
      deletable(deletable),
      transition(ui::PAGE_TRANSITION_TYPED),
      type(type) {}

AutocompleteMatch::AutocompleteMatch(const AutocompleteMatch& match)
    : provider(match.provider),
      relevance(match.relevance),
      subrelevance(match.subrelevance),
      typed_count(match.typed_count),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      inline_autocompletion(match.inline_autocompletion),
      allowed_to_be_default_match(match.allowed_to_be_default_match),
      is_navigational_title_match(match.is_navigational_title_match),
      destination_url(match.destination_url),
      stripped_destination_url(match.stripped_destination_url),
      image_dominant_color(match.image_dominant_color),
      image_url(match.image_url),
      document_type(match.document_type),
      tail_suggest_common_prefix(match.tail_suggest_common_prefix),
      contents(match.contents),
      contents_class(match.contents_class),
      description(match.description),
      description_class(match.description_class),
      swap_contents_and_description(match.swap_contents_and_description),
      answer(match.answer),
      transition(match.transition),
      type(match.type),
      parent_type(match.parent_type),
      has_tab_match(match.has_tab_match),
      subtype_identifier(match.subtype_identifier),
      associated_keyword(match.associated_keyword
                             ? new AutocompleteMatch(*match.associated_keyword)
                             : nullptr),
      keyword(match.keyword),
      from_keyword(match.from_keyword),
      pedal(match.pedal),
      from_previous(match.from_previous),
      search_terms_args(
          match.search_terms_args
              ? new TemplateURLRef::SearchTermsArgs(*match.search_terms_args)
              : nullptr),
      post_content(match.post_content
                       ? new TemplateURLRef::PostContent(*match.post_content)
                       : nullptr),
      additional_info(match.additional_info),
      duplicate_matches(match.duplicate_matches) {}

AutocompleteMatch::AutocompleteMatch(AutocompleteMatch&& match) noexcept =
    default;

AutocompleteMatch::~AutocompleteMatch() {
}

AutocompleteMatch& AutocompleteMatch::operator=(
    const AutocompleteMatch& match) {
  if (this == &match)
    return *this;

  provider = match.provider;
  relevance = match.relevance;
  subrelevance = match.subrelevance;
  typed_count = match.typed_count;
  deletable = match.deletable;
  fill_into_edit = match.fill_into_edit;
  inline_autocompletion = match.inline_autocompletion;
  allowed_to_be_default_match = match.allowed_to_be_default_match;
  is_navigational_title_match = match.is_navigational_title_match;
  destination_url = match.destination_url;
  stripped_destination_url = match.stripped_destination_url;
  image_dominant_color = match.image_dominant_color;
  image_url = match.image_url;
  document_type = match.document_type;
  tail_suggest_common_prefix = match.tail_suggest_common_prefix;
  contents = match.contents;
  contents_class = match.contents_class;
  description = match.description;
  description_class = match.description_class;
  swap_contents_and_description = match.swap_contents_and_description;
  answer = match.answer;
  transition = match.transition;
  type = match.type;
  parent_type = match.parent_type;
  has_tab_match = match.has_tab_match;
  subtype_identifier = match.subtype_identifier;
  associated_keyword.reset(
      match.associated_keyword
          ? new AutocompleteMatch(*match.associated_keyword)
          : nullptr);
  keyword = match.keyword;
  from_keyword = match.from_keyword;
  pedal = match.pedal;
  from_previous = match.from_previous;
  search_terms_args.reset(
      match.search_terms_args
          ? new TemplateURLRef::SearchTermsArgs(*match.search_terms_args)
          : nullptr);
  post_content.reset(match.post_content
                         ? new TemplateURLRef::PostContent(*match.post_content)
                         : nullptr);
  additional_info = match.additional_info;
  duplicate_matches = match.duplicate_matches;
  return *this;
}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
const gfx::VectorIcon& AutocompleteMatch::GetVectorIcon(
    bool is_bookmark) const {
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
    case Type::CLIPBOARD_URL:
    case Type::PHYSICAL_WEB_DEPRECATED:
    case Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case Type::TAB_SEARCH_DEPRECATED:
      return omnibox::kPageIcon;

    case Type::SEARCH_WHAT_YOU_TYPED:
    case Type::SEARCH_SUGGEST:
    case Type::SEARCH_SUGGEST_ENTITY:
    case Type::SEARCH_SUGGEST_PROFILE:
    case Type::SEARCH_OTHER_ENGINE:
    case Type::CONTACT_DEPRECATED:
    case Type::VOICE_SUGGEST:
    case Type::CLIPBOARD_TEXT:
    case Type::CLIPBOARD_IMAGE:
      return vector_icons::kSearchIcon;

    case Type::SEARCH_HISTORY:
    case Type::SEARCH_SUGGEST_PERSONALIZED:
      return omnibox::kClockIcon;

    case Type::EXTENSION_APP_DEPRECATED:
      return omnibox::kExtensionAppIcon;

    case Type::CALCULATOR:
      return omnibox::kCalculatorIcon;

    case Type::SEARCH_SUGGEST_TAIL:
      return omnibox::kBlankIcon;

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
        case DocumentType::DRIVE_IMAGE:
          return omnibox::kDriveImageIcon;
        case DocumentType::DRIVE_PDF:
          return omnibox::kDrivePdfIcon;
        case DocumentType::DRIVE_VIDEO:
          return omnibox::kDriveVideoIcon;
        case DocumentType::DRIVE_OTHER:
          return omnibox::kDriveLogoIcon;
        default:
          return omnibox::kPageIcon;
      }

    case Type::PEDAL:
      return (pedal ? pedal->GetVectorIcon() : omnibox::kPedalIcon);

    case Type::NUM_TYPES:
      NOTREACHED();
      static const gfx::VectorIcon dummy = {};
      return dummy;
  }
}
#endif

base::string16 AutocompleteMatch::GetWhyThisSuggestionText() const {
  // TODO(tommycli): Replace these placeholder strings with final ones from UX.
  switch (type) {
    case Type::URL_WHAT_YOU_TYPED:
      return base::ASCIIToUTF16(
          "This navigation match is the exact URL you typed.");

    case Type::HISTORY_URL:
    case Type::HISTORY_TITLE:
    case Type::HISTORY_BODY:
    case Type::HISTORY_KEYWORD:
      return base::ASCIIToUTF16(
          "This navigation match is a previously visited page from Chrome "
          "History.");

    case Type::NAVSUGGEST:
      return base::ASCIIToUTF16(
          "This navigation match is suggested by the search engine based on "
          "what you typed.");

    case Type::SEARCH_WHAT_YOU_TYPED:
      return base::ASCIIToUTF16("This search query is exactly what you typed.");

    case Type::SEARCH_HISTORY:
      // TODO(tommycli): We may need to distinguish between matches sourced
      // from search history saved in the cloud vs. locally.
      return base::ASCIIToUTF16(
          "This search query is suggested by the search engine based on what "
          "you typed and past search queries.");

    case Type::SEARCH_SUGGEST:
    case Type::SEARCH_SUGGEST_ENTITY:
    case Type::SEARCH_SUGGEST_TAIL:
      return base::ASCIIToUTF16(
          "This search query is suggested by the search engine based on what "
          "you typed.");

    case Type::SEARCH_SUGGEST_PERSONALIZED:
      return base::ASCIIToUTF16(
          "This search query is suggested by the search engine based on what "
          "you typed. It has also been personalized to you.");

    case Type::SEARCH_OTHER_ENGINE:
      return base::ASCIIToUTF16(
          "This search query is for a non-default search engine.");

    case Type::BOOKMARK_TITLE:
      return base::ASCIIToUTF16(
          "This navigation matches the title of a Bookmark.");

    case Type::NAVSUGGEST_PERSONALIZED:
      return base::ASCIIToUTF16(
          "This navigation match is suggested by the search engine based on "
          "what you typed. It has also been personalized to you.");

    case Type::CALCULATOR:
      return base::ASCIIToUTF16(
          "This calculation is the result of evaluating your input provided by "
          "your default search engine.");

    case Type::CLIPBOARD_URL:
    case Type::CLIPBOARD_TEXT:
    case Type::CLIPBOARD_IMAGE:
      return base::ASCIIToUTF16("This match is from the system clipboard.");

    case Type::VOICE_SUGGEST:
      return base::ASCIIToUTF16("This match is from voice.");

    case Type::DOCUMENT_SUGGESTION:
      return base::ASCIIToUTF16("This match is from your documents.");

    case Type::PEDAL:
      return base::ASCIIToUTF16(
          "This is a suggested Chrome action based on what you typed.");

    case Type::EXTENSION_APP_DEPRECATED:
    case Type::SEARCH_SUGGEST_PROFILE:
    case Type::CONTACT_DEPRECATED:
    case Type::PHYSICAL_WEB_DEPRECATED:
    case Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case Type::TAB_SEARCH_DEPRECATED:
    case Type::NUM_TYPES:
      NOTREACHED();
      return base::string16();
  }
}

// static
bool AutocompleteMatch::MoreRelevant(const AutocompleteMatch& match1,
                                     const AutocompleteMatch& match2) {
  // For equal-relevance matches, we sort alphabetically, so that providers
  // who return multiple elements at the same priority get a "stable" sort
  // across multiple updates.
  return (match1.relevance == match2.relevance)
             ? (match1.contents < match2.contents)
             : (match1.relevance > match2.relevance);
}

// static
bool AutocompleteMatch::BetterDuplicate(const AutocompleteMatch& match1,
                                        const AutocompleteMatch& match2) {
  // Prefer the Entity Match over the non-entity match, if they have the same
  // |fill_into_edit| value.
  if (match1.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      match2.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      match1.fill_into_edit == match2.fill_into_edit) {
    return true;
  }
  if (match1.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      match2.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      match1.fill_into_edit == match2.fill_into_edit) {
    return false;
  }

  // Prefer matches allowed to be the default match.
  if (match1.allowed_to_be_default_match && !match2.allowed_to_be_default_match)
    return true;
  if (!match1.allowed_to_be_default_match && match2.allowed_to_be_default_match)
    return false;

  // Prefer document suggestions.
  if (match1.type == AutocompleteMatchType::DOCUMENT_SUGGESTION &&
      match2.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
    return true;
  }
  if (match1.type != AutocompleteMatchType::DOCUMENT_SUGGESTION &&
      match2.type == AutocompleteMatchType::DOCUMENT_SUGGESTION) {
    return false;
  }

  // By default, simply prefer the more relevant match.
  return MoreRelevant(match1, match2);
}

// static
bool AutocompleteMatch::BetterDuplicateByIterator(
    const std::vector<AutocompleteMatch>::const_iterator it1,
    const std::vector<AutocompleteMatch>::const_iterator it2) {
  return BetterDuplicate(*it1, *it2);
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
        base::NumberToString(classifications[i].style);
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

AutocompleteMatch::Type AutocompleteMatch::GetDemotionType() const {
  if (!IsSubMatch())
    return type;
  else
    return parent_type;
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
    const base::string16& keyword) {
  if (!url.is_valid())
    return url;

  // Special-case canonicalizing Docs URLs. This logic is self-contained and
  // will not participate in the TemplateURL canonicalization.
  GURL docs_url = DocumentProvider::GetURLForDeduping(url);
  if (docs_url.is_valid())
    return docs_url;

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
      stripped_destination_url =
          GURL(template_url->url_ref().ReplaceSearchTerms(
              TemplateURLRef::SearchTermsArgs(search_terms),
              template_url_service->search_terms_data()));
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
  static const size_t prefix_len = base::size(prefix) - 1;
  std::string host = stripped_destination_url.host();
  if (host.compare(0, prefix_len, prefix) == 0 && host.length() > prefix_len) {
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
    bool* match_in_subdomain) {
  DCHECK(match_in_scheme);
  DCHECK(match_in_subdomain);

  size_t domain_length =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url.host_piece(),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
          .size();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  size_t host_pos = parsed.CountCharactersBefore(url::Parsed::HOST, false);

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
  }
}

// static
url_formatter::FormatUrlTypes AutocompleteMatch::GetFormatTypes(
    bool preserve_scheme,
    bool preserve_subdomain) {
  auto format_types = url_formatter::kFormatUrlOmitDefaults;
  if (preserve_scheme) {
    format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  } else {
    format_types |= url_formatter::kFormatUrlOmitHTTPS;
  }

  if (!preserve_subdomain) {
    format_types |= url_formatter::kFormatUrlOmitTrivialSubdomains;
  }

  return format_types;
}

// static
bool AutocompleteMatch::AllowedToBeDefault(const AutocompleteInput& input,
                                           AutocompleteMatch& match) {
  if (match.inline_autocompletion.empty())
    return true;
  if (input.prevent_inline_autocomplete())
    return false;
  if (input.text().empty() || !base::IsUnicodeWhitespace(input.text().back()))
    return true;

  // If we've reached here, the input ends in trailing whitespace. If the
  // trailing whitespace prefixes |match.inline_autocompletion|, then allow the
  // match to be default and remove the whitespace from
  // |match.inline_autocompletion|.
  size_t last_non_whitespace_pos =
      input.text().find_last_not_of(base::kWhitespaceUTF16);
  DCHECK_NE(last_non_whitespace_pos, std::string::npos);
  auto whitespace_suffix = input.text().substr(last_non_whitespace_pos + 1);
  if (base::StartsWith(match.inline_autocompletion, whitespace_suffix,
                       base::CompareCase::SENSITIVE)) {
    match.inline_autocompletion =
        match.inline_autocompletion.substr(whitespace_suffix.size());
    return true;
  }

  return false;
}

// static
void AutocompleteMatch::LogSearchEngineUsed(
    const AutocompleteMatch& match,
    TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  TemplateURL* template_url = match.GetTemplateURL(template_url_service, false);
  if (template_url) {
    SearchEngineType search_engine_type =
        match.destination_url.is_valid()
            ? TemplateURLPrepopulateData::GetEngineType(match.destination_url)
            : SEARCH_ENGINE_OTHER;
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngineType", search_engine_type,
                              SEARCH_ENGINE_MAX);
  }
}

// static
size_t AutocompleteMatch::GetNextFamilyID() {
  next_family_id_ += FAMILY_SIZE;
  // Avoid the default value. 0 means "no submatch", so no family can use it.
  if (next_family_id_ == 0)
    next_family_id_ += FAMILY_SIZE;
  return next_family_id_;
}

// static
bool AutocompleteMatch::IsSameFamily(size_t lhs, size_t rhs) {
  return (lhs & FAMILY_SIZE_MASK) == (rhs & FAMILY_SIZE_MASK);
}

void AutocompleteMatch::SetSubMatch(size_t subrelevance,
                                    AutocompleteMatch::Type parent_type) {
  this->subrelevance = subrelevance;
  this->parent_type = parent_type;
}

bool AutocompleteMatch::IsSubMatch() const {
  return subrelevance & ~FAMILY_SIZE_MASK;
}

void AutocompleteMatch::ComputeStrippedDestinationURL(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  // Other than document suggestions, computing |stripped_destination_url| will
  // have the same result during a match's lifecycle, so it's safe to skip
  // re-computing it if it's already computed. Document suggestions'
  // |stripped_url|s are pre-computed by the document provider, and overwriting
  // them here would prevent potential deduping.
  if (stripped_destination_url.is_empty()) {
    stripped_destination_url = GURLToStrippedGURL(
        destination_url, input, template_url_service, keyword);
  }
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

AutocompleteMatch AutocompleteMatch::DerivePedalSuggestion(
    OmniboxPedal* pedal) {
  AutocompleteMatch copy(*this);
  copy.pedal = pedal;
  if (subrelevance == 0)
    subrelevance = GetNextFamilyID();
  copy.SetSubMatch(subrelevance + PEDAL_FAMILY_ID, copy.type);
  DCHECK(IsSameFamily(subrelevance, copy.subrelevance));

  copy.type = Type::PEDAL;
  copy.destination_url = copy.pedal->GetNavigationUrl();

  // Normally this is computed by the match using a TemplateURLService
  // but Pedal URLs are not typical and unknown, and we don't want them to
  // be deduped, e.g. after stripping a query parameter that may do something
  // meaningful like indicate the viewable scope of a settings page.  So here
  // we keep the URL exactly as the Pedal specifies it.
  copy.stripped_destination_url = copy.destination_url;

  // Note: Always use empty classifications for empty text and non-empty
  // classifications for non-empty text.
  const auto& labels = copy.pedal->GetLabelStrings();
  copy.contents = labels.suggestion_contents;
  copy.contents_class = {ACMatchClassification(0, ACMatchClassification::NONE)};
  copy.description = labels.hint;
  copy.description_class = {
      ACMatchClassification(0, ACMatchClassification::NONE)};

  return copy;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const std::string& value) {
  DCHECK(!property.empty());
  DCHECK(!value.empty());
  additional_info[property] = value;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             int value) {
  RecordAdditionalInfo(property, base::NumberToString(value));
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

metrics::OmniboxEventProto::Suggestion::ResultType
AutocompleteMatch::AsOmniboxEventResultType() const {
  using metrics::OmniboxEventProto;

  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::URL_WHAT_YOU_TYPED;
    case AutocompleteMatchType::HISTORY_URL:
      return OmniboxEventProto::Suggestion::HISTORY_URL;
    case AutocompleteMatchType::HISTORY_TITLE:
      return OmniboxEventProto::Suggestion::HISTORY_TITLE;
    case AutocompleteMatchType::HISTORY_BODY:
      return OmniboxEventProto::Suggestion::HISTORY_BODY;
    case AutocompleteMatchType::HISTORY_KEYWORD:
      return OmniboxEventProto::Suggestion::HISTORY_KEYWORD;
    case AutocompleteMatchType::NAVSUGGEST:
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::SEARCH_WHAT_YOU_TYPED;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return OmniboxEventProto::Suggestion::SEARCH_HISTORY;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST;
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_TAIL;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PROFILE;
    case AutocompleteMatchType::CALCULATOR:
      return OmniboxEventProto::Suggestion::CALCULATOR;
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return OmniboxEventProto::Suggestion::SEARCH_OTHER_ENGINE;
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
      return OmniboxEventProto::Suggestion::EXTENSION_APP;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return OmniboxEventProto::Suggestion::BOOKMARK_TITLE;
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::NAVSUGGEST_PERSONALIZED;
    case AutocompleteMatchType::CLIPBOARD_URL:
      return OmniboxEventProto::Suggestion::CLIPBOARD_URL;
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
      return OmniboxEventProto::Suggestion::DOCUMENT;
    case AutocompleteMatchType::PEDAL:
      // TODO(orinj): Add a new OmniboxEventProto type for Pedals.
      // return OmniboxEventProto::Suggestion::PEDAL;
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::CLIPBOARD_TEXT:
      return OmniboxEventProto::Suggestion::CLIPBOARD_TEXT;
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return OmniboxEventProto::Suggestion::CLIPBOARD_IMAGE;
    case AutocompleteMatchType::VOICE_SUGGEST:
      // VOICE_SUGGEST matches are only used in Java and are not logged,
      // so we should never reach this case.
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      break;
  }
  NOTREACHED();
  return OmniboxEventProto::Suggestion::UNKNOWN_RESULT_TYPE;
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

bool AutocompleteMatch::IsSearchProviderSearchSuggestion() const {
  const bool from_search_provider =
      (provider && provider->type() == AutocompleteProvider::TYPE_SEARCH);
  return from_search_provider &&
         type != AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
}

bool AutocompleteMatch::IsOnDeviceSearchSuggestion() const {
  const bool from_on_device_provider =
      (provider &&
       provider->type() == AutocompleteProvider::TYPE_ON_DEVICE_HEAD);
  return from_on_device_provider && subtype_identifier == 271;
}

bool AutocompleteMatch::IsTrivialAutocompletion() const {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE;
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
  // Prevent re-addition of prefix.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL &&
      tail_suggest_common_prefix.empty()) {
    tail_suggest_common_prefix = common_prefix;
    // Insert an ellipsis before uncommon part.
    const auto ellipsis = base::ASCIIToUTF16(kEllipsis);
    contents = ellipsis + contents;
    // If the first class is not already NONE, prepend a NONE class for the new
    // ellipsis.
    if (contents_class.empty() ||
        (contents_class[0].offset == 0 &&
         contents_class[0].style != ACMatchClassification::NONE)) {
      contents_class.insert(contents_class.begin(),
                            {0, ACMatchClassification::NONE});
    }
    // Shift existing styles.
    for (size_t i = 1; i < contents_class.size(); ++i)
      contents_class[i].offset += ellipsis.size();
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
  res += base::trace_event::EstimateMemoryUsage(tail_suggest_common_prefix);
  res += base::trace_event::EstimateMemoryUsage(contents);
  res += base::trace_event::EstimateMemoryUsage(contents_class);
  res += base::trace_event::EstimateMemoryUsage(description);
  res += base::trace_event::EstimateMemoryUsage(description_class);
  if (answer)
    res += base::trace_event::EstimateMemoryUsage(answer.value());
  else
    res += sizeof(SuggestionAnswer);
  res += base::trace_event::EstimateMemoryUsage(associated_keyword);
  res += base::trace_event::EstimateMemoryUsage(keyword);
  res += base::trace_event::EstimateMemoryUsage(search_terms_args);
  res += base::trace_event::EstimateMemoryUsage(post_content);
  res += base::trace_event::EstimateMemoryUsage(additional_info);
  res += base::trace_event::EstimateMemoryUsage(duplicate_matches);

  return res;
}

bool AutocompleteMatch::ShouldShowTabMatchButton() const {
  // TODO(pkasting): This kind of presentational logic does not belong on
  // AutocompleteMatch and should be e.g. a static method in
  // OmniboxMatchCellView that takes an AutocompleteMatch.
  return has_tab_match && !associated_keyword &&
         !OmniboxFieldTrial::IsTabSwitchSuggestionsDedicatedRowEnabled();
}

bool AutocompleteMatch::IsTabSwitchSuggestion() const {
  return (subrelevance & ~FAMILY_SIZE_MASK) == TAB_SWITCH_FAMILY_ID;
}

void AutocompleteMatch::UpgradeMatchWithPropertiesFrom(
    const AutocompleteMatch& duplicate_match) {
  // For Entity Matches, absorb the duplicate match's |allowed_to_be_default|
  // and |inline_autocomplete| properties.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      fill_into_edit == duplicate_match.fill_into_edit &&
      duplicate_match.allowed_to_be_default_match) {
    allowed_to_be_default_match = true;
    if (inline_autocompletion.empty()) {
      inline_autocompletion = duplicate_match.inline_autocompletion;
      is_navigational_title_match = duplicate_match.is_navigational_title_match;
    }
  }

  // And always absorb the higher relevance score of duplicates.
  if (duplicate_match.relevance > relevance) {
    RecordAdditionalInfo(kACMatchPropertyScoreBoostedFrom, relevance);
    relevance = duplicate_match.relevance;
  }
}

void AutocompleteMatch::TryAutocompleteWithTitle(
    const base::string16& title,
    const AutocompleteInput& input) {
  if (!base::FeatureList::IsEnabled(omnibox::kAutocompleteTitles))
    return;

  const base::string16 lower_text{base::i18n::ToLower(title)};
  const base::string16 lower_input_text{base::i18n::ToLower(input.text())};

  if (!base::StartsWith(lower_text, lower_input_text,
                        base::CompareCase::SENSITIVE)) {
    return;
  }

  // For exact matches, promote the relevance to out-score verbatim
  // search-what-you-typed matches.
  if (lower_text == lower_input_text) {
    relevance =
        std::max(relevance, SearchProvider::kNonURLVerbatimRelevance + 10);
    RecordAdditionalInfo("title match", "full");
  } else
    RecordAdditionalInfo("title match", "prefix");

  fill_into_edit = title;
  inline_autocompletion = fill_into_edit.substr(lower_input_text.length());
  allowed_to_be_default_match =
      inline_autocompletion.empty() || !input.prevent_inline_autocomplete();
  is_navigational_title_match = true;
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
