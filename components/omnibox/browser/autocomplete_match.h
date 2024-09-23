// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_

#include <stddef.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/search_engines/template_url.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#endif

class AutocompleteProvider;
class OmniboxAction;
class SuggestionAnswer;
class TemplateURL;
class TemplateURLService;

namespace base {
class Time;
}  // namespace base

namespace gfx {
struct VectorIcon;
}  // namespace gfx

const char kACMatchPropertySuggestionText[] = "match suggestion text";
const char kACMatchPropertyContentsPrefix[] = "match contents prefix";
const char kACMatchPropertyContentsStartIndex[] = "match contents start index";
// A match attribute when a default match's score has been boosted with a higher
// scoring non-default match.
const char kACMatchPropertyScoreBoostedFrom[] = "score_boosted_from";

// Util structs/enums ----------------------------------------------------------

// `RichAutocompletionParams` is a cache for the params used by
// `TryRichAutocompletion()`. `TryRichAutocompletion()` is called about 80 times
// per keystroke; fetching all 16 params each time causes measurable timing
// regressions. Using `static const` local variables instead wouldn't be
// testable.
struct RichAutocompletionParams {
  RichAutocompletionParams();
  static RichAutocompletionParams& GetParams();
  static void ClearParamsForTesting();
  bool enabled;
  bool autocomplete_titles;
  bool autocomplete_titles_shortcut_provider;
  int autocomplete_titles_min_char;
  bool autocomplete_non_prefix_all;
  bool autocomplete_non_prefix_shortcut_provider;
  int autocomplete_non_prefix_min_char;
  bool autocomplete_shortcut_text;
  int autocomplete_shortcut_text_min_char;
  bool counterfactual;
  bool autocomplete_prefer_urls_over_prefixes;
};

enum class IphType {
  kNone,
  // '@gemini' promo; shown in zero state.
  kGemini,
  // Featured search promo; shown in zero state.
  kFeaturedEnterpriseSearch,
  // Embeddings' setting promo when embeddings are disabled; shown in '@history'
  // scope.
  kHistoryEmbeddingsSettingsPromo,
  // Disclaimer when embeddings are enabled; shown in '@history' scope.
  kHistoryEmbeddingsDisclaimer,
  // '@history' promo when embeddings are disabled; shown in zero state.
  kHistoryScopePromo,
  // '@history' promo when embeddings are enabled; shown in zero state.
  kHistoryEmbeddingsScopePromo,
};

enum class FeedbackType {
  kNone,
  kThumbsUp,
  kThumbsDown,
};

// AutocompleteMatch ----------------------------------------------------------

// A single result line with classified spans.  The autocomplete popup displays
// the 'contents' and the 'description' (the description is optional) in the
// autocomplete dropdown, and fills in 'fill_into_edit' into the textbox when
// that line is selected.  fill_into_edit may be the same as 'description' for
// things like URLs, but may be different for searches or other providers.  For
// example, a search result may say "Search for asdf" as the description, but
// "asdf" should appear in the box.
struct AutocompleteMatch {
  using ScoringSignals = ::metrics::OmniboxScoringSignals;

  // Autocomplete matches contain strings that are classified according to a
  // separate vector of styles.  This vector associates flags with particular
  // string segments, and must be in sorted order.  All text must be associated
  // with some kind of classification.  Even if a match has no distinct
  // segments, its vector should contain an entry at offset 0 with no flags.
  //
  // Example: The user typed "goog"
  //   http://www.google.com/        Google
  //   ^          ^   ^              ^   ^
  //   0,         |   15,            |   4,
  //              11,match           0,match
  //
  // This structure holds the classification information for each span.
  struct ACMatchClassification {
    // The values in here are not mutually exclusive -- use them like a
    // bit field.  This also means we use "int" instead of this enum type when
    // passing the values around, so the compiler doesn't complain.
    //
    // A Java counterpart will be generated for this enum.
    // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.omnibox
    // GENERATED_JAVA_CLASS_NAME_OVERRIDE: MatchClassificationStyle
    // clang-format off
    enum Style {
      NONE      = 0,
      URL       = 1 << 0,  // A URL
      MATCH     = 1 << 1,  // A match for the user's search term
      DIM       = 1 << 2,  // "Helper text"
    };
    // clang-format on

    ACMatchClassification(size_t offset, int style)
        : offset(offset), style(style) {}

    bool operator==(const ACMatchClassification& other) const {
      return offset == other.offset && style == other.style;
    }

    bool operator!=(const ACMatchClassification& other) const {
      return offset != other.offset || style != other.style;
    }

    // Offset within the string that this classification starts
    size_t offset;

    // Contains a bitmask of flags defined in enum Style.
    int style;
  };

  // SuggestTiles are used specifically with TILE_NAVSUGGEST matches.
  // This structure should describe only the specific details for individual
  // tiles; all other properties are considered as shared and should be
  // extracted from the encompassing AutocompleteMatch object.
  struct SuggestTile {
    GURL url{};
    std::u16string title{};
    bool is_search{};
  };

  typedef std::vector<ACMatchClassification> ACMatchClassifications;

  // Type used by providers to attach additional, optional information to
  // an AutocompleteMatch.
  typedef std::map<std::string, std::string> AdditionalInfo;

