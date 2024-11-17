// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/search_engines/search_terms_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

class AutocompleteSchemeClassifier;

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  AutocompleteInput();
  // |text| represents the input query.
  //
  // |current_page_classification| represents the type of page the user is
  // viewing and manner in which the user is accessing the omnibox; it's
  // more than simply the URL.  It includes, for example, whether the page
  // is a search result page doing search term replacement or not.
  //
  // |scheme_classifier| is passed to Parse() to help determine the type of
  // input this is; see comments there.
  AutocompleteInput(const std::u16string& text,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier,
                    bool should_use_https_as_default_scheme = false,
                    int https_port_for_testing = 0,
                    bool use_fake_https_for_https_upgrade_testing = false);
  // This constructor adds |cursor_position|, related to |text|.
  // |cursor_position| represents the location of the cursor within the
  // query |text|. It may be set to std::u16string::npos if the input
  // doesn't come directly from the user's typing.
  AutocompleteInput(const std::u16string& text,
                    size_t cursor_position,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier,
                    bool should_use_https_as_default_scheme = false,
                    int https_port_for_testing = 0,
                    bool use_fake_https_for_https_upgrade_testing = false);

  // This constructor adds |desired_tld|, related to |text|. |desired_tld|
  // is the user's desired TLD, if one is not already present in the text to
  // autocomplete. When this is non-empty, it also implies that "www."
  // should be prepended to the domain where possible. The |desired_tld|
  // should not contain a leading '.' (use "com" instead of ".com").
  AutocompleteInput(const std::u16string& text,
                    size_t cursor_position,
                    const std::string& desired_tld,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier,
                    bool should_use_https_as_default_scheme = false,
                    int https_port_for_testing = 0,
                    bool use_fake_https_for_https_upgrade_testing = false);

  AutocompleteInput(const AutocompleteInput& other);
  ~AutocompleteInput();

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(metrics::OmniboxInputType type);

  // Parses |text| (including an optional |desired_tld|) and returns the type of
  // input this will be interpreted as.  |scheme_classifier| is used to check
  // the scheme in |text| is known and registered in the current environment.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static metrics::OmniboxInputType Parse(
      const std::u16string& text,
      const std::string& desired_tld,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Parsed* parts,
      std::u16string* scheme,
      GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(
      const std::u16string& text,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Component* scheme,
      url::Component* host);

  // Returns true if the given text and url combination should be upgraded to
  // use https:// as the default scheme. If so, fills |upgraded_url| with the
  // upgraded https:// URL. |https_port_for_testing| can be set to a non-zero
  // value in tests to load test cases over net::EmbeddedTestServer.
  static bool ShouldUpgradeToHttps(
      const std::u16string& text,
      const GURL& url,
      int https_port_for_testing,
      bool use_fake_https_for_https_upgrade_testing,
      GURL* upgraded_url);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).  Because this uses Parse() under the hood
  // to determine the meaning of the different strings, callers need to supply a
  // |scheme_classifier| to pass to Parse(). If |offset| is non-null, it will
  // be updated with any changes that shift it.
  static std::u16string FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::u16string& formatted_url,
      const AutocompleteSchemeClassifier& scheme_classifier,
      size_t* offset);

  // Returns the number of non-empty components in |parts| besides the host.
  static int NumNonHostComponents(const url::Parsed& parts);

  // Returns whether |text| begins with "http:" or "view-source:http:".
  static bool HasHTTPScheme(const std::u16string& text);

  // Returns whether |text| begins with "https:" or "view-source:https:".
  static bool HasHTTPSScheme(const std::u16string& text);

  // User-provided text to be completed.
  const std::u16string& text() const { return text_; }

  // Returns 0-based cursor position within |text_| or std::u16string::npos if
  // not used.
  size_t cursor_position() const { return cursor_position_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|, |cursor_position_| and |parts_|.  Only callers who know
  // that they're not changing the type/scheme/etc. should use this.
  void UpdateText(const std::u16string& text,
                  size_t cursor_position,
                  const url::Parsed& parts);

  // The current URL, or an invalid GURL if not applicable or available.
  const GURL& current_url() const { return current_url_; }
  // Providers that trigger on focus need the current URL to produce a match
  // that, when displayed, contain the URL of the current page.
  void set_current_url(const GURL& current_url) { current_url_ = current_url; }

  // The title of the current page, corresponding to the current URL, or empty
  // if this is not available.
  const std::u16string& current_title() const { return current_title_; }
  // This is sometimes set as the description if returning a
  // URL-what-you-typed match for the current URL.
  void set_current_title(const std::u16string& title) {
    current_title_ = title;
  }

  // The type of page that is currently behind displayed and how it is
  // displayed (e.g., with search term replacement or without).
  metrics::OmniboxEventProto::PageClassification current_page_classification()
      const {
    return current_page_classification_;
  }

  // The Suggest or Search request source. Determines the client= (for Suggest
  // request URLs) and source= or sourceid= (for Search request URLs).
  SearchTermsData::RequestSource request_source() const {
    switch (current_page_classification()) {
      // Lens Overlay searchboxes don't rely on TemplateURL replacement and set
      // `client=` in //components/omnibox/browser/remote_suggestions_service.cc
      // and `source=` in //c/b/u/lens/lens_overlay_url_builder.cc.
      case metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX:
      case metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX:
      case metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX:
        return SearchTermsData::RequestSource::LENS_OVERLAY;
      default:
        return SearchTermsData::RequestSource::SEARCHBOX;
    }
  }

  // The type of input supplied.
  metrics::OmniboxInputType type() const { return type_; }

  // Returns parsed URL components.
  const url::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const std::u16string& scheme() const { return scheme_; }

  // The input as a URL to navigate to, if possible.
  const GURL& canonicalized_url() const { return canonicalized_url_; }

  // The user's desired TLD.
  const std::string& desired_tld() const { return desired_tld_; }

  // Returns whether inline autocompletion should be prevented.
  bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }
  // |prevent_inline_autocomplete| is true if the generated result set should
  // not require inline autocomplete for the default match. This is difficult
  // to explain in the abstract; the practical use case is that after the user
  // deletes text in the edit, the HistoryURLProvider should make sure not to
  // promote a match requiring inline autocomplete too highly.
  void set_prevent_inline_autocomplete(bool prevent_inline_autocomplete) {
    prevent_inline_autocomplete_ = prevent_inline_autocomplete;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  bool prefer_keyword() const { return prefer_keyword_; }
  // |prefer_keyword| should be true when the keyword UI is onscreen; this
  // will bias the autocomplete result set toward the keyword provider when
  // the input string is a bare keyword.
  void set_prefer_keyword(bool prefer_keyword) {
    prefer_keyword_ = prefer_keyword;
  }

  // Returns whether this input is allowed to be treated as an exact
  // keyword match.  If not, the default result is guaranteed not to be a
  // keyword search, even if the input is "<keyword> <search string>".
  bool allow_exact_keyword_match() const { return allow_exact_keyword_match_; }
  // |allow_exact_keyword_match| should be false when triggering keyword
  // mode on the input string would be surprising or wrong, e.g.  when
  // highlighting text in a page and telling the browser to search for it or
  // navigate to it. This member only applies to substituting keywords.
  void set_allow_exact_keyword_match(bool allow_exact_keyword_match) {
    allow_exact_keyword_match_ = allow_exact_keyword_match;
  }

  // Provides public read-only access to the method that the user used to
  // get into keyword mode (which includes INVALID if they didn't enter it.)
  metrics::OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method()
      const {
    return keyword_mode_entry_method_;
  }

  // Used by code handling keyword entry to set the method by which the user
  // used to enter it.
  void set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method) {
    keyword_mode_entry_method_ = entry_method;
  }

  // Returns whether providers should avoid obtaining matches asynchronously
  // when processing the input.
  bool omit_asynchronous_matches() const { return omit_asynchronous_matches_; }
  // If |omit_asynchronous_matches| is true, the controller asks the
  // providers to only return matches which are synchronously available,
  // which should mean that all providers will be done immediately.
  void set_omit_asynchronous_matches(bool omit_asynchronous_matches) {
    omit_asynchronous_matches_ = omit_asynchronous_matches;
  }

  // Returns the type of UI interaction that started this autocomplete query.
  metrics::OmniboxFocusType focus_type() const { return focus_type_; }
  // |focus_type| should specify the UI interaction that started autocomplete.
  // Generally, this should be left alone as INTERACTION_DEFAULT. Most providers
  // only provide results for the INTERACTION_DEFAULT focus type. Providers like
  // ZeroSuggestProvider that only want to display matches on-focus or
  // on-clobber will look at this flag.
  void set_focus_type(metrics::OmniboxFocusType focus_type) {
    focus_type_ = focus_type;
  }

  // Returns the terms in |text_| that start with http:// or https:// plus
  // at least one more character, stored without the scheme.  Used in
  // duplicate elimination to detect whether, for a given URL, the user may
  // have started typing that URL with an explicit scheme; see comments on
  // AutocompleteMatch::GURLToStrippedGURL().
  const std::vector<std::u16string>& terms_prefixed_by_http_or_https() const {
    return terms_prefixed_by_http_or_https_;
  }

  const std::optional<lens::proto::LensOverlaySuggestInputs>&
  lens_overlay_suggest_inputs() const {
    return lens_overlay_suggest_inputs_;
  }

  void set_lens_overlay_suggest_inputs(
      const lens::proto::LensOverlaySuggestInputs&
          lens_overlay_suggest_inputs) {
    lens_overlay_suggest_inputs_ = lens_overlay_suggest_inputs;
  }

  // Resets all internal variables to the null-constructed state.
  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  void set_added_default_scheme_to_typed_url(
      bool added_default_scheme_to_typed_url) {
    added_default_scheme_to_typed_url_ = added_default_scheme_to_typed_url;
  }

  bool added_default_scheme_to_typed_url() const {
    return added_default_scheme_to_typed_url_;
  }

  bool typed_url_had_http_scheme() const { return typed_url_had_http_scheme_; }

  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Returns true if in zero prefix input state.
  // Zero-Suggest state is determined from focus type and is used to inform
  // autocomplete providers, tab matching, and action attachment. Note that the
  // Zero-Suggest state does NOT mean that `text_` is empty.
  bool IsZeroSuggest() const;

  // Uses the keyword entry mode to decide if the user is currently in keyword
  // mode.
  bool InKeywordMode() const;

 private:
  friend class AutocompleteProviderTest;

  // The common initialization of the non-default constructors, called after
  // the initial fields are set. These remaining parameters are used as inputs
  // to setting the remaining fields.
  void Init(const std::u16string& text,
            const AutocompleteSchemeClassifier& scheme_classifier);

  // NOTE: Whenever adding a new field here, please make sure to update Clear()
  // and EstimateMemoryUsage() methods.
  std::u16string text_;
  size_t cursor_position_;
  GURL current_url_;
  std::u16string current_title_;
  metrics::OmniboxEventProto::PageClassification current_page_classification_;
  metrics::OmniboxInputType type_;
  url::Parsed parts_;
  std::u16string scheme_;
  GURL canonicalized_url_;
  std::string desired_tld_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  metrics::OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method_;
  bool omit_asynchronous_matches_;
  metrics::OmniboxFocusType focus_type_ =
      metrics::OmniboxFocusType::INTERACTION_DEFAULT;
  std::vector<std::u16string> terms_prefixed_by_http_or_https_;
  // The lens overlay suggest inputs to be sent as query parameters in
  // the suggest requests.
  std::optional<lens::proto::LensOverlaySuggestInputs>
      lens_overlay_suggest_inputs_;

  // Flags for OmniboxDefaultNavigationsToHttps feature.
  bool should_use_https_as_default_scheme_;
  bool added_default_scheme_to_typed_url_ = false;
  bool typed_url_had_http_scheme_ = false;
  // Port used by the embedded https server in tests. This is used to determine
  // the correct port while upgrading URLs to https if the original URL has a
  // non-default port.
  // TODO(crbug.com/40743298): Remove when URLLoaderInterceptor can simulate
  // redirects.
  int https_port_for_testing_;
  // If true, indicates that the tests are using a faux-HTTPS server which is
  // actually an HTTP server that pretends to serve HTTPS responses. Should only
  // be true on iOS.
  bool use_fake_https_for_https_upgrade_testing_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_
