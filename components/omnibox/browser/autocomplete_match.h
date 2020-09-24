// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_offset_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/query_tiles/tile.h"
#include "components/search_engines/template_url.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class AutocompleteProvider;
class OmniboxPedal;
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

// AutocompleteMatch ----------------------------------------------------------

// A single result line with classified spans.  The autocomplete popup displays
// the 'contents' and the 'description' (the description is optional) in the
// autocomplete dropdown, and fills in 'fill_into_edit' into the textbox when
// that line is selected.  fill_into_edit may be the same as 'description' for
// things like URLs, but may be different for searches or other providers.  For
// example, a search result may say "Search for asdf" as the description, but
// "asdf" should appear in the box.
struct AutocompleteMatch {
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
        : offset(offset),
          style(style) {
    }

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

  // NavsuggestTiles are used specifically with TILE_NAVSUGGEST matches.
  // This structure should describe only the specific details for individual
  // tiles; all other properties are considered as shared and should be
  // extracted from the encompassing AutocompleteMatch object.
  struct NavsuggestTile {
    GURL url;
    base::string16 title;
  };

  typedef std::vector<ACMatchClassification> ACMatchClassifications;

  // Type used by providers to attach additional, optional information to
  // an AutocompleteMatch.
  typedef std::map<std::string, std::string> AdditionalInfo;

  // The type of this match.
  typedef AutocompleteMatchType::Type Type;

  // Null-terminated array of characters that are not valid within |contents|
  // and |description| strings.
  static const base::char16 kInvalidChars[];

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

  static const char* const kDocumentTypeStrings[];

  // Return a string version of the core type values.
  static const char* DocumentTypeString(DocumentType type);

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable,
                    Type type);
  AutocompleteMatch(const AutocompleteMatch& match);
  AutocompleteMatch(AutocompleteMatch&& match) noexcept;
  ~AutocompleteMatch();

  AutocompleteMatch& operator=(const AutocompleteMatch& match);

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  // Gets the vector icon identifier for the icon to be shown for this match. If
  // |is_bookmark| is true, returns a bookmark icon rather than what the type
  // would normally determine.  Note that in addition to |type|, the icon chosen
  // may depend on match contents (e.g. Drive |document_type| or |pedal|).
  const gfx::VectorIcon& GetVectorIcon(bool is_bookmark) const;