  // The type of this match.
  typedef AutocompleteMatchType::Type Type;

  // Null-terminated array of characters that are not valid within |contents|
  // and |description| strings.
  static const char16_t kInvalidChars[];

  // Document subtype, for AutocompleteMatchType::DOCUMENT.
  // Update kDocumentTypeStrings when updating DocumentType.
  enum class DocumentType {
    NONE = 0,
    DRIVE_DOCS,
    DRIVE_FORMS,
    DRIVE_SHEETS,
    DRIVE_SLIDES,
    DRIVE_IMAGE,
    DRIVE_PDF,
    DRIVE_VIDEO,
    DRIVE_FOLDER,
    DRIVE_OTHER,
    DOCUMENT_TYPE_SIZE
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RichAutocompletionType {
    kNone = 0,
    kUrlNonPrefix = 1,
    kTitlePrefix = 2,
    kTitleNonPrefix = 3,
    kShortcutTextPrefix = 4,
    kMaxValue = kShortcutTextPrefix,
  };

  static constexpr auto kDocumentTypeStrings = std::to_array<const char*>(
      {"none", "drive_docs", "drive_forms", "drive_sheets", "drive_slides",
       "drive_image", "drive_pdf", "drive_video", "drive_folder",
       "drive_other"});

  static_assert(kDocumentTypeStrings.size() ==
                    static_cast<int>(DocumentType::DOCUMENT_TYPE_SIZE),
                "Sizes of AutocompleteMatch::kDocumentTypeStrings and "
                "AutocompleteMatch::DocumentType don't match.");

  // Return a string version of the core type values. Only used for
  // `RecordAdditionalInfo()`.
  static const char* DocumentTypeString(DocumentType type);

  // Use this function to convert integers to DocumentType enum values.
  // If you're sure it will be valid, you can call CHECK on the return value.
  // Returns true if |value| was successfully converted to a valid enum value.
  // The valid enum value will be written into |result|.
  static bool DocumentTypeFromInteger(int value, DocumentType* result);

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable,
                    Type type);
  AutocompleteMatch(const AutocompleteMatch& match);
  AutocompleteMatch(AutocompleteMatch&& match) noexcept;
  ~AutocompleteMatch();

  AutocompleteMatch& operator=(const AutocompleteMatch& match);
  AutocompleteMatch& operator=(AutocompleteMatch&& match) noexcept;

#if BUILDFLAG(IS_ANDROID)
  // Returns a corresponding Java object, creating it if necessary.
  // NOTE: Android specific methods are defined in autocomplete_match_android.cc
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const;

  // Update the bond with- or drop the Java AutocompleteMatch instance.
  // This should be called whenever the native AutocompleteMatch object is
  // updated for an existing Java object.
  void UpdateJavaObjectNativeRef();

  // Notify the Java object that its native counterpart is about to be
  // destroyed.
  void DestroyJavaObject();

  // Returns a corresponding Java Class object.
  static jclass GetClazz(JNIEnv* env);

  // Update the clipboard match with the current clipboard data.
  void UpdateWithClipboardContent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_callback);

  // Called when the match is updated with the clipboard content.
  void OnClipboardSuggestionContentUpdated(
      const base::android::JavaRef<jobject>& j_callback);

  // Update the Java object with clipboard content.
  void UpdateClipboardContent(JNIEnv* env);
  // Update the Java object with new destination URL.
  void UpdateJavaDestinationUrl();
  // Update the Java object with new Answer-in-Suggest.
  void UpdateJavaAnswer();
  // Update the Java object description.
  void UpdateJavaDescription();
  // Update the pointer to corresponding Java tab object.
  void UpdateMatchingJavaTab(const JavaObjectWeakGlobalRef& tab);
  // Get the matching Java Tab object.
  JavaObjectWeakGlobalRef GetMatchingJavaTab() const;
#endif

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
  // Converts SuggestionAnswer::AnswerType to an answer vector icon.
  static const gfx::VectorIcon& AnswerTypeToAnswerIcon(int type);

  // Gets the vector icon identifier for the icon to be shown for this match. If
  // `is_bookmark` is true, returns a bookmark icon rather than what the type
  // would normally determine.  Note that in addition to `type`, the icon chosen
  // may depend on match contents (e.g. Drive `document_type` or `actions`).
  // The reason `is_bookmark` is passed as a parameter and is not baked into the
  // AutocompleteMatch is likely that 1) this info is not used elsewhere in the
  // Autocomplete machinery except before displaying the match and 2) obtaining
  // this info is trivially done by calling BookmarkModel::IsBookmarked().
  // `turl` is used to identify the proper vector icon associated with a given
  // starter pack suggestion (e.g. @tabs, @history, @bookmarks, etc.).
  const gfx::VectorIcon& GetVectorIcon(bool is_bookmark,
                                       const TemplateURL* turl = nullptr) const;
