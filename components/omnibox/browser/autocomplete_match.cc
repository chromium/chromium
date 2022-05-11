// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/template_url_service.h"
#include "inline_autocompletion_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/third_party/mozilla/url_parse.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/suggestion_answer.h"
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
    const std::vector<std::u16string>& terms_prefixed_by_http_or_https,
    const GURL& url) {
  size_t prefix_length =
      url.scheme().length() + strlen(url::kStandardSchemeSeparator);
  DCHECK_GE(url.spec().length(), prefix_length);
  const std::u16string& formatted_url = url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
      nullptr, nullptr, &prefix_length);
  if (prefix_length == std::u16string::npos)
    return false;
  const std::u16string& formatted_url_without_scheme =
      formatted_url.substr(prefix_length);
  for (const auto& term : terms_prefixed_by_http_or_https) {
    if (base::StartsWith(formatted_url_without_scheme, term,
                         base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

// Check if title or non-prefix rich autocompletion is possible. I.e.:
// 1) Enabled for all providers OR for the shortcut provider if the suggestion
//    is from the shortcut provider.
// 2) The input is longer than the threshold.
// 3) Enabled for inputs containing spaces OR the input contains no spaces.
bool RichAutocompletionApplicable(bool enabled_all_providers,
                                  bool enabled_shortcut_provider,
                                  size_t min_char,
                                  bool no_inputs_with_spaces,
                                  bool shortcut_provider,
                                  const std::u16string& input_text) {
  return (enabled_all_providers ||
          (shortcut_provider && enabled_shortcut_provider)) &&
         input_text.size() >= min_char &&
         (!no_inputs_with_spaces ||
          base::ranges::none_of(input_text, &base::IsAsciiWhitespace<char>));
}

}  // namespace

SplitAutocompletion::SplitAutocompletion(std::u16string display_text,
                                         std::vector<gfx::Range> selections)
    : display_text(display_text), selections(selections) {}

SplitAutocompletion::SplitAutocompletion() = default;
SplitAutocompletion::SplitAutocompletion(const SplitAutocompletion& copy) =
    default;
SplitAutocompletion::SplitAutocompletion(SplitAutocompletion&& other) noexcept =
    default;
SplitAutocompletion& SplitAutocompletion::operator=(
    const SplitAutocompletion&) = default;
SplitAutocompletion& SplitAutocompletion::operator=(
    SplitAutocompletion&&) noexcept = default;

SplitAutocompletion::~SplitAutocompletion() = default;

bool SplitAutocompletion::Empty() const {
  return selections.empty();
}

void SplitAutocompletion::Clear() {
  selections.clear();
}

// AutocompleteMatch ----------------------------------------------------------

// static
const char* const AutocompleteMatch::kDocumentTypeStrings[]{
    "none",        "drive_docs", "drive_forms", "drive_sheets", "drive_slides",
    "drive_image", "drive_pdf",  "drive_video", "drive_folder", "drive_other"};

static_assert(
    std::size(AutocompleteMatch::kDocumentTypeStrings) ==
        static_cast<int>(AutocompleteMatch::DocumentType::DOCUMENT_TYPE_SIZE),
    "Sizes of AutocompleteMatch::kDocumentTypeStrings and "
    "AutocompleteMatch::DocumentType don't match.");

// static
const char* AutocompleteMatch::DocumentTypeString(DocumentType type) {
  return kDocumentTypeStrings[static_cast<int>(type)];
}

// static
bool AutocompleteMatch::DocumentTypeFromInteger(int value,
                                                DocumentType* result) {
  DCHECK(result);

  // The resulting value may still be invalid after the static_cast.
  DocumentType document_type = static_cast<DocumentType>(value);
  if (document_type >= DocumentType::NONE &&
      document_type < DocumentType::DOCUMENT_TYPE_SIZE) {
    *result = document_type;
    return true;
  }

  return false;
}

// static
const char16_t AutocompleteMatch::kInvalidChars[] = {
    '\n',   '\r', '\t',
    0x2028,  // Line separator
    0x2029,  // Paragraph separator
    0};

// static
const char16_t AutocompleteMatch::kEllipsis[] = u"... ";

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
      typed_count(match.typed_count),
      deletable(match.deletable),
      fill_into_edit(match.fill_into_edit),
      additional_text(match.additional_text),
      inline_autocompletion(match.inline_autocompletion),
      rich_autocompletion_triggered(match.rich_autocompletion_triggered),
      prefix_autocompletion(match.prefix_autocompletion),
      split_autocompletion(match.split_autocompletion),
      allowed_to_be_default_match(match.allowed_to_be_default_match),
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
      description_for_shortcuts(match.description_for_shortcuts),
      description_class_for_shortcuts(match.description_class_for_shortcuts),
      suggestion_group_id(match.suggestion_group_id),
      swap_contents_and_description(match.swap_contents_and_description),
      answer(match.answer),
      transition(match.transition),
      type(match.type),
      has_tab_match(match.has_tab_match),
      subtypes(match.subtypes),
      associated_keyword(match.associated_keyword
                             ? new AutocompleteMatch(*match.associated_keyword)
                             : nullptr),
      keyword(match.keyword),
      from_keyword(match.from_keyword),
      action(match.action),
      from_previous(match.from_previous),
      search_terms_args(
          match.search_terms_args
              ? new TemplateURLRef::SearchTermsArgs(*match.search_terms_args)
              : nullptr),
      post_content(match.post_content
                       ? new TemplateURLRef::PostContent(*match.post_content)
                       : nullptr),
      additional_info(match.additional_info),
      duplicate_matches(match.duplicate_matches),
      query_tiles(match.query_tiles),
      navsuggest_tiles(match.navsuggest_tiles) {}

AutocompleteMatch::AutocompleteMatch(AutocompleteMatch&& match) noexcept {
  *this = std::move(match);
}

AutocompleteMatch& AutocompleteMatch::operator=(
    AutocompleteMatch&& match) noexcept {
  provider = std::move(match.provider);
  relevance = std::move(match.relevance);
  typed_count = std::move(match.typed_count);
  deletable = std::move(match.deletable);
  fill_into_edit = std::move(match.fill_into_edit);
  additional_text = std::move(match.additional_text);
  inline_autocompletion = std::move(match.inline_autocompletion);
  rich_autocompletion_triggered =
      std::move(match.rich_autocompletion_triggered);
  prefix_autocompletion = std::move(match.prefix_autocompletion);
  split_autocompletion = std::move(match.split_autocompletion);
  allowed_to_be_default_match = std::move(match.allowed_to_be_default_match);
  destination_url = std::move(match.destination_url);
  stripped_destination_url = std::move(match.stripped_destination_url);
  image_dominant_color = std::move(match.image_dominant_color);
  image_url = std::move(match.image_url);
  document_type = std::move(match.document_type);
  tail_suggest_common_prefix = std::move(match.tail_suggest_common_prefix);
  contents = std::move(match.contents);
  contents_class = std::move(match.contents_class);
  description = std::move(match.description);
  description_class = std::move(match.description_class);
  description_for_shortcuts = std::move(match.description_for_shortcuts);
  description_class_for_shortcuts =
      std::move(match.description_class_for_shortcuts);
  suggestion_group_id = std::move(match.suggestion_group_id);
  swap_contents_and_description =
      std::move(match.swap_contents_and_description);
  answer = std::move(match.answer);
  transition = std::move(match.transition);
  type = std::move(match.type);
  has_tab_match = std::move(match.has_tab_match);
  subtypes = std::move(match.subtypes);
  associated_keyword = std::move(match.associated_keyword);
  keyword = std::move(match.keyword);
  from_keyword = std::move(match.from_keyword);
  action = std::move(match.action);
  from_previous = std::move(match.from_previous);
  search_terms_args = std::move(match.search_terms_args);
  post_content = std::move(match.post_content);
  additional_info = std::move(match.additional_info);
  duplicate_matches = std::move(match.duplicate_matches);
  query_tiles = std::move(match.query_tiles);
  navsuggest_tiles = std::move(match.navsuggest_tiles);
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
  std::swap(java_match_, match.java_match_);
  std::swap(matching_java_tab_, match.matching_java_tab_);
  UpdateJavaObjectNativeRef();
#endif
  return *this;
}

AutocompleteMatch::~AutocompleteMatch() {
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif
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
  additional_text = match.additional_text;
  inline_autocompletion = match.inline_autocompletion;
  rich_autocompletion_triggered = match.rich_autocompletion_triggered;
  prefix_autocompletion = match.prefix_autocompletion;
  split_autocompletion = match.split_autocompletion;
  allowed_to_be_default_match = match.allowed_to_be_default_match;
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
  description_for_shortcuts = match.description_for_shortcuts;
  description_class_for_shortcuts = match.description_class_for_shortcuts;
  suggestion_group_id = match.suggestion_group_id;
  swap_contents_and_description = match.swap_contents_and_description;
  answer = match.answer;
  transition = match.transition;
  type = match.type;
  has_tab_match = match.has_tab_match;
  subtypes = match.subtypes;
  associated_keyword.reset(
      match.associated_keyword
          ? new AutocompleteMatch(*match.associated_keyword)
          : nullptr);
  keyword = match.keyword;
  from_keyword = match.from_keyword;
  action = match.action;
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
  query_tiles = match.query_tiles;
  navsuggest_tiles = match.navsuggest_tiles;

#if BUILDFLAG(IS_ANDROID)
  // In case the target element previously held a java object, release it.
  // This happens, when in an expression "match1 = match2;" match1 already
  // is initialized and linked to a Java object: we rewrite the contents of the
  // match1 object and it would be desired to either update its corresponding
  // Java element, or drop it and construct it lazily the next time it is
  // needed.
  // Note that because Java<>C++ AutocompleteMatch relation is 1:1, we do not
  // want to copy the object here.
  DestroyJavaObject();
#endif
  return *this;
}

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
// static
const gfx::VectorIcon& AutocompleteMatch::AnswerTypeToAnswerIcon(int type) {
  switch (static_cast<SuggestionAnswer::AnswerType>(type)) {
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return omnibox::kAnswerCurrencyIcon;
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return omnibox::kAnswerDictionaryIcon;
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return omnibox::kAnswerFinanceIcon;
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return omnibox::kAnswerSunriseIcon;
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return omnibox::kAnswerTranslationIcon;
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return omnibox::kAnswerWhenIsIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

const gfx::VectorIcon& AutocompleteMatch::GetVectorIcon(
    bool is_bookmark) const {
  // TODO(https://crbug.com/1024114): Remove crash logging once fixed.
  SCOPED_CRASH_KEY_NUMBER("AutocompleteMatch", "type", type);
  SCOPED_CRASH_KEY_NUMBER("AutocompleteMatch", "provider_type",
                          provider ? provider->type() : -1);
  if (is_bookmark)
    return omnibox::kBookmarkIcon;
  if (answer.has_value())
    return AnswerTypeToAnswerIcon(answer->type());
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
    case Type::TILE_NAVSUGGEST:
    case Type::OPEN_TAB:
      return omnibox::kPageIcon;

    case Type::SEARCH_SUGGEST: {
      if (subtypes.contains(/*SUBTYPE_TRENDS=*/143))
        return omnibox::kTrendingUpIcon;
      return vector_icons::kSearchIcon;
    }

    case Type::SEARCH_WHAT_YOU_TYPED:
    case Type::SEARCH_SUGGEST_ENTITY:
    case Type::SEARCH_SUGGEST_PROFILE:
    case Type::SEARCH_OTHER_ENGINE:
    case Type::CONTACT_DEPRECATED:
    case Type::VOICE_SUGGEST:
    case Type::PEDAL_DEPRECATED:
    case Type::CLIPBOARD_TEXT:
    case Type::CLIPBOARD_IMAGE:
    case Type::TILE_SUGGESTION:
      return vector_icons::kSearchIcon;

    case Type::SEARCH_HISTORY:
    case Type::SEARCH_SUGGEST_PERSONALIZED: {
      DCHECK(IsSearchHistoryType(type));
      return omnibox::kClockIcon;
    }

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
        case DocumentType::DRIVE_FOLDER:
          return omnibox::kDriveFolderIcon;
        case DocumentType::DRIVE_OTHER:
          return omnibox::kDriveLogoIcon;
        default:
          return omnibox::kPageIcon;
      }

    case Type::NUM_TYPES:
      // TODO(https://crbug.com/1024114): Replace with NOTREACHED() once fixed.
      CHECK(false);
      return vector_icons::kErrorIcon;
  }

  // TODO(https://crbug.com/1024114): Replace with NOTREACHED() once fixed.
  CHECK(false);
  return vector_icons::kErrorIcon;
}
#endif

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

  // Prefer open tab matches over other types of matches.
  if (match1.type == AutocompleteMatchType::OPEN_TAB &&
      match2.type != AutocompleteMatchType::OPEN_TAB) {
    return true;
  }
  if (match1.type != AutocompleteMatchType::OPEN_TAB &&
      match2.type == AutocompleteMatchType::OPEN_TAB) {
    return false;
  }

  // Prefer matches allowed to be the default match.
  if (match1.allowed_to_be_default_match && !match2.allowed_to_be_default_match)
    return true;
  if (!match1.allowed_to_be_default_match && match2.allowed_to_be_default_match)
    return false;

  // Prefer URL autocompleted default matches if the appropriate param is true.
  if (OmniboxFieldTrial::kRichAutocompletionAutocompletePreferUrlsOverPrefixes
          .Get()) {
    if (match1.additional_text.empty() && !match2.additional_text.empty())
      return true;
    if (!match1.additional_text.empty() && match2.additional_text.empty())
      return false;
  }

  // Prefer some providers over others.
  const std::vector<AutocompleteProvider::Type> preferred_providers = {
      // Prefer live document suggestions. We check provider type instead of
      // match type in order to distinguish live suggestions from the document
      // provider from stale suggestions from the shortcuts providers, because
      // the latter omits changing metadata such as last access date.
      AutocompleteProvider::TYPE_DOCUMENT,
      // Prefer bookmark suggestions, as 1) their titles may be explicitly set,
      // and 2) they may display enhanced information such as the bookmark
      // folders path.
      AutocompleteProvider::TYPE_BOOKMARK,
  };

  if (match1.provider->type() != match2.provider->type()) {
    for (const auto& preferred_provider : preferred_providers) {
      if (match1.provider->type() == preferred_provider)
        return true;
      if (match2.provider->type() == preferred_provider)
        return false;
    }
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
    const std::u16string& find_text,
    const std::u16string& text,
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
  if (match_location == std::u16string::npos) {
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
std::u16string AutocompleteMatch::SanitizeString(const std::u16string& text) {
  // NOTE: This logic is mirrored by |sanitizeString()| in
  // omnibox_custom_bindings.js.
  std::u16string result;
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
         type == AutocompleteMatchType::TILE_SUGGESTION ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
}

// static
bool AutocompleteMatch::IsSearchHistoryType(Type type) {
  return type == AutocompleteMatchType::SEARCH_HISTORY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED;
}

// static
bool AutocompleteMatch::IsActionCompatibleType(Type type) {
  // Note: There is a PEDAL type, but it is deprecated because Pedals always
  // attach to matches of other types instead of creating dedicated matches.
  return type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
         // Attaching to Tail Suggest types looks weird, and is actually
         // technically wrong because the Pedals annotator (and history clusters
         // annotator) both use match.contents. If we do want to turn on Actions
         // for tail suggest in the future, we should switch to using
         // match.fill_into_edit or maybe page title for URL matches, and come
         // up with a UI design for the button in the tail suggest layout.
         type != AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
}

// static
bool AutocompleteMatch::ShouldBeSkippedForGroupBySearchVsUrl(Type type) {
  return type == AutocompleteMatchType::CLIPBOARD_URL ||
         type == AutocompleteMatchType::CLIPBOARD_TEXT ||
         type == AutocompleteMatchType::CLIPBOARD_IMAGE ||
         type == AutocompleteMatchType::TILE_NAVSUGGEST ||
         type == AutocompleteMatchType::TILE_SUGGESTION;
}

// static
TemplateURL* AutocompleteMatch::GetTemplateURLWithKeyword(
    TemplateURLService* template_url_service,
    const std::u16string& keyword,
    const std::string& host) {
  return const_cast<TemplateURL*>(GetTemplateURLWithKeyword(
      static_cast<const TemplateURLService*>(template_url_service), keyword,
      host));
}

// static
const TemplateURL* AutocompleteMatch::GetTemplateURLWithKeyword(
    const TemplateURLService* template_url_service,
    const std::u16string& keyword,
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
    const std::u16string& keyword) {
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
    std::u16string search_terms;
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
  static const size_t prefix_len = std::size(prefix) - 1;
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
    replacements.SetSchemeStr(url::kHttpScheme);
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
          url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
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
void AutocompleteMatch::LogSearchEngineUsed(
    const AutocompleteMatch& match,
    TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  TemplateURL* template_url = match.GetTemplateURL(template_url_service, false);
  if (template_url) {
    SearchEngineType search_engine_type =
        match.destination_url.is_valid()
            ? SearchEngineUtils::GetEngineType(match.destination_url)
            : SEARCH_ENGINE_OTHER;
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngineType", search_engine_type,
                              SEARCH_ENGINE_MAX);
  }
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
    std::u16string* keyword_out,
    bool* is_keyword_hint) const {
  *is_keyword_hint = associated_keyword != nullptr;
  keyword_out->assign(
      *is_keyword_hint
          ? associated_keyword->keyword
          : GetSubstitutingExplicitlyInvokedKeyword(template_url_service));
}

std::u16string AutocompleteMatch::GetSubstitutingExplicitlyInvokedKeyword(
    TemplateURLService* template_url_service) const {
  if (!ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_KEYWORD) ||
      template_url_service == nullptr) {
    return std::u16string();
  }

  const TemplateURL* t_url = GetTemplateURL(template_url_service, false);
  return (t_url &&
          t_url->SupportsReplacement(template_url_service->search_terms_data()))
             ? keyword
             : std::u16string();
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
  return answer ? answer->image_url() : image_url;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const std::string& value) {
  DCHECK(!property.empty());
  additional_info[property] = value;
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             const std::u16string& value) {
  RecordAdditionalInfo(property, base::UTF16ToUTF8(value));
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
    case AutocompleteMatchType::CLIPBOARD_TEXT:
      return OmniboxEventProto::Suggestion::CLIPBOARD_TEXT;
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return OmniboxEventProto::Suggestion::CLIPBOARD_IMAGE;
    case AutocompleteMatchType::TILE_SUGGESTION:
      return OmniboxEventProto::Suggestion::TILE_SUGGESTION;
    case AutocompleteMatchType::TILE_NAVSUGGEST:
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::OPEN_TAB:
      return OmniboxEventProto::Suggestion::OPEN_TAB;
    case AutocompleteMatchType::VOICE_SUGGEST:
      // VOICE_SUGGEST matches are only used in Java and are not logged,
      // so we should never reach this case.
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::PEDAL_DEPRECATED:
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
  return from_on_device_provider && subtypes.contains(271);
}

bool AutocompleteMatch::IsTrivialAutocompletion() const {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE;
}

bool AutocompleteMatch::SupportsDeletion() const {
  return deletable ||
         std::any_of(duplicate_matches.begin(), duplicate_matches.end(),
                     [](const auto& m) { return m.deletable; });
}

AutocompleteMatch
AutocompleteMatch::GetMatchWithContentsAndDescriptionPossiblySwapped() const {
  AutocompleteMatch copy(*this);
  if (copy.swap_contents_and_description) {
    std::swap(copy.contents, copy.description);
    std::swap(copy.contents_class, copy.description_class);
    copy.description_for_shortcuts.clear();
    copy.description_class_for_shortcuts.clear();
    // Clear bit to prevent accidentally performing the swap again.
    copy.swap_contents_and_description = false;
  }
  return copy;
}

void AutocompleteMatch::SetAllowedToBeDefault(const AutocompleteInput& input) {
  if (IsEmptyAutocompletion())
    allowed_to_be_default_match = true;
  else if (input.prevent_inline_autocomplete())
    allowed_to_be_default_match = false;
  else if (input.text().empty() ||
           !base::IsUnicodeWhitespace(input.text().back()))
    allowed_to_be_default_match = true;
  else {
    // If we've reached here, the input ends in trailing whitespace. If the
    // trailing whitespace prefixes |inline_autocompletion|, then allow the
    // match to be default and remove the whitespace from
    // |inline_autocompletion|.
    size_t last_non_whitespace_pos =
        input.text().find_last_not_of(base::kWhitespaceUTF16);
    DCHECK_NE(last_non_whitespace_pos, std::string::npos);
    auto whitespace_suffix = input.text().substr(last_non_whitespace_pos + 1);
    if (base::StartsWith(inline_autocompletion, whitespace_suffix,
                         base::CompareCase::SENSITIVE)) {
      inline_autocompletion =
          inline_autocompletion.substr(whitespace_suffix.size());
      allowed_to_be_default_match = true;
    } else
      allowed_to_be_default_match = false;
  }
}

void AutocompleteMatch::SetTailSuggestCommonPrefix(
    const std::u16string& common_prefix) {
  // Prevent re-addition of prefix.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL &&
      tail_suggest_common_prefix.empty()) {
    tail_suggest_common_prefix = common_prefix;
  }
}

void AutocompleteMatch::SetTailSuggestContentPrefix(
    const std::u16string& common_prefix) {
  // Prevent re-addition of prefix.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL &&
      tail_suggest_common_prefix.empty()) {
    SetTailSuggestCommonPrefix(common_prefix);
    // Insert an ellipsis before uncommon part.
    const std::u16string ellipsis = kEllipsis;
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
  res += base::trace_event::EstimateMemoryUsage(additional_text);
  res += base::trace_event::EstimateMemoryUsage(inline_autocompletion);
  res += base::trace_event::EstimateMemoryUsage(prefix_autocompletion);
  res += base::trace_event::EstimateMemoryUsage(destination_url);
  res += base::trace_event::EstimateMemoryUsage(stripped_destination_url);
  res += base::trace_event::EstimateMemoryUsage(image_dominant_color);
  res += base::trace_event::EstimateMemoryUsage(image_url);
  res += base::trace_event::EstimateMemoryUsage(tail_suggest_common_prefix);
  res += base::trace_event::EstimateMemoryUsage(contents);
  res += base::trace_event::EstimateMemoryUsage(contents_class);
  res += base::trace_event::EstimateMemoryUsage(description);
  res += base::trace_event::EstimateMemoryUsage(description_class);
  res += base::trace_event::EstimateMemoryUsage(description_for_shortcuts);
  res +=
      base::trace_event::EstimateMemoryUsage(description_class_for_shortcuts);
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

void AutocompleteMatch::UpgradeMatchWithPropertiesFrom(
    AutocompleteMatch& duplicate_match) {
  // For Entity Matches, absorb the duplicate match's |allowed_to_be_default|
  // and |inline_autocompletion| properties.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      fill_into_edit == duplicate_match.fill_into_edit &&
      duplicate_match.allowed_to_be_default_match) {
    allowed_to_be_default_match = true;
    if (IsEmptyAutocompletion()) {
      inline_autocompletion = duplicate_match.inline_autocompletion;
      prefix_autocompletion = duplicate_match.prefix_autocompletion;
      split_autocompletion = duplicate_match.split_autocompletion;
    }
  }

  // For Search Suggest and Search What-You-Typed matches, absorb any
  // Search History type.
  if ((type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
       type == AutocompleteMatchType::SEARCH_SUGGEST) &&
      fill_into_edit == duplicate_match.fill_into_edit &&
      IsSearchHistoryType(duplicate_match.type)) {
    type = duplicate_match.type;
  }

  // And always absorb the higher relevance score of duplicates.
  if (duplicate_match.relevance > relevance) {
    RecordAdditionalInfo(kACMatchPropertyScoreBoostedFrom, relevance);
    relevance = duplicate_match.relevance;
  }

  // Take the |action|, if any, so that it will be presented instead of buried.
  if (!action && duplicate_match.action &&
      AutocompleteMatch::IsActionCompatibleType(type)) {
    action = duplicate_match.action;
    duplicate_match.action = nullptr;
  }

  // Copy |rich_autocompletion_triggered| for counterfactual logging. Only copy
  // true values since a rich autocompleted would have
  // |allowed_to_be_default_match| true and would be preferred to a non rich
  // autocompleted duplicate in non-counterfactual variations.
  if (duplicate_match.rich_autocompletion_triggered)
    rich_autocompletion_triggered = true;
}

bool AutocompleteMatch::TryRichAutocompletion(
    const std::u16string& primary_text,
    const std::u16string& secondary_text,
    const AutocompleteInput& input,
    const std::u16string& shortcut_text) {
  if (!OmniboxFieldTrial::IsRichAutocompletionEnabled())
    return false;

  bool counterfactual =
      OmniboxFieldTrial::kRichAutocompletionCounterfactual.Get();

  if (input.prevent_inline_autocomplete())
    return false;

  // Lowercase the strings for case-insensitive comparisons.
  const std::u16string primary_text_lower{base::i18n::ToLower(primary_text)};
  const std::u16string secondary_text_lower{
      base::i18n::ToLower(secondary_text)};
  const std::u16string shortcut_text_lower{base::i18n::ToLower(shortcut_text)};
  const std::u16string input_text_lower{base::i18n::ToLower(input.text())};

  // Try matching the prefix of |primary_text|.
  if (base::StartsWith(primary_text_lower, input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    if (counterfactual)
      return false;
    // This case intentionally doesn't set |rich_autocompletion_triggered| to
    // true since presumably non-rich autocompletion should also be able to
    // handle this case.
    inline_autocompletion = primary_text.substr(input_text_lower.length());
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "primary & prefix");
    return true;
  }

  // Determine which types of rich AC are allowed. Depends on:
  // - The input length.
  // - Whether the input contains whitespaces.
  // - Whether the match is from the shortcut provider.
  // - Fieldtrial params.
  const bool can_autocomplete_titles = RichAutocompletionApplicable(
      OmniboxFieldTrial::kRichAutocompletionAutocompleteTitles.Get(),
      OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesShortcutProvider
          .Get(),
      OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar.Get(),
      OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesNoInputsWithSpaces
          .Get(),
      !shortcut_text.empty(), input.text());
  const bool can_autocomplete_non_prefix = RichAutocompletionApplicable(
      OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.Get(),
      OmniboxFieldTrial::
          kRichAutocompletionAutocompleteNonPrefixShortcutProvider.Get(),
      OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar.Get(),
      OmniboxFieldTrial::
          kRichAutocompletionAutocompleteNonPrefixNoInputsWithSpaces.Get(),
      !shortcut_text.empty(), input.text());
  const bool can_autocomplete_shortcut_text = RichAutocompletionApplicable(
      false,
      OmniboxFieldTrial::kRichAutocompletionAutocompleteShortcutText.Get(),
      OmniboxFieldTrial::kRichAutocompletionAutocompleteShortcutTextMinChar
          .Get(),
      OmniboxFieldTrial::
          kRichAutocompletionAutocompleteShortcutTextNoInputsWithSpaces.Get(),
      !shortcut_text.empty(), input.text());

  // All else equal, prefer matching primary over secondary texts and prefixes
  // over non-prefixes. |prefer_primary_non_prefix_over_secondary_prefix|
  // determines whether to prefer matching primary text non-prefixes or
  // secondary text prefixes.
  bool prefer_primary_non_prefix_over_secondary_prefix =
      OmniboxFieldTrial::kRichAutocompletionAutocompletePreferUrlsOverPrefixes
          .Get();

  size_t find_index;

  // A helper to avoid duplicate code. Depending on the
  // |prefer_primary_non_prefix_over_secondary_prefix|, this may be invoked
  // either before or after trying prefix secondary autocompletion.
  auto NonPrefixPrimaryHelper = [&]() {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    inline_autocompletion =
        primary_text.substr(find_index + input_text_lower.length());
    prefix_autocompletion = primary_text.substr(0, find_index);
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "primary & non-prefix");
    return true;
  };

  // Try matching a non-prefix of |primary_text| if
  // |prefer_primary_non_prefix_over_secondary_prefix| is true; otherwise, we'll
  // try this only after tying to match the prefix of |secondary_text|.
  if (prefer_primary_non_prefix_over_secondary_prefix &&
      can_autocomplete_non_prefix &&
      (find_index = FindAtWordbreak(primary_text_lower, input_text_lower)) !=
          std::u16string::npos) {
    return NonPrefixPrimaryHelper();
  }

  // Try matching the prefix of |secondary_text|.
  if (can_autocomplete_titles &&
      base::StartsWith(secondary_text_lower, input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    additional_text = primary_text;
    inline_autocompletion = secondary_text.substr(input_text_lower.length());
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "secondary & prefix");
    return true;
  }

  // Try matching a non-prefix of |primary_text|. If
  // |prefer_primary_non_prefix_over_secondary_prefix| is false; otherwise, this
  // was already tried above.
  if (!prefer_primary_non_prefix_over_secondary_prefix &&
      can_autocomplete_non_prefix &&
      (find_index = FindAtWordbreak(primary_text_lower, input_text_lower)) !=
          std::u16string::npos) {
    return NonPrefixPrimaryHelper();
  }

  // Try matching a non-prefix of |secondary_text|.
  if (can_autocomplete_non_prefix && can_autocomplete_titles &&
      (find_index = FindAtWordbreak(secondary_text_lower, input_text_lower)) !=
          std::u16string::npos) {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    additional_text = primary_text;
    inline_autocompletion =
        secondary_text.substr(find_index + input_text_lower.length());
    prefix_autocompletion = secondary_text.substr(0, find_index);
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "secondary & non-prefix");
    return true;
  }

  const bool can_autocomplete_split_url =
      OmniboxFieldTrial::kRichAutocompletionSplitUrlCompletion.Get() &&
      input.text().size() >=
          static_cast<size_t>(
              OmniboxFieldTrial::kRichAutocompletionSplitCompletionMinChar
                  .Get());

  // Try split matching (see comments for |split_autocompletion|) with
  // |primary_text|.
  std::vector<std::pair<size_t, size_t>> input_words;
  if (can_autocomplete_split_url &&
      !(input_words = FindWordsSequentiallyAtWordbreak(primary_text_lower,
                                                       input_text_lower))
           .empty()) {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    split_autocompletion = SplitAutocompletion(
        primary_text_lower,
        TermMatchesToSelections(primary_text_lower.length(), input_words));
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "primary & split");
    return true;
  }

  // Try split matching (see comments for |split_autocompletion|) with
  // |secondary_text|.
  const bool can_autocomplete_split_title =
      OmniboxFieldTrial::kRichAutocompletionSplitTitleCompletion.Get() &&
      input.text().size() >=
          static_cast<size_t>(
              OmniboxFieldTrial::kRichAutocompletionSplitCompletionMinChar
                  .Get());

  if (can_autocomplete_split_title &&
      !(input_words = FindWordsSequentiallyAtWordbreak(secondary_text_lower,
                                                       input_text_lower))
           .empty()) {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    additional_text = primary_text;
    split_autocompletion = SplitAutocompletion(
        secondary_text_lower,
        TermMatchesToSelections(secondary_text_lower.length(), input_words));
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "secondary & split");
    return true;
  }

  // Try matching the prefix of `shortcut_text`.
  if (can_autocomplete_shortcut_text &&
      base::StartsWith(shortcut_text_lower, input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    rich_autocompletion_triggered = true;
    if (counterfactual)
      return false;
    additional_text = primary_text;
    inline_autocompletion = shortcut_text.substr(input_text_lower.length());
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "shortcut text & prefix");
    return true;
  }
  // Don't try matching a non-prefix of `shortcut_text`. Shortcut matches are
  // intended for repeated inputs, i.e. inputs that are prefixes of previous
  // inputs.

  return false;
}

bool AutocompleteMatch::IsEmptyAutocompletion() const {
  return inline_autocompletion.empty() && prefix_autocompletion.empty() &&
         split_autocompletion.Empty();
}

void AutocompleteMatch::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
  dict.Add("fill_into_edit", fill_into_edit);
  dict.Add("additional_text", additional_text);
  dict.Add("destination_url", destination_url);
  dict.Add("keyword", keyword);
}

#if DCHECK_IS_ON()
void AutocompleteMatch::Validate() const {
  std::string provider_name = provider ? provider->GetName() : "None";
  ValidateClassifications(contents, contents_class, provider_name);
  ValidateClassifications(description, description_class, provider_name);
  ValidateClassifications(description_for_shortcuts,
                          description_class_for_shortcuts, provider_name);
}
#endif  // DCHECK_IS_ON()

// static
void AutocompleteMatch::ValidateClassifications(
    const std::u16string& text,
    const ACMatchClassifications& classifications,
    const std::string& provider_name) {
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