#endif

  // Returns text explaining why this suggestion was displayed. Can return an
  // empty string if there is no explanation.
  base::string16 GetWhyThisSuggestionText() const;

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

  // Helper functions for classes creating matches:
  // Fills in the classifications for |text|, using |style| as the base style
  // and marking the first instance of |find_text| as a match.  (This match
  // will also not be dimmed, if |style| has DIM set.)
  static void ClassifyMatchInString(const base::string16& find_text,
                                    const base::string16& text,
                                    int style,
                                    ACMatchClassifications* classifications);

  // Similar to ClassifyMatchInString(), but for cases where the range to mark
  // as matching is already known (avoids calling find()).  This can be helpful
  // when find() would be misleading (e.g. you want to mark the second match in
  // a string instead of the first).
  static void ClassifyLocationInString(size_t match_location,
                                       size_t match_length,
                                       size_t overall_length,
                                       int style,
                                       ACMatchClassifications* classifications);

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

  // Returns true if at least one style in |classifications| is of type MATCH.
  static bool HasMatchStyle(const ACMatchClassifications& classifications);

  // Removes invalid characters from |text|. Should be called on strings coming
  // from external sources (such as extensions) before assigning to |contents|
  // or |description|.
  static base::string16 SanitizeString(const base::string16& text);

  // Convenience function to check if |type| is a search (as opposed to a URL or
  // an extension).
  static bool IsSearchType(Type type);

  // Convenience function to check if |type| is a special search suggest type -
  // like entity, personalized, profile or postfix.
  static bool IsSpecializedSearchType(Type type);

  // Convenience function to check if |type| is a search history type -
  // usually this surfaces a clock icon to the user.
  static bool IsSearchHistoryType(Type type);

  // Returns true if matches with given |type| may be attached with a |pedal|.
  static bool IsPedalCompatibleType(Type type);

  // Convenience function to check if |type| is one of the suggest types we
  // need to skip for search vs url partitions - url, text or image in the
  // clipboard or query tile.
  static bool ShouldBeSkippedForGroupBySearchVsUrl(Type type);

  // A static version GetTemplateURL() that takes the match's keyword and
  // match's hostname as parameters.  In short, returns the TemplateURL
  // associated with |keyword| if it exists; otherwise returns the TemplateURL
  // associated with |host| if it exists.
  static TemplateURL* GetTemplateURLWithKeyword(
      TemplateURLService* template_url_service,
      const base::string16& keyword,
      const std::string& host);
  static const TemplateURL* GetTemplateURLWithKeyword(
      const TemplateURLService* template_url_service,
      const base::string16& keyword,
      const std::string& host);

  // Returns |url| altered by stripping off "www.", converting https protocol
  // to http, and stripping excess query parameters.  These conversions are
  // merely to allow comparisons to remove likely duplicates; these URLs are
  // not used as actual destination URLs.
  // - |input| is used to decide if the scheme is allowed to be altered during
  //   stripping.  If this URL, minus the scheme and separator, starts with any
  //   the terms in input.terms_prefixed_by_http_or_https(), we avoid converting
  //   an HTTPS scheme to HTTP.  This means URLs that differ only by these
  //   schemes won't be marked as dupes, since the distinction seems to matter
  //   to the user.
  // - If |template_url_service| is not NULL, it is used to get a template URL
  //   corresponding to this match, which is used to strip off query args other
  //   than the search terms themselves that would otherwise prevent doing
  //   proper deduping.
  // - If the match's keyword is known, it can be provided in |keyword|.
  //   Otherwise, it can be left empty and the template URL (if any) is
  //   determined from the destination's hostname.
  static GURL GURLToStrippedGURL(const GURL& url,
                                 const AutocompleteInput& input,
                                 const TemplateURLService* template_url_service,
                                 const base::string16& keyword);

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

  // Gets data relevant to whether there should be any special keyword-related
  // UI shown for this match.  If this match represents a selected keyword, i.e.
  // the UI should be "in keyword mode", |keyword| will be set to the keyword
  // and |is_keyword_hint| will be set to false.  If this match has a non-NULL
  // |associated_keyword|, i.e. we should show a "Press [tab] to search ___"
  // hint and allow the user to toggle into keyword mode, |keyword| will be set
  // to the associated keyword and |is_keyword_hint| will be set to true.  Note
  // that only one of these states can be in effect at once.  In all other
  // cases, |keyword| will be cleared, even when our member variable |keyword|
  // is non-empty -- such as with non-substituting keywords or matches that
  // represent searches using the default search engine.  See also
  // GetSubstitutingExplicitlyInvokedKeyword().
  void GetKeywordUIState(TemplateURLService* template_url_service,
                         base::string16* keyword,
                         bool* is_keyword_hint) const;

  // Returns |keyword|, but only if it represents a substituting keyword that
  // the user has explicitly invoked.  If for example this match represents a
  // search with the default search engine (and the user didn't explicitly
  // invoke its keyword), this returns the empty string.  The result is that
  // this function returns a non-empty string in the same cases as when the UI
  // should show up as being "in keyword mode".
  base::string16 GetSubstitutingExplicitlyInvokedKeyword(
      TemplateURLService* template_url_service) const;

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
                            const base::string16& value);
  void RecordAdditionalInfo(const std::string& property, int value);
  void RecordAdditionalInfo(const std::string& property, base::Time value);

  // Returns the value recorded for |property| in the |additional_info|
  // dictionary.  Returns the empty string if no such value exists.
  std::string GetAdditionalInfo(const std::string& property) const;

  // Returns the enum equivalent to the type of this autocomplete match.
  metrics::OmniboxEventProto::Suggestion::ResultType AsOmniboxEventResultType()
      const;

  // Returns whether this match is a "verbatim" match: a URL navigation directly
  // to the user's input, a search for the user's input with the default search
  // engine, or a "keyword mode" search for the query portion of the user's
  // input.  Note that rare or unusual types that could be considered verbatim,
  // such as keyword engine matches or extension-provided matches, aren't
  // detected by this IsVerbatimType, as the user will not be able to infer
  // what will happen when they press enter in those cases if the match is not
  // shown.
  bool IsVerbatimType() const;

  // Returns whether this match is a search suggestion provided by search
  // provider.
  bool IsSearchProviderSearchSuggestion() const;

  // Returns whether this match is a search suggestion provided by on device
  // providers.
  bool IsOnDeviceSearchSuggestion() const;

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
  AutocompleteMatch GetMatchWithContentsAndDescriptionPossiblySwapped() const;

  // Determines whether this match is allowed to be the default match by
  // comparing |input.text| and |inline_autocompletion|. Therefore,
  // |inline_autocompletion| should be set prior to invoking this method. Also
  // Also considers trailing whitespace in the input, so the input should not be
  // fixed up. May trim trailing whitespaces from |inline_autocompletion|.
  //
  // Input "x" will allow default matches "x", "xy", and "x y".
  // Input "x " will allow default matches "x" and "x y".
  // Input "x  " will allow default match "x".
  // Input "x y" will allow default match "x y".
  // Input "x" with prevent_inline_autocomplete will allow default match "x".
  void SetAllowedToBeDefault(const AutocompleteInput& input);

  // If this match is a tail suggestion, prepends the passed |common_prefix|.
  // If not, but the prefix matches the beginning of the suggestion, dims that
  // portion in the classification.
  void InlineTailPrefix(const base::string16& common_prefix);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Upgrades this match by absorbing the best properties from
  // |duplicate_match|. For instance: if |duplicate_match| has a higher
  // relevance score, this match's own relevance score will be upgraded.
  void UpgradeMatchWithPropertiesFrom(AutocompleteMatch& duplicate_match);

  // Tries, in order, to:
  // - Prefix autocomplete |primary_text|,
  // - Prefix autocomplete |secondary_text|,
  // - Non-prefix autocomplete |primary_text|, and
  // - Non-prefix autocomplete |secondary_text|.
  // Midword and title autocompletion are only attempted if
  // |OmniboxFieldTrial::RichAutocompletionAutocompleteTitles()| and
  // |OmniboxFieldTrial::RichAutocompletionAutocompleteNonPrefix*()| are true
  // respectively.
  // Returns false if none of the autocompletions were appropriate (or the
  // features were disabled).
  bool TryRichAutocompletion(const base::string16& primary_text,
                             const base::string16& secondary_text,
                             const AutocompleteInput& input,
                             bool shortcut_provider = false);

  // True if both |inline_autocompletion| & |prefix_autocompletion| are empty.
  bool IsEmptyAutocompletion() const;

  // The provider of this match, used to remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  AutocompleteProvider* provider = nullptr;

  // The relevance of this match. See table in autocomplete.h for scores
  // returned by various providers. This is used to rank matches among all
  // responding providers, so different providers must be carefully tuned to
  // supply matches with appropriate relevance.
  int relevance = 0;

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
  base::string16 fill_into_edit;
  // This string is displayed adjacent to |fill_into_edit|. Will usually be
  // either the |description| or |content|, whichever isn't represented by
  // |fill_into_edit|. Always empty if kRichAutocompletionShowTitlesParam is
  // disabled.
  base::string16 fill_into_edit_additional_text;
  // When rich autocompleting titles, |fill_into_edit| &
  // |fill_into_edit_additional_text| are swapped (i.e. the former contains the
  // title, and the latter contains the URL). This is consistent with how the
  // suggestion should be displayed. But the shortcut DB must persist the
  // original (i.e. URL) |fill_into_edit|; otherwise the shortcut provider may
  // autocomplete the title even when title autocompletion is inappropriate
  // (e.g. because the input is too short). Therefore, |swapped_fill_into_edit|
  // is set to true when rich autocompleting titles.
  bool swapped_fill_into_edit = false;

  // The inline autocompletion to display after the user's input in the
  // omnibox, if this match becomes the default match.  It may be empty.
  base::string16 inline_autocompletion;
  // The inline autocompletion to display before the user's input in the
  // omnibox, if this match becomes the default match. Always empty if
  // non-prefix autocompletion is disabled.
  base::string16 prefix_autocompletion;

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
  // duplicates.
  GURL stripped_destination_url;

  // Optional image information. Used for entity suggestions. The dominant color
  // can be used to paint the image placeholder while fetching the image.
  std::string image_dominant_color;
  GURL image_url;

  // Optional override to use for types that specify an icon sub-type.
  DocumentType document_type = DocumentType::NONE;

  // Holds the common part of tail suggestion.
  base::string16 tail_suggest_common_prefix;

  // The main text displayed in the address bar dropdown.
  base::string16 contents;
  ACMatchClassifications contents_class;

  // Additional helper text for each entry, such as a title or description.
  base::string16 description;
  ACMatchClassifications description_class;
  // In the case of the document provider, the description includes a last
  // updated date that may become stale. To avoid showing stale descriptions,
  // when |description_for_shortcut| is not empty, it will be stored instead of
  // |description| in the shortcuts provider.
  base::string16 description_for_shortcuts;
  ACMatchClassifications description_class_for_shortcuts;

  // The optional suggestion group Id based on the SuggestionGroupIds enum in
  // suggestion_config.proto. Used to look up the header text this match must
  // appear under from ACResult.
  //
  // If this value exists, it should always be positive and nonzero. In Java and
  // JavaScript, -1 is used as a sentinel value, but should never occur in C++.
  base::Optional<int> suggestion_group_id;

  // If true, UI-level code should swap the contents and description fields
  // before displaying.
  // This field is set when matches are appended to autocomplete results via
  // |AutocompleteResult::AppendMatches| rather than when matches are created.
  bool swap_contents_and_description = false;

  // A rich-format version of the display for the dropdown.
  base::Optional<SuggestionAnswer> answer;

  // The transition type to use when the user opens this match.  By default
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;

  // Type of this match.
  Type type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  // True if we saw a tab that matched this suggestion.
  bool has_tab_match = false;

  // Used to identify the specific source / type for suggestions by the
  // suggest server. See |result_subtypes| in omnibox.proto for more
  // details.
  // We use flat_set to help us deduplicate repetitive elements.
  // The order of elements reported back via AQS is irrelevant, and in the case
  // we have repetitive subtypes (eg. as a result of Chrome enriching the set
  // with its own metadata) we want to merge these subtypes together.
  // flat_set uses std::vector as a container, allowing us to reduce memory
  // overhead of keeping a handful of integers, while offering similar
  // functionality as std::set.
  base::flat_set<int> subtypes;

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
  base::string16 keyword;

  // Set in matches originating from keyword results.
  bool from_keyword = false;

  // Set to a matching pedal if appropriate.  The pedal is not owned, and the
  // owning OmniboxPedalProvider must outlive this.
  OmniboxPedal* pedal = nullptr;

  // True if this match is from a previous result.
  bool from_previous = false;

  // Optional search terms args.  If present,
  // AutocompleteController::UpdateAssistedQueryStats() will incorporate this
  // data with additional data it calculates and pass the completed struct to
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

  // A list of query tiles to be shown as part of this match.
  std::vector<query_tiles::Tile> query_tiles;

  // A list of navsuggest tiles to be shown as part of this match.
  // This object is only populated for TILE_NAVSUGGEST AutocompleteMatches.
  std::vector<NavsuggestTile> navsuggest_tiles;

  // So users of AutocompleteMatch can use the same ellipsis that it uses.
  static const char kEllipsis[];

#if DCHECK_IS_ON()
  // Does a data integrity check on this match.
  void Validate() const;
#endif  // DCHECK_IS_ON()

  // Checks one text/classifications pair for valid values.
  static void ValidateClassifications(
      const base::string16& text,
      const ACMatchClassifications& classifications,
      const std::string& provider_name = "");
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;
typedef std::vector<AutocompleteMatch> ACMatches;

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_
