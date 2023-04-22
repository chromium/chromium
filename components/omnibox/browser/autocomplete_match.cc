// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

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
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/third_party/mozilla/url_parse.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace {

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
// Used for `SEARCH_SUGGEST_TAIL` and `NULL_RESULT_MESSAGE` (e.g. starter pack)
// type suggestion icons.
static gfx::VectorIcon empty_icon;
#endif

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

// Check if title, non-prefix, or shortcut rich autocompletion is possible. Both
// must be true:
// 1) Either the feature is enabled for all providers OR the suggestion is from
//    the shortcut provider.
// 2) The input is at least `min_char` long.
bool RichAutocompletionApplicable(bool enabled_all_providers,
                                  bool enabled_shortcut_provider,
                                  size_t min_char,
                                  bool shortcut_provider,
                                  const std::u16string& input_text) {
  return (enabled_all_providers ||
          (shortcut_provider && enabled_shortcut_provider)) &&
         input_text.size() >= min_char;
}

// Gives a basis for match comparison that prefers some providers over others
// while remaining neutral with a default score of zero for most providers.
int GetDeduplicationProviderPreferenceScore(AutocompleteProvider::Type type) {
  const static int shortcuts_preference =
      base::FeatureList::IsEnabled(
          omnibox::kPreferNonShortcutMatchesWhenDeduping)
          ? -1
          : 0;
  const static std::unordered_map<AutocompleteProvider::Type, int>
      provider_preference = {
          {// Prefer live document suggestions. We check provider type instead
           // of match type in order to distinguish live suggestions from the
           // document provider from stale suggestions from the shortcuts
           // providers, because the latter omits changing metadata such as last
           // access date.
           AutocompleteProvider::TYPE_DOCUMENT, 2},
          {// Prefer bookmark suggestions, as 1) their titles may be explicitly
           // set, and 2) they may display enhanced information such as the
           // bookmark folders path.
           AutocompleteProvider::TYPE_BOOKMARK, 1},
          {// Prefer non-shorcut matches over shortcuts, the latter of which may
           // have stale or missing URL titles (the latter from what-you-typed
           // matches).
           AutocompleteProvider::TYPE_SHORTCUTS, shortcuts_preference},
          {// Prefer non-fuzzy matches over fuzzy matches.
           AutocompleteProvider::TYPE_HISTORY_FUZZY, -2},
      };
  const auto it = provider_preference.find(type);
  if (it == provider_preference.end()) {
    return 0;
  }
  return it->second;
}

// Implementation of boost::hash_combine
// http://www.boost.org/doc/libs/1_43_0/doc/html/hash/reference.html#boost.hash_combine
template <typename T>
inline void hash_combine(std::size_t& seed, const T& value) {
  std::hash<T> hasher;
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}  // namespace

template <typename S, typename T>
size_t ACMatchKeyHash<S, T>::operator()(const ACMatchKey<S, T>& key) const {
  size_t seed = 0;
  hash_combine(seed, key.first);
  hash_combine(seed, key.second);
  return seed;
}

// This trick allows implementing ACMatchKeyHash in the implementation file.
template struct ACMatchKeyHash<std::u16string, std::string>;
template struct ACMatchKeyHash<std::string, bool>;

// RichAutocompletionParams ---------------------------------------------------

RichAutocompletionParams::RichAutocompletionParams()
    : enabled(OmniboxFieldTrial::IsRichAutocompletionEnabled()),
      autocomplete_titles(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteTitles.Get()),
      autocomplete_titles_shortcut_provider(
          OmniboxFieldTrial::
              kRichAutocompletionAutocompleteTitlesShortcutProvider.Get()),
      autocomplete_titles_min_char(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteTitlesMinChar
              .Get()),
      autocomplete_non_prefix_all(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.Get()),
      autocomplete_non_prefix_shortcut_provider(
          OmniboxFieldTrial::
              kRichAutocompletionAutocompleteNonPrefixShortcutProvider.Get()),
      autocomplete_non_prefix_min_char(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixMinChar
              .Get()),
      autocomplete_shortcut_text(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteShortcutText.Get()),
      autocomplete_shortcut_text_min_char(
          OmniboxFieldTrial::kRichAutocompletionAutocompleteShortcutTextMinChar
              .Get()),
      counterfactual(
          OmniboxFieldTrial::kRichAutocompletionCounterfactual.Get()),
      autocomplete_prefer_urls_over_prefixes(
          OmniboxFieldTrial::
              kRichAutocompletionAutocompletePreferUrlsOverPrefixes.Get()) {}

RichAutocompletionParams& RichAutocompletionParams::GetParams() {
  static RichAutocompletionParams params;
  return params;
}

void RichAutocompletionParams::ClearParamsForTesting() {
  GetParams() = {};
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
      allowed_to_be_default_match(match.allowed_to_be_default_match),
      destination_url(match.destination_url),
      stripped_destination_url(match.stripped_destination_url),
      image_dominant_color(match.image_dominant_color),
      image_url(match.image_url),
      entity_id(match.entity_id),
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
      actions(match.actions),
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
      suggest_tiles(match.suggest_tiles),
      scoring_signals(match.scoring_signals),
      culled_by_provider(match.culled_by_provider) {}

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
  allowed_to_be_default_match = std::move(match.allowed_to_be_default_match);
  destination_url = std::move(match.destination_url);
  stripped_destination_url = std::move(match.stripped_destination_url);
  image_dominant_color = std::move(match.image_dominant_color);
  image_url = std::move(match.image_url);
  entity_id = std::move(match.entity_id);
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
  actions = std::move(match.actions);
  from_previous = std::move(match.from_previous);
  search_terms_args = std::move(match.search_terms_args);
  post_content = std::move(match.post_content);
  additional_info = std::move(match.additional_info);
  duplicate_matches = std::move(match.duplicate_matches);
  query_tiles = std::move(match.query_tiles);
  suggest_tiles = std::move(match.suggest_tiles);
  scoring_signals = std::move(match.scoring_signals);
  culled_by_provider = std::move(match.culled_by_provider);
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
  allowed_to_be_default_match = match.allowed_to_be_default_match;
  destination_url = match.destination_url;
  stripped_destination_url = match.stripped_destination_url;
  image_dominant_color = match.image_dominant_color;
  image_url = match.image_url;
  entity_id = match.entity_id;
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
  actions = match.actions;
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
  suggest_tiles = match.suggest_tiles;
  scoring_signals = match.scoring_signals;
  culled_by_provider = match.culled_by_provider;

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
    case Type::NULL_RESULT_MESSAGE:
      return empty_icon;

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

    case Type::HISTORY_CLUSTER:
      return omnibox::kJourneysIcon;

    case Type::STARTER_PACK:
      return omnibox::kProductIcon;

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

  // Prefer some providers above others according to score (default is zero).
  const int match1_score =
      GetDeduplicationProviderPreferenceScore(match1.provider->type());
  const int match2_score =
      GetDeduplicationProviderPreferenceScore(match2.provider->type());
  if (match1_score != match2_score) {
    return match1_score > match2_score;
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
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &output, std::max(i->offset, j->offset), i->style | j->style);
    const size_t next_i_offset = (i + 1) == classifications1.end()
                                     ? static_cast<size_t>(-1)
                                     : (i + 1)->offset;
    const size_t next_j_offset = (j + 1) == classifications2.end()
                                     ? static_cast<size_t>(-1)
                                     : (j + 1)->offset;
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
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(serialized_classifications, ",",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DCHECK(!(tokens.size() & 1));  // The number of tokens should be even.
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int classification_offset = 0;
    int classification_style = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &classification_offset) ||
        !base::StringToInt(tokens[i + 1], &classification_style)) {
      NOTREACHED();
      return classifications;
    }
    classifications.push_back(
        ACMatchClassification(classification_offset, classification_style));
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

bool AutocompleteMatch::IsStarterPackType(Type type) {
  return type == AutocompleteMatchType::STARTER_PACK;
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
omnibox::GroupId AutocompleteMatch::GetDefaultGroupId(Type type) {
  if (type == AutocompleteMatchType::TILE_NAVSUGGEST ||
      type == AutocompleteMatchType::TILE_SUGGESTION) {
    return omnibox::GROUP_MOBILE_MOST_VISITED;
  }

  if (type == AutocompleteMatchType::CLIPBOARD_URL ||
      type == AutocompleteMatchType::CLIPBOARD_TEXT ||
      type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    return omnibox::GROUP_MOBILE_CLIPBOARD;
  }

  if (IsStarterPackType(type))
    return omnibox::GROUP_STARTER_PACK;

  if (IsSearchType(type))
    return omnibox::GROUP_SEARCH;

  if (type == AutocompleteMatchType::HISTORY_CLUSTER)
    return omnibox::GROUP_HISTORY_CLUSTER;

  return omnibox::GROUP_OTHER_NAVS;
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
  return (template_url || host.empty())
             ? template_url
             : template_url_service->GetTemplateURLForHost(host);
}

// static
GURL AutocompleteMatch::GURLToStrippedGURL(
    const GURL& url,
    const AutocompleteInput& input,
    const TemplateURLService* template_url_service,
    const std::u16string& keyword,
    const bool keep_search_intent_params,
    const bool normalize_search_terms) {
  if (!url.is_valid())
    return url;

  // Special-case canonicalizing Docs URLs. This logic is self-contained and
  // will not participate in the TemplateURL canonicalization.
  GURL docs_url = DocumentProvider::GetURLForDeduping(url);
  if (docs_url.is_valid())
    return docs_url;

  GURL stripped_destination_url = url;

  // If the destination URL looks like it was generated from a TemplateURL,
  // remove all substitutions other than the search terms and optionally the
  // search intent params. This allows eliminating cases like past search URLs
  // from history that differ only by some obscure query param from each other
  // or from the search/keyword provider matches.
  const TemplateURL* template_url = GetTemplateURLWithKeyword(
      template_url_service, keyword, stripped_destination_url.host());
  if (template_url != nullptr &&
      template_url->SupportsReplacement(
          template_url_service->search_terms_data())) {
    using CacheKey = std::tuple<const TemplateURL*, GURL, bool, bool>;
    static base::LRUCache<CacheKey, GURL> template_cache(30);
    const CacheKey cache_key = {template_url, url, keep_search_intent_params,
                                normalize_search_terms};
    const auto& cached = template_cache.Get(cache_key);
    if (cached != template_cache.end()) {
      stripped_destination_url = cached->second;
    } else if (template_url->KeepSearchTermsInURL(
                   url, template_url_service->search_terms_data(),
                   keep_search_intent_params, normalize_search_terms,
                   &stripped_destination_url)) {
      template_cache.Put(cache_key, stripped_destination_url);
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
       !WordMatchesURLContent(input.terms_prefixed_by_http_or_https(), url))) {
    replacements.SetSchemeStr(url::kHttpScheme);
    needs_replacement = true;
  }

  if (input.parts().ref.is_empty() && url.has_ref()) {
    replacements.ClearRef();
    needs_replacement = true;
  }

  if (needs_replacement)
    stripped_destination_url =
        stripped_destination_url.ReplaceComponents(replacements);
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
  // Other than document suggestions, computing `stripped_destination_url` will
  // have the same result during a match's lifecycle, so it's safe to skip
  // re-computing it if it's already computed. Document provider and history
  // quick provider document suggestions' `stripped_url`s are pre-computed by
  // the document provider, and overwriting them here would be wasteful and, in
  // the case of the document provider, prevent potential deduping.
  if (stripped_destination_url.is_empty()) {
    stripped_destination_url = GURLToStrippedGURL(
        destination_url, input, template_url_service, keyword,
        /*keep_search_intent_params=*/false,
        /*normalize_search_terms=*/
        base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions));
  }
}

bool AutocompleteMatch::IsDocumentSuggestion() {
  const GURL docs_url = DocumentProvider::GetURLForDeduping(destination_url);
  // May as well set `stripped_destination_url` to avoid duplicate computation
  // later in `ComputeStrippedDestinationURL()`. Additionally tracking if the
  // suggestion is not a doc would add more clutter than benefit.
  if (docs_url.is_valid())
    stripped_destination_url = docs_url;
  return docs_url.is_valid();
}

bool AutocompleteMatch::IsActionCompatible() const {
  return type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
         // Attaching to Tail Suggest types looks weird, and is actually
         // technically wrong because the Pedals annotator (and history clusters
         // annotator) both use match.contents. If we do want to turn on Actions
         // for tail suggest in the future, we should switch to using
         // match.fill_into_edit or maybe page title for URL matches, and come
         // up with a UI design for the button in the tail suggest layout.
         type != AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
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
  return GetTemplateURLWithKeyword(template_url_service, keyword,
                                   allow_fallback_to_destination_host
                                       ? destination_url.host()
                                       : std::string());
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
    case AutocompleteMatchType::HISTORY_CLUSTER:
      return OmniboxEventProto::Suggestion::HISTORY_CLUSTER;
    case AutocompleteMatchType::STARTER_PACK:
      return OmniboxEventProto::Suggestion::STARTER_PACK;
    case AutocompleteMatchType::VOICE_SUGGEST:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::PEDAL_DEPRECATED:
    // NULL_RESULT_MESSAGE suggestions cannot be acted upon, so no need to log.
    case AutocompleteMatchType::NULL_RESULT_MESSAGE:
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

void AutocompleteMatch::FilterOmniboxActions(
    const std::vector<OmniboxActionId>& allowed_action_ids) {
  // Short circuit if there's nothing to do.
  if (actions.empty()) {
    return;
  }

  // Find the type of object we can keep.
  auto allowed_action_id_iter =
      base::ranges::find_if(allowed_action_ids, [this](auto allowed_action_id) {
        return GetActionWhere([allowed_action_id](const auto& action) {
                 return action->ActionId() == allowed_action_id;
               }) != nullptr;
      });

  auto allowed_action_id = allowed_action_id_iter != allowed_action_ids.end()
                               ? *allowed_action_id_iter
                               : OmniboxActionId::LAST;

  std::erase_if(actions, [&](const auto& action) {
    return action->ActionId() != allowed_action_id;
  });
}

bool AutocompleteMatch::IsTrivialAutocompletion() const {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE;
}

bool AutocompleteMatch::SupportsDeletion() const {
  return deletable ||
         base::ranges::any_of(duplicate_matches,
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
  res += base::trace_event::EstimateMemoryUsage(entity_id);
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
  // TODO(manukh): There's some duplicate logic between `BetterDuplicate()` and
  //   `UpgradeMatchWithPropertiesFrom()`. This is unavoidable due to having to
  //   taking different fields from different duplicates, rather having 1 match
  //   that's absolutely overrides all other matches. Perhaps we can avoid this
  //   if we join the 2 functions.

  // For Entity Matches, absorb the duplicate match's |allowed_to_be_default|
  // and |inline_autocompletion| properties.
  if (type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY &&
      fill_into_edit == duplicate_match.fill_into_edit &&
      duplicate_match.allowed_to_be_default_match) {
    allowed_to_be_default_match = true;
    if (IsEmptyAutocompletion()) {
      inline_autocompletion = duplicate_match.inline_autocompletion;
      prefix_autocompletion = duplicate_match.prefix_autocompletion;
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

  from_previous = from_previous && duplicate_match.from_previous;

  // Take the `actions` so that they will be presented instead of buried.
  if (actions.empty() && !duplicate_match.actions.empty() &&
      IsActionCompatible()) {
    actions = std::move(duplicate_match.actions);
  }

  // Prefer fresh suggestion text over potentially stale shortcut text for
  // bookmark paths and document metadata. Don't edit the omnibox text (i.e.
  // `fill_into_edit`, `inline_autocompletion`, and `additional_text`) as the
  // duplicate may not be `allowed_to_be_default_match`.
  if (GetDeduplicationProviderPreferenceScore(
          duplicate_match.provider->type()) >
      GetDeduplicationProviderPreferenceScore(provider->type())) {
    contents = duplicate_match.contents;
    contents_class = duplicate_match.contents_class;
    description = duplicate_match.description;
    description_class = duplicate_match.description_class;
    description_for_shortcuts = duplicate_match.description_for_shortcuts;
    description_class_for_shortcuts =
        duplicate_match.description_class_for_shortcuts;
    swap_contents_and_description =
        duplicate_match.swap_contents_and_description;
  }

  // Copy `rich_autocompletion_triggered` for counterfactual logging.
  if (rich_autocompletion_triggered == RichAutocompletionType::kNone) {
    rich_autocompletion_triggered =
        duplicate_match.rich_autocompletion_triggered;
  }

  // Merge scoring signals from duplicate match for ML model scoring and
  // training.
  if (OmniboxFieldTrial::IsLogUrlScoringSignalsEnabled()) {
    MergeScoringSignals(duplicate_match);
  }
}

void AutocompleteMatch::MergeScoringSignals(const AutocompleteMatch& other) {
  if (!other.scoring_signals.has_value()) {
    return;
  }

  if (!scoring_signals.has_value()) {
    scoring_signals = absl::make_optional<ScoringSignals>();
  }

  // Take the maximum.
  if (other.scoring_signals->has_typed_count()) {
    scoring_signals->set_typed_count(std::max(
        scoring_signals->typed_count(), other.scoring_signals->typed_count()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_visit_count()) {
    scoring_signals->set_visit_count(std::max(
        scoring_signals->visit_count(), other.scoring_signals->visit_count()));
  }

  // Take the minimum.
  if (scoring_signals->has_elapsed_time_last_visit_secs() &&
      other.scoring_signals->has_elapsed_time_last_visit_secs()) {
    scoring_signals->set_elapsed_time_last_visit_secs(
        std::min(scoring_signals->elapsed_time_last_visit_secs(),
                 other.scoring_signals->elapsed_time_last_visit_secs()));
  } else if (other.scoring_signals->has_elapsed_time_last_visit_secs()) {
    scoring_signals->set_elapsed_time_last_visit_secs(
        other.scoring_signals->elapsed_time_last_visit_secs());
  }

  // Take the maximum.
  if (other.scoring_signals->has_shortcut_visit_count()) {
    scoring_signals->set_shortcut_visit_count(
        std::max(scoring_signals->shortcut_visit_count(),
                 other.scoring_signals->shortcut_visit_count()));
  }

  // Take the minimum.
  if (scoring_signals->has_shortest_shortcut_len() &&
      other.scoring_signals->has_shortest_shortcut_len()) {
    scoring_signals->set_shortest_shortcut_len(
        std::min(scoring_signals->shortest_shortcut_len(),
                 other.scoring_signals->shortest_shortcut_len()));
  } else if (other.scoring_signals->has_shortest_shortcut_len()) {
    scoring_signals->set_shortest_shortcut_len(
        other.scoring_signals->shortest_shortcut_len());
  }

  // Take the minimum.
  if (scoring_signals->has_elapsed_time_last_shortcut_visit_sec() &&
      other.scoring_signals->has_elapsed_time_last_shortcut_visit_sec()) {
    scoring_signals->set_elapsed_time_last_shortcut_visit_sec(std::min(
        scoring_signals->elapsed_time_last_shortcut_visit_sec(),
        other.scoring_signals->elapsed_time_last_shortcut_visit_sec()));
  } else if (other.scoring_signals
                 ->has_elapsed_time_last_shortcut_visit_sec()) {
    scoring_signals->set_elapsed_time_last_shortcut_visit_sec(
        other.scoring_signals->elapsed_time_last_shortcut_visit_sec());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_host_only()) {
    scoring_signals->set_is_host_only(scoring_signals->is_host_only() ||
                                      other.scoring_signals->is_host_only());
  }

  // Take the maximum.
  if (other.scoring_signals->has_num_bookmarks_of_url()) {
    scoring_signals->set_num_bookmarks_of_url(
        std::max(scoring_signals->num_bookmarks_of_url(),
                 other.scoring_signals->num_bookmarks_of_url()));
  }

  // Take the minimum.
  if (scoring_signals->has_first_bookmark_title_match_position() &&
      other.scoring_signals->has_first_bookmark_title_match_position()) {
    scoring_signals->set_first_bookmark_title_match_position(
        std::min(scoring_signals->first_bookmark_title_match_position(),
                 other.scoring_signals->first_bookmark_title_match_position()));
  } else if (other.scoring_signals->has_first_bookmark_title_match_position()) {
    scoring_signals->set_first_bookmark_title_match_position(
        other.scoring_signals->first_bookmark_title_match_position());
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_bookmark_title_match_length()) {
    scoring_signals->set_total_bookmark_title_match_length(
        std::max(scoring_signals->total_bookmark_title_match_length(),
                 other.scoring_signals->total_bookmark_title_match_length()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_num_input_terms_matched_by_bookmark_title()) {
    scoring_signals->set_num_input_terms_matched_by_bookmark_title(std::max(
        scoring_signals->num_input_terms_matched_by_bookmark_title(),
        other.scoring_signals->num_input_terms_matched_by_bookmark_title()));
  }

  // Take the minimum.
  if (scoring_signals->has_first_url_match_position() &&
      other.scoring_signals->has_first_url_match_position()) {
    scoring_signals->set_first_url_match_position(
        std::min(scoring_signals->first_url_match_position(),
                 other.scoring_signals->first_url_match_position()));
  } else if (other.scoring_signals->has_first_url_match_position()) {
    scoring_signals->set_first_url_match_position(
        other.scoring_signals->first_url_match_position());
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_url_match_length()) {
    scoring_signals->set_total_url_match_length(
        std::max(scoring_signals->total_url_match_length(),
                 other.scoring_signals->total_url_match_length()));
  }

  // Take the OR result.
  if (other.scoring_signals->has_host_match_at_word_boundary()) {
    scoring_signals->set_host_match_at_word_boundary(
        scoring_signals->host_match_at_word_boundary() ||
        other.scoring_signals->host_match_at_word_boundary());
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_host_match_length()) {
    scoring_signals->set_total_host_match_length(
        std::max(scoring_signals->total_host_match_length(),
                 other.scoring_signals->total_host_match_length()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_path_match_length()) {
    scoring_signals->set_total_path_match_length(
        std::max(scoring_signals->total_path_match_length(),
                 other.scoring_signals->total_path_match_length()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_query_or_ref_match_length()) {
    scoring_signals->set_total_query_or_ref_match_length(
        std::max(scoring_signals->total_query_or_ref_match_length(),
                 other.scoring_signals->total_query_or_ref_match_length()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_total_title_match_length()) {
    scoring_signals->set_total_title_match_length(
        std::max(scoring_signals->total_title_match_length(),
                 other.scoring_signals->total_title_match_length()));
  }

  // Take the OR result.
  if (other.scoring_signals->has_has_non_scheme_www_match()) {
    scoring_signals->set_has_non_scheme_www_match(
        scoring_signals->has_non_scheme_www_match() ||
        other.scoring_signals->has_non_scheme_www_match());
  }

  // Take the maximum.
  if (other.scoring_signals->has_num_input_terms_matched_by_title()) {
    scoring_signals->set_num_input_terms_matched_by_title(
        std::max(scoring_signals->num_input_terms_matched_by_title(),
                 other.scoring_signals->num_input_terms_matched_by_title()));
  }

  // Take the maximum.
  if (other.scoring_signals->has_num_input_terms_matched_by_url()) {
    scoring_signals->set_num_input_terms_matched_by_url(
        std::max(scoring_signals->num_input_terms_matched_by_url(),
                 other.scoring_signals->num_input_terms_matched_by_url()));
  }

  // Take the minimum.
  if (scoring_signals->has_length_of_url() &&
      other.scoring_signals->has_length_of_url()) {
    scoring_signals->set_length_of_url(
        std::min(scoring_signals->length_of_url(),
                 other.scoring_signals->length_of_url()));
  } else if (other.scoring_signals->has_length_of_url()) {
    scoring_signals->set_length_of_url(other.scoring_signals->length_of_url());
  }

  // Take the maximum.
  if (other.scoring_signals->has_site_engagement()) {
    scoring_signals->set_site_engagement(
        std::max(scoring_signals->site_engagement(),
                 other.scoring_signals->site_engagement()));
  }

  // Take the OR result.
  if (other.scoring_signals->has_allowed_to_be_default_match()) {
    scoring_signals->set_allowed_to_be_default_match(
        scoring_signals->allowed_to_be_default_match() ||
        other.scoring_signals->allowed_to_be_default_match());
  }
}

bool AutocompleteMatch::TryRichAutocompletion(
    const std::u16string& primary_text,
    const std::u16string& secondary_text,
    const AutocompleteInput& input,
    const std::u16string& shortcut_text) {
  const auto& params = RichAutocompletionParams::GetParams();

  if (!params.enabled)
    return false;

  if (input.prevent_inline_autocomplete())
    return false;

  // Lowercase the strings for case-insensitive comparisons.
  const std::u16string primary_text_lower{base::i18n::ToLower(primary_text)};
  const std::u16string input_text_lower{base::i18n::ToLower(input.text())};

  // Try matching the prefix of |primary_text|.
  if (base::StartsWith(primary_text_lower, input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    if (params.counterfactual)
      return false;
    // This case intentionally doesn't set |rich_autocompletion_triggered| since
    // presumably non-rich autocompletion should also be able to handle this
    // case.
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
      params.autocomplete_titles, params.autocomplete_titles_shortcut_provider,
      params.autocomplete_titles_min_char, !shortcut_text.empty(),
      input.text());
  const bool can_autocomplete_non_prefix = RichAutocompletionApplicable(
      params.autocomplete_non_prefix_all,
      params.autocomplete_non_prefix_shortcut_provider,
      params.autocomplete_non_prefix_min_char, !shortcut_text.empty(),
      input.text());
  const bool can_autocomplete_shortcut_text =
      RichAutocompletionApplicable(false, params.autocomplete_shortcut_text,
                                   params.autocomplete_shortcut_text_min_char,
                                   !shortcut_text.empty(), input.text());

  // All else equal, prefer matching primary over secondary texts and prefixes
  // over non-prefixes. |prefer_primary_non_prefix_over_secondary_prefix|
  // determines whether to prefer matching primary text non-prefixes or
  // secondary text prefixes.
  bool prefer_primary_non_prefix_over_secondary_prefix =
      params.autocomplete_prefer_urls_over_prefixes;

  size_t find_index;

  // A helper to avoid duplicate code. Depending on the
  // |prefer_primary_non_prefix_over_secondary_prefix|, this may be invoked
  // either before or after trying prefix secondary autocompletion.
  auto NonPrefixPrimaryHelper = [&]() {
    rich_autocompletion_triggered = RichAutocompletionType::kUrlNonPrefix;
    if (params.counterfactual)
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

  const std::u16string secondary_text_lower{
      base::i18n::ToLower(secondary_text)};

  // Try matching the prefix of |secondary_text|.
  if (can_autocomplete_titles &&
      base::StartsWith(secondary_text_lower, input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    rich_autocompletion_triggered = RichAutocompletionType::kTitlePrefix;
    if (params.counterfactual)
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
    rich_autocompletion_triggered = RichAutocompletionType::kTitleNonPrefix;
    if (params.counterfactual)
      return false;
    additional_text = primary_text;
    inline_autocompletion =
        secondary_text.substr(find_index + input_text_lower.length());
    prefix_autocompletion = secondary_text.substr(0, find_index);
    allowed_to_be_default_match = true;
    RecordAdditionalInfo("autocompletion", "secondary & non-prefix");
    return true;
  }

  // Try matching the prefix of `shortcut_text`.
  if (can_autocomplete_shortcut_text &&
      base::StartsWith(base::i18n::ToLower(shortcut_text), input_text_lower,
                       base::CompareCase::SENSITIVE)) {
    rich_autocompletion_triggered = RichAutocompletionType::kShortcutTextPrefix;
    if (params.counterfactual)
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
  return inline_autocompletion.empty() && prefix_autocompletion.empty();
}

void AutocompleteMatch::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
  dict.Add("fill_into_edit", fill_into_edit);
  dict.Add("additional_text", additional_text);
  dict.Add("destination_url", destination_url);
  dict.Add("keyword", keyword);
}

OmniboxAction* AutocompleteMatch::GetPrimaryAction() const {
  return actions.empty() ? nullptr : actions[0].get();
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
        << "] is out of bounds for \"" << text
        << "\". Provider: " << provider_name << ".";
    last_offset = i->offset;
  }
}