#endif

  // Comparison function for determining whether the first match is better than
  // the second.
  static bool MoreRelevant(const AutocompleteMatch& match1,
                           const AutocompleteMatch& match2);

  // Comparison functions for determining whether the first match is preferred
  // over the second when choosing between candidate duplicates.
  static bool BetterDuplicate(const AutocompleteMatch& match1,
                              const AutocompleteMatch& match2);
  static bool BetterDuplicateByIterator(
      const std::vector<AutocompleteMatch>::const_iterator it1,
      const std::vector<AutocompleteMatch>::const_iterator it2);

  // Returns a new vector of classifications containing the merged contents of
  // |classifications1| and |classifications2|.
  static ACMatchClassifications MergeClassifications(
      const ACMatchClassifications& classifications1,
      const ACMatchClassifications& classifications2);

  // Converts classifications to and from a serialized string representation
  // (using comma-separated integers to sequentially list positions and styles).
  static std::string ClassificationsToString(
      const ACMatchClassifications& classifications);
  static ACMatchClassifications ClassificationsFromString(
      const std::string& serialized_classifications);

  // Adds a classification to the end of |classifications| iff its style is
  // different from the last existing classification.  |offset| must be larger
  // than the offset of the last classification in |classifications|.
  static void AddLastClassificationIfNecessary(
      ACMatchClassifications* classifications,
      size_t offset,
      int style);

  // Removes invalid characters from |text|. Should be called on strings coming
  // from external sources (such as extensions) before assigning to |contents|
  // or |description|.
  static std::u16string SanitizeString(const std::u16string& text);

  // Convenience function to check if `type` is featured Enterprise search.
  static bool IsFeaturedEnterpriseSearchType(Type type);

  // Convenience function to check if `type` is featured search type, e.g.
  // starter pack and featured site search engines created by policy.
  static bool IsFeaturedSearchType(Type type);

  // Convenience function to check if |type| is a search (as opposed to a URL or
  // an extension).
  static bool IsSearchType(Type type);

  // Convenience function to check if |type| is a special search suggest type -
  // like entity, personalized, profile or postfix.
  static bool IsSpecializedSearchType(Type type);

  // Convenience function to check if |type| is a search history type -
  // usually this surfaces a clock icon to the user.
  static bool IsSearchHistoryType(Type type);

  // Returns whether this match is a starter pack suggestion provided by the
  // built-in provider. This is the suggestion that the starter pack keyword
  // mode chips attach to.
  static bool IsStarterPackType(Type type);

  // Returns whether this match is a Clipboard suggestion.
  static bool IsClipboardType(Type type);

  // Convenience function to check if |type| is one of the suggest types we
  // need to skip for search vs url partitions - url, text or image in the
  // clipboard or query tile.
  static bool ShouldBeSkippedForGroupBySearchVsUrl(Type type);

  // Return a group ID based on type. Should only be used as a fill in for
  // matches that don't already have a group ID set by providers.
  static omnibox::GroupId GetDefaultGroupId(Type type);

  // A static version GetTemplateURL() that takes the match's keyword and
  // match's hostname as parameters.  In short, returns the TemplateURL
  // associated with |keyword| if it exists; otherwise returns the TemplateURL
  // associated with |host| if it exists.
  static TemplateURL* GetTemplateURLWithKeyword(
      TemplateURLService* template_url_service,
      const std::u16string& keyword,
      const std::string& host);
  static const TemplateURL* GetTemplateURLWithKeyword(
      const TemplateURLService* template_url_service,
      const std::u16string& keyword,
      const std::string& host);

  // Returns `url` altered by stripping off "www.", converting https protocol
  // to http, and stripping query params other than the search terms. If
  // `keep_search_intent_params` is true, also keep search intent params which
  // disambiguate the search terms and determine the fulfillment.
  // These conversions are meant to allow URL comparisons to find likely
  // duplicates; and these URLs are not used as actual destination URLs.
  // In most use cases `keep_search_intent_params` need not be true, e.g., when
  // computing `stripped_destination_url` for matches. Otherwise, keeping the
  // search intent params would create an unnecessary level of granularity which
  // prevents proper deduping of matches from various local or remote providers.
  // If a provider wishes to prevent its matches (or a subset of them) from
  // being deduped with other matches with the same search terms, it must
  // precompute `stripped_destination_url` while maintaining the search intent
  // params. A notable example is when multiple entity suggestions with the same
  // search terms are offered by SearchProvider or ZeroSuggestProvider. In that
  // case, the entity matches (with the exception of the first one) keep their
  // search intent params in `stripped_destination_url` to avoid being deduped.
  // - `input` is used to decide if the scheme is allowed to be altered during
  //   stripping.  If this URL, minus the scheme and separator, starts with any
  //   the terms in input.terms_prefixed_by_http_or_https(), we avoid converting
  //   an HTTPS scheme to HTTP.  This means URLs that differ only by these
  //   schemes won't be marked as dupes, since the distinction seems to matter
  //   to the user.
  // - If `template_url_service` is not NULL, it is used to get a template URL
  //   corresponding to this match, which is used to strip off query params
  //   other than the search terms (and optionally search intent params) that
  //   would otherwise prevent proper comparison/deduping.
  // - If the match's keyword is known, it can be provided in `keyword`.
  //   Otherwise, it can be left empty and the template URL (if any) is
  //   determined from the destination's hostname.
  // - If `normalize_search_terms` is true, the search terms in the final URL
  //   will be converted to lowercase with extra whitespace characters
  //   collapsed.
  static GURL GURLToStrippedGURL(const GURL& url,
                                 const AutocompleteInput& input,
                                 const TemplateURLService* template_url_service,
                                 const std::u16string& keyword,
                                 const bool keep_search_intent_params,
                                 const bool normalize_search_terms);

  // Sets the |match_in_scheme| and |match_in_subdomain| flags based on the
  // provided |url| and list of substring |match_positions|. |match_positions|
  // is the [begin, end) positions of a match within the unstripped URL spec.
  using MatchPosition = std::pair<size_t, size_t>;
  static void GetMatchComponents(
      const GURL& url,
      const std::vector<MatchPosition>& match_positions,
      bool* match_in_scheme,
      bool* match_in_subdomain);

  // Gets the formatting flags used for display of suggestions. This method
  // encapsulates the return of experimental flags too, so any URLs displayed
  // as an Omnibox suggestion should use this method.
  //
  // This function returns flags that may destructively format the URL, and
  // therefore should never be used for the |fill_into_edit| field.
  //
  // |preserve_scheme| and |preserve_subdomain| indicate that these URL
  // components are important (part of the match), and should not be trimmed.
  static url_formatter::FormatUrlTypes GetFormatTypes(bool preserve_scheme,
                                                      bool preserve_subdomain);
  // Logs the search engine used to navigate to a search page or auto complete
  // suggestion. For direct URL navigations, nothing is logged.
  static void LogSearchEngineUsed(const AutocompleteMatch& match,
                                  TemplateURLService* template_url_service);

  // Computes the stripped destination URL (via GURLToStrippedGURL()) and
  // stores the result in |stripped_destination_url|.  |input| is used for the
  // same purpose as in GURLToStrippedGURL().
  void ComputeStrippedDestinationURL(const AutocompleteInput& input,
                                     TemplateURLService* template_url_service);

  // Returns whether `destination_url` looks like a doc URL. If so, will also
  // set `stripped_destination_url` to avoid repeating the computation later.
  bool IsDocumentSuggestion();

  // Checks if this match is a trend suggestion based on the match subtypes.
  bool IsTrendSuggestion() const;

  // Checks if this match is an informational IPH suggestion based on the match
  // and provider type.
  bool IsIPHSuggestion() const;

  // Returns true if this match may attach one or more `actions`.
  // This method is used to keep actions off of matches with types that don't
  // mix well with Pedals or other actions (e.g. entities).
  bool IsActionCompatible() const;

  // Returns true if this match has a keyword that puts the omnibox instantly
  // into keyword mode when the match is focused via keyboard, instead of
  // the usual waiting for activation of a visible keyword button.
  bool HasInstantKeyword(TemplateURLService* template_url_service) const;

  // Gets data relevant to whether there should be any special keyword-related
  // UI shown for this match. If this match represents a selected keyword, i.e.
  // the UI should be "in keyword mode", `keyword_out` will be set to the
  // keyword and `is_keyword_hint` will be set to false. If this match has a
  // non-null `associated_keyword`, i.e. we should show a "Press [tab] to search
  // ___" hint and allow the user to toggle into keyword mode, `keyword_out`
  // will be set to the associated keyword and `is_keyword_hint` will be set to
  // true. Note that only one of these states can be in effect at once. In all
  // other cases, `keyword_out` will be cleared, even when our member variable
  // `keyword` is non-empty -- such as with non-substituting keywords or matches
  // that represent searches using the default search engine. See also
  // `GetSubstitutingExplicitlyInvokedKeyword()`. `keyword_placeholder_out` will
  // be set to any placeholder text the keyword wants to display. Set for both
  // hint and non-hint keyword modes. `is_history_embeddings_enabled` will
  // affect the placeholder text for the @history keyword.
  void GetKeywordUIState(TemplateURLService* template_url_service,
                         bool is_history_embeddings_enabled,
                         std::u16string* keyword_out,
                         std::u16string* keyword_placeholder_out,
                         bool* is_keyword_hint) const;

  // Returns |keyword|, but only if it represents a substituting keyword that
  // the user has explicitly invoked.  If for example this match represents a
  // search with the default search engine (and the user didn't explicitly
  // invoke its keyword), this returns the empty string.  The result is that
  // this function returns a non-empty string in the same cases as when the UI
  // should show up as being "in keyword mode".
  std::u16string GetSubstitutingExplicitlyInvokedKeyword(
      TemplateURLService* template_url_service) const;

  // Returns the placeholder text to display for the currently selected keyword
  // match, returned for both hint and non-hint keyword modes.
  std::u16string GetKeywordPlaceholder(
      TemplateURLService* template_url_service,
      bool is_history_embeddings_enabled) const;

  // Returns the TemplateURL associated with this match.  This may be NULL if
  // the match has no keyword OR if the keyword no longer corresponds to a valid
  // TemplateURL.  See comments on |keyword| below.
  // If |allow_fallback_to_destination_host| is true and the keyword does
  // not map to a valid TemplateURL, we'll then check for a TemplateURL that
  // corresponds to the destination_url's hostname.
  TemplateURL* GetTemplateURL(TemplateURLService* template_url_service,
                              bool allow_fallback_to_destination_host) const;

  // Gets the URL for the match image (whether it be an answer or entity). If
  // there isn't an image URL, returns an empty GURL (test with is_empty()).
  GURL ImageUrl() const;

  // Adds optional information to the |additional_info| dictionary.
  void RecordAdditionalInfo(const std::string& property,
                            const std::string& value);
  void RecordAdditionalInfo(const std::string& property,
                            const std::u16string& value);
  void RecordAdditionalInfo(const std::string& property, int value);
  void RecordAdditionalInfo(const std::string& property, double value);
  void RecordAdditionalInfo(const std::string& property, base::Time value);

  // Returns the value recorded for |property| in the |additional_info|
  // dictionary. Returns the empty string if no such value exists. This is for
  // debugging in chrome://omnibox only. It should only be called by
  // `OmniboxPageHandler` and tests. For match info that's used for
  // non-debugging, use class fields. Unfortunately, There are existing
  // non-debug callsites; those should be cleaned up, not added to.
  std::string GetAdditionalInfoForDebugging(const std::string& property) const;

  // Returns the provider type selected from this match, which is by default
  // taken from the match `provider` type but may be a (pseudo-)provider
  // associated with one of the match's action types if one of the match's
  // actions are chosen with `action_index`.
  metrics::OmniboxEventProto::ProviderType GetOmniboxEventProviderType(
      int action_index = -1) const;

  // Returns the result type selected from this match, which is by default
  // equivalent to the match type but may be one of the match's action
  // types if one of the match's actions are chosen with `action_index`.
  metrics::OmniboxEventProto::Suggestion::ResultType GetOmniboxEventResultType(
      int action_index = -1) const;

  // Returns whether this match is a "verbatim" match: a URL navigation directly
  // to the user's input, a search for the user's input with the default search
  // engine, or a "keyword mode" search for the query portion of the user's
  // input.  Note that rare or unusual types that could be considered verbatim,
  // such as keyword engine matches or extension-provided matches, aren't
  // detected by this IsVerbatimType, as the user will not be able to infer
  // what will happen when they press enter in those cases if the match is not
  // shown.
  bool IsVerbatimType() const;

  // Returns whether this match is a "verbatim URL" suggestion.
  bool IsVerbatimUrlSuggestion() const;

  // Returns whether this match is a search suggestion provided by search
  // provider.
  bool IsSearchProviderSearchSuggestion() const;

  // Returns whether this match is a search suggestion provided by on device
  // providers.
  bool IsOnDeviceSearchSuggestion() const;

  // Returns the top-level sorting order of the suggestion.
  // Suggestions should be sorted by this value first, and by Relevance score
  // next.
  int GetSortingOrder() const;

  // Returns true if the match is eligible for ML scoring signal logging.
  bool IsMlSignalLoggingEligible() const;

  // Returns true if the match is eligible to be re-scored by ML scoring.
  bool IsMlScoringEligible() const;

  // Filter OmniboxActions based on the supplied qualifiers.
  // The order of the supplied qualifiers determines the preference.
  void FilterOmniboxActions(
      const std::vector<OmniboxActionId>& allowed_action_ids);

  // Rearranges and truncates ActionsInSuggest objects to match the desired
  // order and presence of actions.
  // Unlike FilterOmniboxActions(), this method specifically targets
  // ActionsInSuggest.
  void FilterAndSortActionsInSuggest();

  // Remove all Answer Actions.
  void RemoveAnswerActions();

  // Returns whether the autocompletion is trivial enough that we consider it
  // an autocompletion for which the omnibox autocompletion code did not add
  // any value.
  bool IsTrivialAutocompletion() const;

  // Returns whether this match or any duplicate of this match can be deleted.
  // This is used to decide whether we should call DeleteMatch().
  bool SupportsDeletion() const;

  // Returns a copy of this match with the contents and description fields, and
  // their associated classifications, possibly swapped.  We swap these if this
  // is a match for which we should emphasize the title (stored in the
  // description field) over the URL (in the contents field).
  //
  // We specifically return a copy to prevent the UI code from accidentally
  // mucking with the matches stored in the model, lest other omnibox systems
  // get confused about which is which.  See the code that sets
  // |swap_contents_and_description| for conditions they are swapped.
  //
  // TODO(crbug.com/40179316): Clean up the handling of contents and description
  // so that this copy is no longer required.
  AutocompleteMatch GetMatchWithContentsAndDescriptionPossiblySwapped() const;

  // Determines whether this match is allowed to be the default match by
  // comparing |input.text| and |inline_autocompletion|. Therefore,
  // |inline_autocompletion| should be set prior to invoking this method. Also
  // considers trailing whitespace in the input, so the input should not be
  // fixed up. May trim trailing whitespaces from |inline_autocompletion|.
  //
  // Input "x" will allow default matches "x", "xy", and "x y".
  // Input "x " will allow default matches "x" and "x y".
  // Input "x  " will allow default match "x".
  // Input "x y" will allow default match "x y".
  // Input "x" with prevent_inline_autocomplete will allow default match "x".
  void SetAllowedToBeDefault(const AutocompleteInput& input);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Upgrades this match by absorbing the best properties from
  // |duplicate_match|. For instance: if |duplicate_match| has a higher
  // relevance score, this match's own relevance score will be upgraded.
  void UpgradeMatchWithPropertiesFrom(AutocompleteMatch& duplicate_match);

  // Merges scoring signals from the other match for ML model scoring and
  // training .
  void MergeScoringSignals(const AutocompleteMatch& other);

  // Tries, in order, to:
  // - Prefix autocomplete |primary_text|
  // - Prefix autocomplete |secondary_text|
  // - Non-prefix autocomplete |primary_text|
  // - Non-prefix autocomplete |secondary_text|
  // - Split autocomplete |primary_text|
  // - Split autocomplete |secondary_text|
  // Returns false if none of the autocompletions were appropriate (or the
  // features were disabled).
  bool TryRichAutocompletion(const std::u16string& primary_text,
                             const std::u16string& secondary_text,
                             const AutocompleteInput& input,
                             const std::u16string& shortcut_text = u"");

  // True if `inline_autocompletion` and `prefix_autocompletion` are both empty.
  bool IsEmptyAutocompletion() const;

  // Serialise this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Returns the action at `index`, or nullptr if `index` is out of bounds.
  OmniboxAction* GetActionAt(size_t index) const;

  // Returns if `predicate` returns true for the match or one of its duplicates.
  template <typename UnaryPredicate>
  bool MatchOrDuplicateMeets(UnaryPredicate predicate) const {
    return predicate(*this) ||
           base::ranges::any_of(duplicate_matches, std::move(predicate));
  }

  // Finds first action where `predicate` returns true. This is a special use
  // utility method for situations where actions with certain constraints
  // need to be selected. If no such action is found, returns nullptr.
  template <typename UnaryPredicate>
  OmniboxAction* GetActionWhere(UnaryPredicate predicate) const {
    auto it = base::ranges::find_if(actions, std::move(predicate));
    return it != actions.end() ? it->get() : nullptr;
  }

  // Create a new match from scratch based on this match and its action at
  // given `action_index`. The content and takeover match on the returned
  // match will be set up to execute the action, and only a minimum of
  // data is shared from this source match.
  AutocompleteMatch CreateActionMatch(size_t action_index) const;

  // The provider of this match, used to remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  raw_ptr<AutocompleteProvider> provider = nullptr;

  // The relevance of this match. See table in autocomplete_provider.h for
  // scores returned by various providers. This is used to rank matches among
  // all responding providers, so different providers must be carefully tuned to
  // supply matches with appropriate relevance.
  int relevance = 0;

  // The "navigational intent" of this match. In other words, the likelihood
  // that the user intends to navigate to a specific place by making use of
  // this match.
  omnibox::NavigationalIntent navigational_intent{omnibox::NAV_INTENT_NONE};

  // How many times this result was typed in / selected from the omnibox.
  // Only set for some providers and result_types.  If it is not set,
  // its value is -1.  At the time of writing this comment, it is only
  // set for matches from HistoryURL and HistoryQuickProvider.
  int typed_count = -1;

  // True if the user should be able to delete this match.
  bool deletable = false;

  // This string is loaded into the location bar when the item is selected
  // by pressing the arrow keys. This may be different than a URL, for example,
  // for search suggestions, this would just be the search terms.
  std::u16string fill_into_edit;

  // This string is displayed adjacent to the omnibox if this match is the
  // default. Will usually be URL when autocompleting a title, and empty
  // otherwise.
  std::u16string additional_text;

  // The inline autocompletion to display after the user's input in the
  // omnibox, if this match becomes the default match.  It may be empty.
  std::u16string inline_autocompletion;
  // Whether rich autocompletion triggered; i.e. this suggestion *is or could
  // have been* rich autocompleted. This is usually redundant and checking
  // whether `prefix_autocompletion` is non-empty should be used instead to
  // determine if this suggestion *is* rich autocompleted. But for
  // counterfactual variations, `prefix_autocompletion` isn't copied when
  // deduping matches to avoid showing rich autocompletion and so can't be used
  // to trigger logging.
  // TODO(manukh): remove `rich_autocompletion_triggered` when counterfactual
  //  experiments end.
  RichAutocompletionType rich_autocompletion_triggered =
      RichAutocompletionType::kNone;
  // The inline autocompletion to display before the user's input in the
  // omnibox, if this match becomes the default match. Always empty if
  // non-prefix autocompletion is disabled.
  std::u16string prefix_autocompletion;

  // If false, the omnibox should prevent this match from being the
  // default match.  Providers should set this to true only if the
  // user's input, plus any inline autocompletion on this match, would
  // lead the user to expect a navigation to this match's destination.
  // For example, with input "foo", a search for "bar" or navigation
  // to "bar.com" should not set this flag; a navigation to "foo.com"
  // should only set this flag if ".com" will be inline autocompleted;
  // and a navigation to "foo/" (an intranet host) or search for "foo"
  // should set this flag.
  bool allowed_to_be_default_match = false;

  // The URL to actually load when the autocomplete item is selected. This URL
  // should be canonical so we can compare URLs with strcmp to avoid dupes.
  // It may be empty if there is no possible navigation.
  GURL destination_url;

  // The destination URL modified for better dupe finding.  The result may not
  // be navigable or even valid; it's only meant to be used for detecting
  // duplicates. Providers are not expected to set this field,
  // `AutocompleteResult` will set it using `ComputeStrippedDestinationURL()`.
  // Providers may manually set it to avoid the default
  // `ComputeStrippedDestinationURL()` computation.
  GURL stripped_destination_url;

  // Extra headers to add to the navigation. See `NavigateParams::extra_headers`
  // for how headers should be represented.
  std::string extra_headers;

  // Optional image information. Used for entity suggestions. The dominant color
  // can be used to paint the image placeholder while fetching the image.
  std::string image_dominant_color;
  GURL image_url;

  // Optional entity id for entity suggestions. Empty string means no entity ID.
  // This is not meant for display, but internal use only. The actual UI display
  // is controlled by the `type` and `image_url`.
  std::string entity_id;

  // Optional website URI for entity suggestions. Empty string means no website
  // URI.
  std::string website_uri;

  // Optional override to use for types that specify an icon sub-type.
  DocumentType document_type = DocumentType::NONE;

  // Holds the common part of tail suggestion. Used to indent the contents.
  // Can't simply store the character length of the string, as different
  // characters may have different rendered widths.
  std::u16string tail_suggest_common_prefix;

  // The main text displayed in the address bar dropdown.
  std::u16string contents;
  ACMatchClassifications contents_class;

  // Additional helper text for each entry, such as a title or description.
  std::u16string description;
  ACMatchClassifications description_class;
  // In the case of the document provider, `description` includes a last
  // updated date that may become stale. Likewise for the bookmark provider,
  // `contents` may be the path which may become stale when the bookmark is
  // moved. To avoid showing stale text, when `description_for_shortcut`
  // is not empty, it will be stored instead of `description` (or `contents` if
  // `swap_contents_and_description` is true) in the shortcuts provider.
  // TODO(manukh) This is a temporary misnomer (since it can represent both
  //   `description` and `contents`) until `swap_contents_and_description` is
  //   removed.
  std::u16string description_for_shortcuts;
  ACMatchClassifications description_class_for_shortcuts;

  // The optional suggestion group ID used to look up the suggestion group info
  // for the group this suggestion belongs to from the AutocompleteResult.
  //
  // Use omnibox::GROUP_INVALID in place of a missing value when converting
  // this to a primitive type.
  // TODO(manukh): Seems redundant to prefix a suggestion field with
  //  'suggestion_'. Check if it makes sense to rename to 'group_id', and
  //  likewise for the associated methods and local variables.
  std::optional<omnibox::GroupId> suggestion_group_id;

  // If true, UI-level code should swap the contents and description fields
  // before displaying.
  bool swap_contents_and_description = false;

  // A rich-format version of the display for the dropdown.
  std::optional<SuggestionAnswer> answer;

  std::optional<omnibox::RichAnswerTemplate> answer_template;

  // AnswerType for answer verticals, including rich answers.
  omnibox::AnswerType answer_type{omnibox::ANSWER_TYPE_UNSPECIFIED};

  // The transition type to use when the user opens this match.  By default,
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;

  // Type of this match.
  Type type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  // The type of this suggestion as reported from and back to the suggest server
  // via the server response and the ChromeSearchboxStats (reported in the match
  // destination URL) respectively.
  // The default value indicates a native Chrome suggestion which must include a
  // SUBTYPE_OMNIBOX_* in `subtypes`.
  //
  // The value is always present in omnibox::SuggestType enum. Although the list
  // of types in omnibox::SuggestType enum may not be exhaustive, the known type
  // names found in the server response are mapped to the equivalent enum values
  // and the unknown types fall back to omnibox::TYPE_QUERY.
  omnibox::SuggestType suggest_type{omnibox::TYPE_NATIVE_CHROME};

  // Used to identify the specific source / type for suggestions by the
  // suggest server. See SuggestSubtype in types.proto for more details.
  // Uses flat_set to deduplicate subtypes (e.g., as a result of Chrome adding
  // additional subtypes). The order of elements reported back via
  // ChromeSearchboxStats is irrelevant. flat_set uses std::vector as a
  // container, reducing memory overhead of keeping a handful of integers, while
  // offering similar functionality as std::set.
  //
  // This set may contain int values not present in omnibox::SuggestSubtype
  // enum. This is because the list of subtypes in omnibox::SuggestSubtype enum
  // is not exhaustive. However, casting int values into omnibox::SuggestSubtype
  // enum without testing membership is expected to be safe as
  // omnibox::SuggestSubtype enum has a fixed int underlying type.
  base::flat_set<omnibox::SuggestSubtype> subtypes;

  // True if we saw a tab that matched this suggestion.
  // Unset if it has not been computed yet.
  std::optional<bool> has_tab_match;

  // Set with a keyword provider match if this match can show a keyword hint.
  // For example, if this is a SearchProvider match for "www.amazon.com",
  // |associated_keyword| could be a KeywordProvider match for "amazon.com".
  //
  // When this is set, the popup will show a ">" symbol at the right edge of the
  // line for this match, and tab/shift-tab will toggle in and out of keyword
  // mode without disturbing the rest of the popup.  See also
  // OmniboxPopupModel::SetSelectedLineState().
  std::unique_ptr<AutocompleteMatch> associated_keyword;

  // The keyword of the TemplateURL the match originated from.  This is nonempty
  // for both explicit "keyword mode" matches as well as matches for the default
  // search provider (so, any match for which we're doing substitution); it
  // doesn't imply (alone) that the UI is going to show a keyword hint or
  // keyword mode.  For that, see GetKeywordUIState() or
  // GetSubstitutingExplicitlyInvokedKeyword().
  //
  // CAUTION: The TemplateURL associated with this keyword may be deleted or
  // modified while the AutocompleteMatch is alive.  This means anyone who
  // accesses it must perform any necessary sanity checks before blindly using
  // it!
  std::u16string keyword;

  // Set in matches originating from keyword results.
  bool from_keyword = false;

  // The visible actions relevant to this match.
  std::vector<scoped_refptr<OmniboxAction>> actions;

  // An optional invisible action that takes over the match navigation. That is:
  // if provided, when the user selects the match, the navigation is ignored and
  // this action is executed instead.
  scoped_refptr<OmniboxAction> takeover_action;

  // True if this match is from a previous result.
  bool from_previous = false;

  // Optional search terms args.  If present,
  // AutocompleteController::UpdateSearchboxStats() will incorporate this data
  // with additional data it calculates and pass the completed struct to
  // TemplateURLRef::ReplaceSearchTerms() to reset the match's |destination_url|
  // after the complete set of matches in the AutocompleteResult has been chosen
  // and sorted.  Most providers will leave this as NULL, which will cause the
  // AutocompleteController to do no additional transformations.
  std::unique_ptr<TemplateURLRef::SearchTermsArgs> search_terms_args;

  // Optional post content. If this is set, the request to load the destination
  // url will pass this post content as well.
  std::unique_ptr<TemplateURLRef::PostContent> post_content;

  // Information dictionary into which each provider can optionally record a
  // property and associated value and which is presented in chrome://omnibox.
  AdditionalInfo additional_info;

  // A vector of matches culled during de-duplication process, sorted from
  // second-best to worst according to the de-duplication preference criteria.
  // This vector is retained so that if the user deletes a match, all the
  // duplicates are deleted as well. This is also used for re-duping Search
  // Entity vs. plain Search suggestions.
  std::vector<AutocompleteMatch> duplicate_matches;

  // A list of navsuggest tiles to be shown as part of this match.
  // This object is only populated for TILE_NAVSUGGEST AutocompleteMatches.
  std::vector<SuggestTile> suggest_tiles;

  // Signals for ML scoring.
  std::optional<ScoringSignals> scoring_signals;

  // A flag to mark whether this would've been excluded from the "original" list
  // of matches. Traditionally, providers limit the number of suggestions they
  // provide to the top N most relevant matches. When ML scoring is enabled,
  // however, providers pass ALL suggestion candidates to the controller. When
  // this flag is true, this match is an "extra" suggestion that would've
  // originally been culled by the provider.
  // TODO(yoangela|manukh): Currently unused except in tests. Remove if not
  //   needed. Might be needed when increasing the max provider limit?
  bool culled_by_provider = false;

  // True for shortcut suggestions that were boosted. Used for grouping logic.
  // TODO(manukh): Remove this field and use `suggestion_group_id` once grouping
  //   launches. In the meantime, shortcut grouping won't work for users in the
  //   grouping experiments.
  bool shortcut_boosted = false;

  // E.g. the gemini IPH match shown at the bottom of the popup.
  IphType iph_type = IphType::kNone;

  // IPH matches aren't clickable like other matches, but may have a next-action
  // or learn-more type of link. This link is always appended to the end of
  // their contents/description text.
  std::u16string iph_link_text;
  GURL iph_link_url;

  // The user feedback on the match.
  FeedbackType feedback_type = FeedbackType::kNone;

  // So users of AutocompleteMatch can use the same ellipsis that it uses.
  static const char16_t kEllipsis[];

