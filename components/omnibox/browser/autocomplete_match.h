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
      INVISIBLE = 1 << 3,  // "Prefix" text we don't want to see
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

    int style;
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
  enum class DocumentType {
    NONE,
    DRIVE_DOCS,
    DRIVE_FORMS,
    DRIVE_SHEETS,
    DRIVE_SLIDES,
    DRIVE_OTHER
  };

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable,
                    Type type);
  AutocompleteMatch(const AutocompleteMatch& match);
  ~AutocompleteMatch();

  // Converts |type| to a string representation.  Used in logging and debugging.
  AutocompleteMatch& operator=(const AutocompleteMatch& match);

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  // Gets the vector icon identifier for the icon to be shown for |type|. If
  // |is_bookmark| is true, returns a bookmark icon rather than what the type
  // would determine.
  static const gfx::VectorIcon& TypeToVectorIcon(Type type,
                                                 bool is_bookmark,
                                                 DocumentType document_type);
#endif

  // Comparison function for determining when one match is better than another.
  static bool MoreRelevant(const AutocompleteMatch& elem1,
                           const AutocompleteMatch& elem2);

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
  // - If |additional_query_params| is provided, these will be added to the
  //   resulting URL in the cases where a template URL is used. This is used to
  //   distinguish cases such as entity suggestions where the response contains
  //   additional meaningful parameters beyond the search terms themselves.
  static GURL GURLToStrippedGURL(
      const GURL& url,
      const AutocompleteInput& input,
      const TemplateURLService* template_url_service,
      const base::string16& keyword,
      const std::string& additional_query_params = "");

  // Sets the |match_in_scheme|, |match_in_subdomain|, and |match_after_host|
  // flags based on the provided |url| and list of substring |match_positions|.
  // |match_positions| is the [begin, end) positions of a match within the
  // unstripped URL spec.
  using MatchPosition = std::pair<size_t, size_t>;
  static void GetMatchComponents(
      const GURL& url,
      const std::vector<MatchPosition>& match_positions,
      bool* match_in_scheme,
      bool* match_in_subdomain,
      bool* match_after_host);

  // Gets the formatting flags used for display of suggestions. This method
  // encapsulates the return of experimental flags too, so any URLs displayed
  // as an Omnibox suggestion should use this method.
  //
  // This function returns flags that may destructively format the URL, and
  // therefore should never be used for the |fill_into_edit| field.
  //
  // |preserve_scheme|, |preserve_subdomain|, and |preserve_after_host| indicate
  // that these URL components are important (part of the match), and should
  // not be trimmed or elided.
  static url_formatter::FormatUrlTypes GetFormatTypes(bool preserve_scheme,
                                                      bool preserve_subdomain,
                                                      bool preserve_after_host);

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

  // Changes properties to make use of the Pedal (e.g. content, URLs...).
  void ApplyPedal();

  // Adds optional information to the |additional_info| dictionary.
  void RecordAdditionalInfo(const std::string& property,
                            const std::string& value);
  void RecordAdditionalInfo(const std::string& property, int value);
  void RecordAdditionalInfo(const std::string& property, base::Time value);

  // Returns the value recorded for |property| in the |additional_info|
  // dictionary.  Returns the empty string if no such value exists.
  std::string GetAdditionalInfo(const std::string& property) const;

  // Returns whether this match is a "verbatim" match: a URL navigation directly
  // to the user's input, a search for the user's input with the default search
  // engine, or a "keyword mode" search for the query portion of the user's
  // input.  Note that rare or unusual types that could be considered verbatim,
  // such as keyword engine matches or extension-provided matches, aren't
  // detected by this IsVerbatimType, as the user will not be able to infer
  // what will happen when they press enter in those cases if the match is not
  // shown.
  bool IsVerbatimType() const;

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

  // If this match is a tail suggestion, prepends the passed |common_prefix|.
  // If not, but the prefix matches the beginning of the suggestion, dims that
  // portion in the classification.
  void InlineTailPrefix(const base::string16& common_prefix);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Some types of matches (answers for dictionary definitions, e.g.) do not
  // follow the common rules for reversing lines.
  bool IsExceptedFromLineReversal() const;

  // Not to be confused with |has_tab_match|, this returns true if the match
  // has a matching tab and will use a switch-to-tab button. It returns false,
  // for example, when the switch button is not shown because a keyword match
  // is taking precedence.
  bool ShouldShowTabMatch() const;

  // The provider of this match, used to remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  AutocompleteProvider* provider;

  // The relevance of this match. See table in autocomplete.h for scores
  // returned by various providers. This is used to rank matches among all
  // responding providers, so different providers must be carefully tuned to
  // supply matches with appropriate relevance.
  //
  // TODO(pkasting): http://b/1111299 This should be calculated algorithmically,
  // rather than being a fairly fixed value defined by the table above.
  int relevance;

  // How many times this result was typed in / selected from the omnibox.
  // Only set for some providers and result_types.  If it is not set,
  // its value is -1.  At the time of writing this comment, it is only
  // set for matches from HistoryURL and HistoryQuickProvider.
  int typed_count;

  // True if the user should be able to delete this match.
  bool deletable;

  // This string is loaded into the location bar when the item is selected
  // by pressing the arrow keys. This may be different than a URL, for example,
  // for search suggestions, this would just be the search terms.
  base::string16 fill_into_edit;

  // The inline autocompletion to display after the user's typing in the
  // omnibox, if this match becomes the default match.  It may be empty.
  base::string16 inline_autocompletion;

  // If false, the omnibox should prevent this match from being the
  // default match.  Providers should set this to true only if the
  // user's input, plus any inline autocompletion on this match, would
  // lead the user to expect a navigation to this match's destination.
  // For example, with input "foo", a search for "bar" or navigation
  // to "bar.com" should not set this flag; a navigation to "foo.com"
  // should only set this flag if ".com" will be inline autocompleted;
  // and a navigation to "foo/" (an intranet host) or search for "foo"
  // should set this flag.
  bool allowed_to_be_default_match;

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
  std::string image_url;

  // Optional override to use for types that specify an icon sub-type.
  DocumentType document_type;

  // The main text displayed in the address bar dropdown.
  base::string16 contents;
  ACMatchClassifications contents_class;

  // Additional helper text for each entry, such as a title or description.
  base::string16 description;
  ACMatchClassifications description_class;

  // If true, UI-level code should swap the contents and description fields
  // before displaying.
  bool swap_contents_and_description;

  // TODO(jdonnelly): Remove the first two properties once the downstream
  // clients are using the SuggestionAnswer.
  // A rich-format version of the display for the dropdown.
  base::string16 answer_contents;
  base::string16 answer_type;
  base::Optional<SuggestionAnswer> answer;

  // The transition type to use when the user opens this match.  By default
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  ui::PageTransition transition;

  // Type of this match.
  Type type;

  // True if we saw a tab that matched this suggestion.
  bool has_tab_match;

  // Used to identify the specific source / type for suggestions by the
  // suggest server. See |result_subtype_identifier| in omnibox.proto for more
  // details.
  // The identifier 0 is reserved for cases where this specific type is unset.
  int subtype_identifier;

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

  // Set to a matching pedal if appropriate.  The pedal is not owned, and the
  // owning OmniboxPedalProvider must outlive this.
  OmniboxPedal* pedal = nullptr;

  // True if this match is from a previous result.
  bool from_previous;

  // Optional search terms args.  If present,
  // AutocompleteController::UpdateAssistedQueryStats() will incorporate this
  // data with additional data it calculates and pass the completed struct to
  // TemplateURLRef::ReplaceSearchTerms() to reset the match's |destination_url|
  // after the complete set of matches in the AutocompleteResult has been chosen
  // and sorted.  Most providers will leave this as NULL, which will cause the
  // AutocompleteController to do no additional transformations.
  std::unique_ptr<TemplateURLRef::SearchTermsArgs> search_terms_args;

  // Information dictionary into which each provider can optionally record a
  // property and associated value and which is presented in chrome://omnibox.
  AdditionalInfo additional_info;

  // A list of matches culled during de-duplication process, retained to
  // ensure if a match is deleted, the duplicates are deleted as well.
  std::vector<AutocompleteMatch> duplicate_matches;

#if DCHECK_IS_ON()
  // Does a data integrity check on this match.
  void Validate() const;

  // Checks one text/classifications pair for valid values.
  void ValidateClassifications(
      const base::string16& text,
      const ACMatchClassifications& classifications) const;
#endif  // DCHECK_IS_ON()
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;
typedef std::vector<AutocompleteMatch> ACMatches;

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_MATCH_H_
