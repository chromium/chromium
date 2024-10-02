// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "inline_autocompletion_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/third_party/mozilla/url_parse.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/featured_search_provider.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);
constexpr bool kIsAndroid = BUILDFLAG(IS_ANDROID);

namespace {

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
// Used for `SEARCH_SUGGEST_TAIL` and `NULL_RESULT_MESSAGE` (e.g. starter pack)
// type suggestion icons.

const gfx::VectorIcon& GetEmptyIcon() {
  static const gfx::VectorIcon instance;
  return instance;
}

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
int GetDeduplicationProviderPreferenceScore(
    const AutocompleteProvider* provider) {
  if (!provider) {
    return 0;
  }
  const AutocompleteProvider::Type type = provider->type();

  using ProviderPrefMap = base::flat_map<AutocompleteProvider::Type, int>;
  static const base::NoDestructor<ProviderPrefMap> provider_prefs({
      // Prefer live document suggestions. We check provider type instead
      // of match type in order to distinguish live suggestions from the
      // document provider from stale suggestions from the shortcuts
      // providers, because the latter omits changing metadata such as last
      // access date.
      {AutocompleteProvider::TYPE_DOCUMENT, 2},
      // Prefer bookmark suggestions, as:
      // 1) Their titles may be explicitly set.
      // 2) They may display enhanced information such as the bookmark
      //    folders path.
      {AutocompleteProvider::TYPE_BOOKMARK, 1},
      // Don't let bookmarks override builtins, as that interferes with
      // starter pack matches when user has bookmarked their destination.
      {AutocompleteProvider::TYPE_BUILTIN, kIsDesktop ? 1 : 0},
      // Prefer non-shorcut matches over shortcuts, the latter of which may
      // have stale or missing URL titles (the latter from what-you-typed
      // matches).
      //
      // If the value here becomes a fixed value, then change `provider_prefs`
      // from a NoDestructor to a FixedFlatMap.
      {AutocompleteProvider::TYPE_SHORTCUTS,
       base::FeatureList::IsEnabled(
           omnibox::kPreferNonShortcutMatchesWhenDeduping)
           ? -1
           : 0},
      // Prefer non-fuzzy matches over fuzzy matches.
      {AutocompleteProvider::TYPE_HISTORY_FUZZY, -2},
  });
  const auto it = provider_prefs->find(type);
  return it != provider_prefs->end() ? it->second : 0;
}

// Implementation of boost::hash_combine
// http://www.boost.org/doc/libs/1_43_0/doc/html/hash/reference.html#boost.hash_combine
template <typename T>
inline void hash_combine(std::size_t& seed, const T& value) {
  std::hash<T> hasher;
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}  // namespace

template <typename... Args>
size_t ACMatchKeyHash<Args...>::operator()(
    const ACMatchKey<Args...>& key) const {
  size_t seed = 0;
  // Compute a hash by applying hash_combine to each element of the "key" tuple.
  std::apply([&seed](auto&&... args) { ((hash_combine(seed, args)), ...); },
             key);
  return seed;
}

// This trick allows implementing ACMatchKeyHash in the implementation file.
// Every unique specialization of ACMatchKey should have a corresponding
// declaration here.
template struct ACMatchKeyHash<std::u16string,
                               std::string>;  // base_search_provider
template struct ACMatchKeyHash<std::string, bool, bool>;  // autocomplete_result

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
      extra_headers(match.extra_headers),
      image_dominant_color(match.image_dominant_color),
      image_url(match.image_url),
      entity_id(match.entity_id),
      website_uri(match.website_uri),
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
      answer_template(match.answer_template),
      answer_type(match.answer_type),
      transition(match.transition),
      type(match.type),
      suggest_type(match.suggest_type),
      subtypes(match.subtypes),
      has_tab_match(match.has_tab_match),
      associated_keyword(match.associated_keyword
                             ? new AutocompleteMatch(*match.associated_keyword)
                             : nullptr),
      keyword(match.keyword),
      from_keyword(match.from_keyword),
      actions(match.actions),
      takeover_action(match.takeover_action),
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
      suggest_tiles(match.suggest_tiles),
      scoring_signals(match.scoring_signals),
      culled_by_provider(match.culled_by_provider),
      shortcut_boosted(match.shortcut_boosted),
      iph_type(match.iph_type),
      iph_link_text(match.iph_link_text),
      iph_link_url(match.iph_link_url),
      feedback_type(match.feedback_type) {}

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
  extra_headers = std::move(match.extra_headers);
  image_dominant_color = std::move(match.image_dominant_color);
  image_url = std::move(match.image_url);
  entity_id = std::move(match.entity_id);
  website_uri = std::move(match.website_uri);
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
  answer_template = std::move(match.answer_template);
  answer_type = std::move(match.answer_type);
  transition = std::move(match.transition);
  type = std::move(match.type);
  suggest_type = std::move(match.suggest_type);
  subtypes = std::move(match.subtypes);
  has_tab_match = std::move(match.has_tab_match);
  associated_keyword = std::move(match.associated_keyword);
  keyword = std::move(match.keyword);
  from_keyword = std::move(match.from_keyword);
  actions = std::move(match.actions);
  takeover_action = std::move(match.takeover_action);
  from_previous = std::move(match.from_previous);
  search_terms_args = std::move(match.search_terms_args);
  post_content = std::move(match.post_content);
  additional_info = std::move(match.additional_info);
  duplicate_matches = std::move(match.duplicate_matches);
  suggest_tiles = std::move(match.suggest_tiles);
  scoring_signals = std::move(match.scoring_signals);
  culled_by_provider = std::move(match.culled_by_provider);
  shortcut_boosted = std::move(match.shortcut_boosted);
  iph_type = std::move(match.iph_type);
  iph_link_text = std::move(match.iph_link_text);
  iph_link_url = std::move(match.iph_link_url);
  feedback_type = std::move(match.feedback_type);
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
  extra_headers = match.extra_headers;
  image_dominant_color = match.image_dominant_color;
  image_url = match.image_url;
  entity_id = match.entity_id;
  website_uri = match.website_uri;
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
  answer_template = match.answer_template;
  answer_type = match.answer_type;
  transition = match.transition;
  type = match.type;
  suggest_type = match.suggest_type;
  subtypes = match.subtypes;
  has_tab_match = match.has_tab_match;
  associated_keyword.reset(
      match.associated_keyword
          ? new AutocompleteMatch(*match.associated_keyword)
          : nullptr);
  keyword = match.keyword;
  from_keyword = match.from_keyword;
  actions = match.actions;
  takeover_action = match.takeover_action;
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
  suggest_tiles = match.suggest_tiles;
  scoring_signals = match.scoring_signals;
  culled_by_provider = match.culled_by_provider;
  shortcut_boosted = match.shortcut_boosted;
  iph_type = match.iph_type;
  iph_link_text = match.iph_link_text;
  iph_link_url = match.iph_link_url;
  feedback_type = match.feedback_type;

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
  switch (static_cast<omnibox::AnswerType>(type)) {
    case omnibox::ANSWER_TYPE_CURRENCY:
      return omnibox::kAnswerCurrencyChromeRefreshIcon;
    case omnibox::ANSWER_TYPE_DICTIONARY:
      return omnibox::kAnswerDictionaryChromeRefreshIcon;
    case omnibox::ANSWER_TYPE_FINANCE:
      return omnibox::kAnswerFinanceChromeRefreshIcon;
    case omnibox::ANSWER_TYPE_SUNRISE_SUNSET:
      return omnibox::kAnswerSunriseChromeRefreshIcon;
    case omnibox::ANSWER_TYPE_TRANSLATION:
      return omnibox::kAnswerTranslationChromeRefreshIcon;
    case omnibox::ANSWER_TYPE_WHEN_IS:
      return omnibox::kAnswerWhenIsChromeRefreshIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

const gfx::VectorIcon& AutocompleteMatch::GetVectorIcon(
    bool is_bookmark,
    const TemplateURL* turl) const {
  if (is_bookmark)
    return omnibox::kBookmarkChromeRefreshIcon;
  if (answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
    return AnswerTypeToAnswerIcon(answer_type);
  }

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
    case Type::TILE_MOST_VISITED_SITE:
    case Type::OPEN_TAB:
    case Type::HISTORY_EMBEDDINGS:
    case Type::FEATURED_ENTERPRISE_SEARCH:
      return omnibox::kPageChromeRefreshIcon;

    case Type::SEARCH_SUGGEST:
      return IsTrendSuggestion() ? omnibox::kTrendingUpChromeRefreshIcon
                                 : vector_icons::kSearchChromeRefreshIcon;

    case Type::PEDAL:
      return takeover_action ? takeover_action->GetVectorIcon()
                             : vector_icons::kSearchChromeRefreshIcon;

    case Type::SEARCH_WHAT_YOU_TYPED:
    case Type::SEARCH_SUGGEST_ENTITY:
    case Type::SEARCH_SUGGEST_PROFILE:
    case Type::SEARCH_OTHER_ENGINE:
    case Type::CONTACT_DEPRECATED:
    case Type::VOICE_SUGGEST:
    case Type::CLIPBOARD_TEXT:
    case Type::CLIPBOARD_IMAGE:
    case Type::TILE_SUGGESTION:
    case Type::TILE_REPEATABLE_QUERY:
      return vector_icons::kSearchChromeRefreshIcon;

    case Type::SEARCH_HISTORY:
    case Type::SEARCH_SUGGEST_PERSONALIZED:
      DCHECK(IsSearchHistoryType(type));
      return vector_icons::kHistoryChromeRefreshIcon;

    case Type::EXTENSION_APP_DEPRECATED:
      return omnibox::kExtensionAppIcon;

    case Type::CALCULATOR:
      return omnibox::kCalculatorChromeRefreshIcon;

    case Type::NULL_RESULT_MESSAGE:
      // Select the icon according to the type of IPH. Otherwise (for No Results
      // Found), fallthrough to use the empty icon.
      switch (iph_type) {
        case IphType::kNone:
          return GetEmptyIcon();
        case IphType::kGemini:
          return omnibox::kSparkIcon;
        case IphType::kFeaturedEnterpriseSearch:
          return omnibox::kEnterpriseIcon;
        case IphType::kHistoryEmbeddingsSettingsPromo:
          return omnibox::kSparkIcon;
        case IphType::kHistoryEmbeddingsDisclaimer:
          return GetEmptyIcon();
        case IphType::kHistoryScopePromo:
          return vector_icons::kHistoryChromeRefreshIcon;
        case IphType::kHistoryEmbeddingsScopePromo:
          return omnibox::kSparkIcon;
      }

    case Type::SEARCH_SUGGEST_TAIL:
      return GetEmptyIcon();

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
          return omnibox::kPageChromeRefreshIcon;
      }

    case Type::HISTORY_CLUSTER:
      return omnibox::kJourneysChromeRefreshIcon;

    case Type::STARTER_PACK:
      if (turl) {
        switch (turl->GetBuiltinEngineType()) {
          case KEYWORD_MODE_STARTER_PACK_BOOKMARKS:
            return omnibox::kStarActiveChromeRefreshIcon;
          case KEYWORD_MODE_STARTER_PACK_HISTORY:
            return vector_icons::kHistoryChromeRefreshIcon;
          case KEYWORD_MODE_STARTER_PACK_TABS:
            return omnibox::kProductChromeRefreshIcon;
          case KEYWORD_MODE_STARTER_PACK_GEMINI:
            return omnibox::kSparkIcon;
          default:
            break;
        }
      }
      return omnibox::kProductChromeRefreshIcon;

    case Type::NUM_TYPES:
      NOTREACHED();
  }
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
  if (kIsDesktop) {
    // Prefer featured Enterprise site search matches.
    if (match1.type == AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH &&
        match2.type != AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH) {
      return true;
    }
    if (match1.type != AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH &&
        match2.type == AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH) {
      return false;
    }

    // Prefer starter pack matches.
    if (match1.type == AutocompleteMatchType::STARTER_PACK &&
        match2.type != AutocompleteMatchType::STARTER_PACK) {
      return true;
    }
    if (match1.type != AutocompleteMatchType::STARTER_PACK &&
        match2.type == AutocompleteMatchType::STARTER_PACK) {
      return false;
    }
  }

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
      GetDeduplicationProviderPreferenceScore(match1.provider);
  const int match2_score =
      GetDeduplicationProviderPreferenceScore(match2.provider);
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
  std::vector<std::string_view> tokens =
      base::SplitStringPiece(serialized_classifications, ",",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DCHECK(!(tokens.size() & 1));  // The number of tokens should be even.
  for (size_t i = 0; i < tokens.size(); i += 2) {
    int classification_offset = 0;
    int classification_style = ACMatchClassification::NONE;
    if (!base::StringToInt(tokens[i], &classification_offset) ||
        !base::StringToInt(tokens[i + 1], &classification_style)) {
      NOTREACHED_IN_MIGRATION();
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
bool AutocompleteMatch::IsFeaturedEnterpriseSearchType(Type type) {
  return type == AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH;
}

// static
bool AutocompleteMatch::IsFeaturedSearchType(Type type) {
  return IsStarterPackType(type) || IsFeaturedEnterpriseSearchType(type);
}

// static
bool AutocompleteMatch::IsSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::SEARCH_HISTORY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST ||
         type == AutocompleteMatchType::SEARCH_OTHER_ENGINE ||
         type == AutocompleteMatchType::CALCULATOR ||
         type == AutocompleteMatchType::VOICE_SUGGEST ||
         type == AutocompleteMatchType::CLIPBOARD_TEXT ||
         type == AutocompleteMatchType::CLIPBOARD_IMAGE ||
         IsSpecializedSearchType(type);
}

// static
bool AutocompleteMatch::IsSpecializedSearchType(Type type) {
  return type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
         type == AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED ||
         type == AutocompleteMatchType::TILE_SUGGESTION ||
         type == AutocompleteMatchType::TILE_REPEATABLE_QUERY ||
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

bool AutocompleteMatch::IsClipboardType(Type type) {
  return type == AutocompleteMatchType::CLIPBOARD_URL ||
         type == AutocompleteMatchType::CLIPBOARD_TEXT ||
         type == AutocompleteMatchType::CLIPBOARD_IMAGE;
}

// static
bool AutocompleteMatch::ShouldBeSkippedForGroupBySearchVsUrl(Type type) {
  return IsClipboardType(type) ||
         type == AutocompleteMatchType::TILE_NAVSUGGEST ||
         type == AutocompleteMatchType::TILE_MOST_VISITED_SITE ||
         type == AutocompleteMatchType::TILE_REPEATABLE_QUERY ||
         type == AutocompleteMatchType::TILE_SUGGESTION;
}

// static
omnibox::GroupId AutocompleteMatch::GetDefaultGroupId(Type type) {
  if (type == AutocompleteMatchType::TILE_NAVSUGGEST ||
      type == AutocompleteMatchType::TILE_SUGGESTION ||
      type == AutocompleteMatchType::TILE_MOST_VISITED_SITE ||
      type == AutocompleteMatchType::TILE_REPEATABLE_QUERY) {
    return omnibox::GROUP_MOBILE_MOST_VISITED;
  }

  if (type == AutocompleteMatchType::CLIPBOARD_URL ||
      type == AutocompleteMatchType::CLIPBOARD_TEXT ||
      type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    return omnibox::GROUP_MOBILE_CLIPBOARD;
  }

  // TODO(b/309458788): Create a separate group for Featured Enterprise search.
  //                    In the meantime, reusing the same group as starter pack
  //                    is OK, since they are shown together anyway.
  if (IsStarterPackType(type) || IsFeaturedEnterpriseSearchType(type)) {
    return omnibox::GROUP_STARTER_PACK;
  }

  if (IsSearchType(type))
    return omnibox::GROUP_SEARCH;

  if (type == AutocompleteMatchType::HISTORY_CLUSTER)
    return omnibox::GROUP_HISTORY_CLUSTER;

  if (type == AutocompleteMatchType::NULL_RESULT_MESSAGE) {
    return omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP;
  }

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
  if (template_url && template_url->SupportsReplacement(
                          template_url_service->search_terms_data())) {
    using CacheKey = std::tuple<const TemplateURL*, GURL, bool, bool>;
    static base::NoDestructor<base::LRUCache<CacheKey, GURL>> template_cache(
        30);
    const CacheKey cache_key = {template_url, url, keep_search_intent_params,
                                normalize_search_terms};
    const auto& cached = template_cache->Get(cache_key);
    if (cached != template_cache->end()) {
      stripped_destination_url = cached->second;
    } else if (template_url->KeepSearchTermsInURL(
                   url, template_url_service->search_terms_data(),
                   keep_search_intent_params, normalize_search_terms,
                   &stripped_destination_url)) {
      template_cache->Put(cache_key, stripped_destination_url);
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
    replacements.SetHostStr(std::string_view(host).substr(prefix_len));
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
  if (!template_url) {
    return;
  }

  SearchEngineType search_engine_type =
      match.destination_url.is_valid()
          ? SearchEngineUtils::GetEngineType(match.destination_url)
          : SEARCH_ENGINE_OTHER;
  UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngineType", search_engine_type,
                            SEARCH_ENGINE_MAX);

  if (template_url->created_by_policy() !=
      TemplateURLData::CreatedByPolicy::kNoPolicy) {
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngineType.SetByEnterprisePolicy",
                              search_engine_type, SEARCH_ENGINE_MAX);

    switch (template_url->created_by_policy()) {
      case TemplateURLData::CreatedByPolicy::kDefaultSearchProvider:
        UMA_HISTOGRAM_ENUMERATION(
            "Omnibox.SearchEngineType.SetByEnterprisePolicy."
            "DefaultSearchProvider",
            search_engine_type, SEARCH_ENGINE_MAX);
        break;

      case TemplateURLData::CreatedByPolicy::kSiteSearch:
        UMA_HISTOGRAM_ENUMERATION(
            "Omnibox.SearchEngineType.SetByEnterprisePolicy."
            "SiteSearchSettings",
            search_engine_type, SEARCH_ENGINE_MAX);
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  } else if (template_url->type() ==
             TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    if (template_url_service->GetDefaultSearchProvider() == template_url) {
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchEngineType.SetByExtension."
          "SettingsOverrideDefaultSearchProvider",
          search_engine_type, SEARCH_ENGINE_MAX);
    } else {
      // TODO(crbug.com/367330704): Find an extension that uses the Chrome
      //   Settings override to add an engine but that doesn't set is_default to
      //   true in order to manually test this code path.
      UMA_HISTOGRAM_ENUMERATION(
          "Omnibox.SearchEngineType.SetByExtension."
          "SettingsOverrideNonDefaultSearchProvider",
          search_engine_type, SEARCH_ENGINE_MAX);
    }
  } else if (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION) {
    // The omnibox API only allows for keyword/site search entries, not default
    // search engines so only one type of histogram needs to be recorded here.
    //
    // TODO(crbug.com/367330704): Figure out why this code path isn't being
    //   reached when issuing an omnibox API extension search. Tested with
    //   https://chromewebstore.google.com/detail/github-omnibox/pdifemobhgmmnjlfjigebjkkbhllgcgp
    UMA_HISTOGRAM_ENUMERATION(
        "Omnibox.SearchEngineType.SetByExtension.OmniboxAPI",
        search_engine_type, SEARCH_ENGINE_MAX);
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

bool AutocompleteMatch::HasInstantKeyword(
    TemplateURLService* template_url_service) const {
  if (!associated_keyword) {
    return false;
  }
  TemplateURL* turl =
      associated_keyword->GetTemplateURL(template_url_service, false);
  if (!turl) {
    return false;
  }
  // Note, starter pack keywords with '@' prefix removed do not get
  // the special instant keyword UX, by design.
  return (turl->starter_pack_id() != 0 || turl->featured_by_policy()) &&
         turl->keyword().starts_with(u'@');
}

void AutocompleteMatch::GetKeywordUIState(
    TemplateURLService* template_url_service,
    bool is_history_embeddings_enabled,
    std::u16string* keyword_out,
    std::u16string* keyword_placeholder_out,
    bool* is_keyword_hint) const {
  *is_keyword_hint = associated_keyword != nullptr;
  keyword_out->assign(
      *is_keyword_hint
          ? associated_keyword->keyword
          : GetSubstitutingExplicitlyInvokedKeyword(template_url_service));
  *keyword_placeholder_out = GetKeywordPlaceholder(
      template_url_service, is_history_embeddings_enabled);
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

std::u16string AutocompleteMatch::GetKeywordPlaceholder(
    TemplateURLService* template_url_service,
    bool is_history_embeddings_enabled) const {
#if BUILDFLAG(IS_IOS)
  // `kOmniboxScoped` isn't defined on iOS and all history embedding subfeatures
  // are disabled on iOS.
  return u"";
#else
  if (!history_embeddings::kOmniboxScoped.Get())
    return u"";

  const TemplateURL* t_url = GetTemplateURL(template_url_service, false);
  if (!t_url)
    return u"";
  int message_id;
  switch (t_url->starter_pack_id()) {
    case TemplateURLStarterPackData::kBookmarks:
      message_id = IDS_OMNIBOX_BOOKMARKS_SCOPE_PLACEHOLDER_TEXT;
      break;
    case TemplateURLStarterPackData::kHistory:
      message_id = is_history_embeddings_enabled
                       ? IDS_OMNIBOX_HISTORY_EMBEDDINGS_SCOPE_PLACEHOLDER_TEXT
                       : IDS_OMNIBOX_HISTORY_SCOPE_PLACEHOLDER_TEXT;
      break;
    case TemplateURLStarterPackData::kTabs:
      message_id = IDS_OMNIBOX_TABS_SCOPE_PLACEHOLDER_TEXT;
      break;
    case TemplateURLStarterPackData::kGemini:
      message_id = IDS_OMNIBOX_GEMINI_SCOPE_PLACEHOLDER_TEXT;
      break;
    default:
      return u"";
  }
  return l10n_util::GetStringUTF16(message_id);
#endif
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
  if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
      answer_template.has_value()) {
    return GURL(answer_template->answers(0).image().url());
  }
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
                                             double value) {
  RecordAdditionalInfo(property, base::NumberToString(value));
}

void AutocompleteMatch::RecordAdditionalInfo(const std::string& property,
                                             base::Time value) {
  RecordAdditionalInfo(
      property, base::StringPrintf("%d hours ago",
                                   (base::Time::Now() - value).InHours()));
}

std::string AutocompleteMatch::GetAdditionalInfoForDebugging(
    const std::string& property) const {
  auto i(additional_info.find(property));
  return (i == additional_info.end()) ? std::string() : i->second;
}

metrics::OmniboxEventProto::ProviderType
AutocompleteMatch::GetOmniboxEventProviderType(int action_index) const {
  using metrics::OmniboxEventProto;

  // Mostly the `provider` provides the provider type below, but a few
  // action types have meaningful overrides here.
  if (action_index >= 0 && static_cast<size_t>(action_index) < actions.size()) {
    switch (actions[action_index]->ActionId()) {
      case OmniboxActionId::PEDAL:
        return OmniboxEventProto::PEDALS;
      case OmniboxActionId::TAB_SWITCH:
        return OmniboxEventProto::TAB_SWITCH;
      default:
        break;
    }
  }

  if (provider) {
    return provider->AsOmniboxEventProviderType();
  }

  return OmniboxEventProto::UNKNOWN_PROVIDER;
}

metrics::OmniboxEventProto::Suggestion::ResultType
AutocompleteMatch::GetOmniboxEventResultType(int action_index) const {
  using metrics::OmniboxEventProto;

  if (action_index >= 0 && static_cast<size_t>(action_index) < actions.size()) {
    switch (actions[action_index]->ActionId()) {
      case OmniboxActionId::PEDAL:
        return OmniboxEventProto::Suggestion::PEDAL;
      case OmniboxActionId::TAB_SWITCH:
        return OmniboxEventProto::Suggestion::TAB_SWITCH;
      case OmniboxActionId::HISTORY_CLUSTERS:
      case OmniboxActionId::ACTION_IN_SUGGEST:
      case OmniboxActionId::ANSWER_ACTION:
        // Preserve existing behavior by continuing on to use the match `type`.
        break;
      case OmniboxActionId::UNKNOWN:
      case OmniboxActionId::LAST:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

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
    case AutocompleteMatchType::TILE_REPEATABLE_QUERY:
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
    case AutocompleteMatchType::TILE_MOST_VISITED_SITE:
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::OPEN_TAB:
      return OmniboxEventProto::Suggestion::OPEN_TAB;
    case AutocompleteMatchType::HISTORY_CLUSTER:
      return OmniboxEventProto::Suggestion::HISTORY_CLUSTER;
    case AutocompleteMatchType::STARTER_PACK:
      return OmniboxEventProto::Suggestion::STARTER_PACK;
    case AutocompleteMatchType::VOICE_SUGGEST:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST;
    case AutocompleteMatchType::PEDAL:
      return OmniboxEventProto::Suggestion::PEDAL;
    case AutocompleteMatchType::HISTORY_EMBEDDINGS:
      return OmniboxEventProto::Suggestion::HISTORY_EMBEDDINGS;
    case AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH:
      return OmniboxEventProto::Suggestion::FEATURED_ENTERPRISE_SEARCH;
    case AutocompleteMatchType::NULL_RESULT_MESSAGE:
      return OmniboxEventProto::Suggestion::NULL_RESULT_MESSAGE;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      break;
  }
  DUMP_WILL_BE_NOTREACHED() << "Unknown AutocompleteMatchType: " << type;
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

bool AutocompleteMatch::IsVerbatimUrlSuggestion() const {
  return type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         base::ranges::any_of(duplicate_matches, [](const auto& match) {
           return match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED;
         });
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

int AutocompleteMatch::GetSortingOrder() const {
  if (IsFeaturedEnterpriseSearchType(type)) {
    return 0;
  }

  if (IsStarterPackType(type)) {
    return 1;
  }

  if constexpr (kIsAndroid) {
    if (IsClipboardType(type)) {
      return 1;
    }
  }

#if !BUILDFLAG(IS_IOS)
  // Group history cluster suggestions with searches.
  if (type == AutocompleteMatchType::HISTORY_CLUSTER) {
    return 3;
  }
#endif  // !BUILDFLAG(IS_IOS)
  if (answer_template && actions.size() > 0 &&
      OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.Get()) {
    return 4;
  }
  if (IsSearchType(type)) {
    return 3;
  }
  // Group boosted shortcuts above searches.
  if (omnibox_feature_configs::ShortcutBoosting::Get().group_with_searches &&
      shortcut_boosted) {
    return 2;
  }
  if (IsIPHSuggestion())
    return 5;
  return 4;
}

bool AutocompleteMatch::IsMlSignalLoggingEligible() const {
  const auto& ml_config = OmniboxFieldTrial::GetMLConfig();
  if (answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
    return false;
  }
  return type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
         type == AutocompleteMatchType::HISTORY_URL ||
         type == AutocompleteMatchType::HISTORY_TITLE ||
         type == AutocompleteMatchType::BOOKMARK_TITLE ||
         type == AutocompleteMatchType::NAVSUGGEST ||
         type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED ||
         type == AutocompleteMatchType::TILE_NAVSUGGEST ||
         (ml_config.shortcut_document_signals &&
          type == AutocompleteMatchType::DOCUMENT_SUGGESTION &&
          relevance != 0) ||
         AutocompleteMatch::IsSearchType(type) ||
         AutocompleteMatch::IsVerbatimType();
}

bool AutocompleteMatch::IsMlScoringEligible() const {
  if (!IsMlSignalLoggingEligible() || !scoring_signals.has_value()) {
    return false;
  }

  // Do not apply ML scoring to calculator or answer suggestions as the ML model
  // currently doesn't provide accurate scores for suggestions that have a low
  // click-through rate.
  if (type == AutocompleteMatchType::CALCULATOR ||
      answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED) {
    return false;
  }

  // Do not apply ML scoring to stale suggestions sourced from the
  // DocumentProvider cache.
  if (type == AutocompleteMatchType::DOCUMENT_SUGGESTION && relevance == 0) {
    return false;
  }

  const auto& ml_config = OmniboxFieldTrial::GetMLConfig();

  // Search suggestions are conditionally eligible for ML scoring.
  if (AutocompleteMatch::IsSearchType(type)) {
    return ml_config.enable_ml_scoring_for_searches;
  }

  // Verbatim URL suggestions are conditionally eligible for ML scoring.
  // A "verbatim URL" suggestion is any suggestion that is UWYT itself or has
  // been deduped with a UWYT suggestion.
  if (AutocompleteMatch::IsVerbatimUrlSuggestion()) {
    return ml_config.enable_ml_scoring_for_verbatim_urls;
  }

  // Certain suggestion types are manually excluded from ML scoring (since
  // applying ML scoring to these suggestions currently results in suboptimal
  // behavior).
  if (type == AutocompleteMatchType::NAVSUGGEST ||
      type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED ||
      type == AutocompleteMatchType::TILE_NAVSUGGEST) {
    return false;
  }

  // If any of the duplicates under this match are ineligible for ML scoring,
  // then the top-level match (this) is also considered ineligible for ML
  // scoring.
  if (base::ranges::any_of(duplicate_matches, [](const auto& match) {
        return !match.IsMlScoringEligible();
      })) {
    return false;
  }

  return true;
}

bool AutocompleteMatch::IsTrendSuggestion() const {
  return subtypes.contains(/*omnibox::SUBTYPE_TRENDS=*/143);
}

bool AutocompleteMatch::IsIPHSuggestion() const {
  return iph_type != IphType::kNone;
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

  OmniboxActionId allowed_action_id =
      allowed_action_id_iter != allowed_action_ids.end()
          ? *allowed_action_id_iter
          : OmniboxActionId::LAST;

  std::erase_if(actions, [&](const auto& action) {
    return action->ActionId() != allowed_action_id;
  });
}

void AutocompleteMatch::FilterAndSortActionsInSuggest() {
  if (actions.empty()) {
    return;
  }

  // Sort: Call -> Directions -> Reviews, or Reviews -> Directions -> Call.
  bool sort_descending =
      OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.Get();
  auto less_comparator = [sort_descending](auto k1, auto k2) -> bool {
    bool is_less_ascending = (k1 == omnibox::ActionInfo_ActionType_CALL) ||
                             (k2 == omnibox::ActionInfo_ActionType_REVIEWS);
    return is_less_ascending ^ sort_descending;
  };
  std::multimap<omnibox::ActionInfo::ActionType, scoped_refptr<OmniboxAction>,
                decltype(less_comparator)>
      actions_in_suggest_to_reinsert(less_comparator);

  // Collect all Actions in Suggest.
  omnibox::ActionInfo::ActionType remove_action_type =
      OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.Get();
  std::erase_if(actions, [&actions_in_suggest_to_reinsert, remove_action_type](
                             const scoped_refptr<OmniboxAction>& action) {
    auto* ais = OmniboxActionInSuggest::FromAction(action.get());
    if (ais != nullptr && ais->Type() != remove_action_type) {
      actions_in_suggest_to_reinsert.emplace(ais->Type(), action);
    }
    return ais != nullptr;
  });

  for (auto pair : actions_in_suggest_to_reinsert) {
    actions.emplace_back(std::move(pair.second));
  }
}

void AutocompleteMatch::RemoveAnswerActions() {
  if (actions.empty()) {
    return;
  }

  std::erase_if(actions, [&](const auto& action) {
    auto* ans_action = OmniboxAnswerAction::FromAction(action.get());
    return ans_action != nullptr;
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
  if (input.text().starts_with('@') && type != Type::SEARCH_WHAT_YOU_TYPED) {
    // @ inputs are very special. The only kind of match that can be default is
    // a search-what-you-typed sentinel suggestion, so as to not distract from
    // the starter pack suggestions. Note: There may be some edge cases to
    // consider if more providers are updated to use of this method; then we
    // may want to avoid applying this rule when in keyword mode.
    allowed_to_be_default_match = false;
  } else if (IsEmptyAutocompletion()) {
    allowed_to_be_default_match = true;
  } else if (input.prevent_inline_autocomplete()) {
    allowed_to_be_default_match = false;
  } else if (input.text().empty() ||
             !base::IsUnicodeWhitespace(input.text().back())) {
    allowed_to_be_default_match = true;
  } else {
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
  res += base::trace_event::EstimateMemoryUsage(extra_headers);
  res += base::trace_event::EstimateMemoryUsage(image_dominant_color);
  res += base::trace_event::EstimateMemoryUsage(image_url);
  res += base::trace_event::EstimateMemoryUsage(entity_id);
  res += base::trace_event::EstimateMemoryUsage(website_uri);
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
    suggest_type = duplicate_match.suggest_type;
  }

  // And always absorb the higher relevance score of duplicates.
  if (duplicate_match.relevance > relevance) {
    RecordAdditionalInfo(kACMatchPropertyScoreBoostedFrom, relevance);
    relevance = duplicate_match.relevance;
    shortcut_boosted |= duplicate_match.shortcut_boosted;
  }

  from_previous = from_previous && duplicate_match.from_previous;

  // Absorb the `actions` and `takeover_action` so they won't be buried.
  // Don't absorb answer actions; they should always be created fresh.
  if (actions.empty() && !duplicate_match.actions.empty() &&
      IsActionCompatible() &&
      OmniboxAnswerAction::FromAction(duplicate_match.actions[0].get()) ==
          nullptr) {
    actions = std::move(duplicate_match.actions);
    takeover_action = std::move(duplicate_match.takeover_action);
  }

  // Prefer fresh suggestion text over potentially stale shortcut text for
  // bookmark paths and document metadata. Don't edit the omnibox text (i.e.
  // `fill_into_edit`, `inline_autocompletion`, and `additional_text`) as the
  // duplicate may not be `allowed_to_be_default_match`.
  if (GetDeduplicationProviderPreferenceScore(duplicate_match.provider) >
      GetDeduplicationProviderPreferenceScore(provider)) {
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

  // Merge ML scoring signals from duplicate match when appropriate.
  if (OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled() &&
      IsMlSignalLoggingEligible()) {
    MergeScoringSignals(duplicate_match);
  }
}

void AutocompleteMatch::MergeScoringSignals(const AutocompleteMatch& other) {
  // Keep consistent:
  // - omnibox_event.proto `ScoringSignals`
  // - omnibox_scoring_signals.proto `OmniboxScoringSignals`
  // - autocomplete_scoring_model_handler.cc
  //   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
  // - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
  // - autocomplete_controller.cc `RecordScoringSignalCoverageForProvider()`
  // - omnibox_metrics_provider.cc `GetScoringSignalsForLogging()`
  // - omnibox.mojom `struct Signals`
  // - omnibox_page_handler.cc
  //   `TypeConverter<AutocompleteMatch::ScoringSignals, mojom::SignalsPtr>`
  // - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
  //   AutocompleteMatch::ScoringSignals>`
  // - omnibox_util.ts `signalNames`
  // - omnibox/histograms.xml
  //   `Omnibox.URLScoringModelExecuted.ScoringSignalCoverage`

  if (!other.scoring_signals.has_value()) {
    return;
  }

  // Records the ACMatch type of the duplicate match when two or more matches
  // with different ml scoring signals are merged.
  const char kACMatchPropertyScoringSignalsMerged[] = "Scoring signals merged";
  RecordAdditionalInfo(
      kACMatchPropertyScoringSignalsMerged,
      GetAdditionalInfoForDebugging(kACMatchPropertyScoringSignalsMerged) +
          AutocompleteMatchType::ToString(other.type) + ", " +
          other.GetAdditionalInfoForDebugging(
              kACMatchPropertyScoringSignalsMerged));

  if (!scoring_signals.has_value()) {
    scoring_signals = std::make_optional<ScoringSignals>();
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

  // Take the maximum.
  if (other.scoring_signals->has_search_suggest_relevance()) {
    scoring_signals->set_search_suggest_relevance(
        std::max(scoring_signals->search_suggest_relevance(),
                 other.scoring_signals->search_suggest_relevance()));
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_search_suggest_entity()) {
    scoring_signals->set_is_search_suggest_entity(
        scoring_signals->is_search_suggest_entity() ||
        other.scoring_signals->is_search_suggest_entity());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_verbatim()) {
    scoring_signals->set_is_verbatim(scoring_signals->is_verbatim() ||
                                     other.scoring_signals->is_verbatim());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_navsuggest()) {
    scoring_signals->set_is_navsuggest(scoring_signals->is_navsuggest() ||
                                       other.scoring_signals->is_navsuggest());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_search_suggest_tail()) {
    scoring_signals->set_is_search_suggest_tail(
        scoring_signals->is_search_suggest_tail() ||
        other.scoring_signals->is_search_suggest_tail());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_answer_suggest()) {
    scoring_signals->set_is_answer_suggest(
        scoring_signals->is_answer_suggest() ||
        other.scoring_signals->is_answer_suggest());
  }

  // Take the OR result.
  if (other.scoring_signals->has_is_calculator_suggest()) {
    scoring_signals->set_is_calculator_suggest(
        scoring_signals->is_calculator_suggest() ||
        other.scoring_signals->is_calculator_suggest());
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

OmniboxAction* AutocompleteMatch::GetActionAt(size_t index) const {
  return index >= actions.size() ? nullptr : actions[index].get();
}

AutocompleteMatch AutocompleteMatch::CreateActionMatch(
    size_t action_index) const {
  CHECK_LT(action_index, actions.size());
  CHECK_EQ(actions[action_index]->ActionId(), OmniboxActionId::PEDAL);

  AutocompleteMatch action_match(provider, relevance, false,
                                 AutocompleteMatchType::PEDAL);
  action_match.takeover_action = actions[action_index];
  action_match.transition = ui::PAGE_TRANSITION_GENERATED;
  action_match.suggest_type = omnibox::SuggestType::TYPE_NATIVE_CHROME;
  action_match.suggestion_group_id = suggestion_group_id;

  // Use the pedal text as primary match `contents`.
  action_match.contents = action_match.takeover_action->GetLabelStrings().hint;
  action_match.fill_into_edit = action_match.contents;
  if (action_match.contents.empty()) {
    action_match.contents_class.clear();
  } else {
    action_match.contents_class = {{0, ACMatchClassification::NONE}};
  }

  return action_match;
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
#if DCHECK_IS_ON()
  std::string debug_string = " text: " + base::UTF16ToUTF8(text) +
                             ", provider: " + provider_name +
                             ", classifications (offset, style): ";
  for (const auto& classification : classifications) {
    debug_string += " (" + base::NumberToString(classification.offset) + ", " +
                    base::NumberToString(classification.style) + ")";
  }

  if (text.empty()) {
    DCHECK(classifications.empty()) << debug_string;
    return;
  }

  // The classifications should always cover the whole string.
  DCHECK(!classifications.empty()) << " No classification;" << debug_string;
  DCHECK_EQ(0U, classifications[0].offset)
      << " Classification misses beginning;" << debug_string;
  if (classifications.size() == 1)
    return;

  // The classifications should always be sorted.
  size_t last_offset = classifications[0].offset;
  for (auto i(classifications.begin() + 1); i != classifications.end(); ++i) {
    DCHECK_GT(i->offset, last_offset)
        << " Unsorted classification;" << debug_string;
    DCHECK_LT(i->offset, text.length())
        << " Out of bounds classification;" << debug_string;
    last_offset = i->offset;
  }
#endif  // DCHECK_IS_ON()
}