#if DCHECK_IS_ON()
  // Does a data integrity check on this match.
  void Validate() const;
#endif  // DCHECK_IS_ON()

  // Checks one text/classifications pair for valid values.
  static void ValidateClassifications(
      const std::u16string& text,
      const ACMatchClassifications& classifications,
      const std::string& provider_name = "");

 private:
#if BUILDFLAG(IS_ANDROID)
  // Corresponding Java object.
  // This element should not be copied with the rest of the AutocompleteMatch
  // object to ensure consistent 1:1 relationship between the objects.
  // This object should never be accessed directly. To acquire a reference to
  // java object, call the GetOrCreateJavaObject().
  // Note that this object is lazily constructed to avoid creating Java matches
  // for throw away AutocompleteMatch objects, eg. during Classify() or
  // QualifyPartialUrlQuery() calls.
  // See AutocompleteControllerAndroid for more details.
  mutable std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>
      java_match_;

  // When set, holds a weak reference to Java Tab object.
  JavaObjectWeakGlobalRef matching_java_tab_{};

  base::WeakPtrFactory<AutocompleteMatch> weak_ptr_factory_{this};
#endif
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;
typedef std::vector<AutocompleteMatch> ACMatches;

// Can be used as the key for grouping AutocompleteMatches in a map based on a
// std::tuple of fields.
// The accompanying hash function makes the key usable in an std::unordered_map.
template <typename... Args>
using ACMatchKey = std::tuple<Args...>;

template <typename... Args>
struct ACMatchKeyHash {
  size_t operator()(const ACMatchKey<Args...>& key) const;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_
