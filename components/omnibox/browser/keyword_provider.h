// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the keyword autocomplete provider. The keyword provider
// is responsible for remembering/suggesting user "search keyword queries"
// (e.g.  "imdb Godzilla") and then fixing them up into valid URLs.  An
// instance of it gets created and managed by the autocomplete controller.
// KeywordProvider uses a TemplateURLService to find the set of keywords.

#ifndef COMPONENTS_OMNIBOX_BROWSER_KEYWORD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_KEYWORD_PROVIDER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;
class KeywordExtensionsDelegate;
class TemplateURL;
class TemplateURLService;

// Autocomplete provider for keyword input.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// To construct these matches, the provider treats user input as a series of
// whitespace-delimited tokens and tries to match the first token as the prefix
// of a known "keyword".  A keyword is some string that maps to a search query
// URL; the rest of the user's input is taken as the input to the query.  For
// example, the keyword "bug" might map to the URL "http://b/issue?id=%s", so
// input like "bug 123" would become "http://b/issue?id=123".
//
// Because we do prefix matching, user input could match more than one keyword
// at once.  (Example: the input "f jazz" matches all keywords starting with
// "f".)  We return the best matches, up to three.
//
// The resulting matches are shown with content specified by the keyword
// (usually "Search [name] for %s"), description "(Keyword: [keyword])", and
// action "[keyword] %s".  If the user has typed a (possibly partial) keyword
// but no search terms, the suggested result is shown greyed out, with
// "<enter term(s)>" as the substituted input, and does nothing when selected.
class KeywordProvider : public AutocompleteProvider {
 public:
  // Returned by `AdjustInputForStarterPackEngines` to represent the stripped
  // input and starter pack. See its comment.
  struct AdjustedInputAndStarterPackEngine {
    AutocompleteInput adjusted_input;
    raw_ptr<const TemplateURL> starter_pack_engine;
  };

  KeywordProvider(AutocompleteProviderClient* client,
                  AutocompleteProviderListener* listener);
  KeywordProvider(const KeywordProvider&) = delete;
  KeywordProvider& operator=(const KeywordProvider&) = delete;

  // Extracts the next whitespace-delimited token from input and returns it.
  // Sets |remaining_input| to everything after the first token (skipping over
  // the first intervening whitespace).
  // If |trim_leading_whitespace| is true then leading whitespace in
  // |*remaining_input| will be trimmed.
  static std::u16string SplitKeywordFromInput(const std::u16string& input,
                                              bool trim_leading_whitespace,
                                              std::u16string* remaining_input);

  // Returns the replacement string from the user input. The replacement
  // string is the portion of the input that does not contain the keyword.
  // For example, the replacement string for "b blah" is blah.
  // If |trim_leading_whitespace| is true then leading whitespace in
  // replacement string will be trimmed.
  static std::u16string SplitReplacementStringFromInput(
      const std::u16string& input,
      bool trim_leading_whitespace);

  // Returns the matching substituting keyword for |input|, or NULL if there
  // is no keyword for the specified input.  If the matching keyword was found,
  // updates |input|'s text and cursor position.
  // |model| must be non-null.
  static const TemplateURL* GetSubstitutingTemplateURLForInput(
      TemplateURLService* model,
      AutocompleteInput* input);

  // If the keyword mode for a starter pack engine, returns `input` with the
  // keyword stripped and the starter pack's `TemplateURL`. E.g. for "@History
  // text", the input 'text' and the history `TemplateURL` are
  // returned. Otherwise, returns `input` untouched and `nullptr`.
  static AdjustedInputAndStarterPackEngine AdjustInputForStarterPackEngines(
      const AutocompleteInput& input,
      TemplateURLService* model);

  // If |text| corresponds (in the sense of
  // TemplateURLModel::CleanUserInputKeyword()) to an enabled, substituting
  // keyword, returns that keyword; returns the empty string otherwise.
  std::u16string GetKeywordForText(const std::u16string& text) const;

  // Creates a fully marked-up AutocompleteMatch for a specific keyword.
  AutocompleteMatch CreateVerbatimMatch(const std::u16string& text,
                                        const std::u16string& keyword,
                                        const AutocompleteInput& input);

  // AutocompleteProvider:
  void DeleteMatch(const AutocompleteMatch& match) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

 private:
  friend class KeywordExtensionsDelegateImpl;

  ~KeywordProvider() override;

  // Extracts the keyword from |input| into |keyword|. Any remaining characters
  // after the keyword are placed in |remaining_input|. Returns true if |input|
  // is valid and has a keyword. This makes use of SplitKeywordFromInput to
  // extract the keyword and remaining string, and uses |template_url_service|
  // to validate and clean up the extracted keyword (e.g., to remove unnecessary
  // characters).
  // In general use this instead of SplitKeywordFromInput.
  // Leading whitespace in |*remaining_input| will be trimmed.
  // |template_url_service| must be non-null.
  static bool ExtractKeywordFromInput(
      const AutocompleteInput& input,
      const TemplateURLService* template_url_service,
      std::u16string* keyword,
      std::u16string* remaining_input);

  // Determines the relevance for some input, given its type, whether the user
  // typed the complete keyword, and whether the user is in
  // "prefer keyword matches" mode, and whether the keyword supports
  // replacement. If |allow_exact_keyword_match| is false, the relevance for
  // keywords that support replacements is degraded.
  static int CalculateRelevance(metrics::OmniboxInputType type,
                                bool complete,
                                bool support_replacement,
                                bool prefer_keyword,
                                bool allow_exact_keyword_match);

  // Creates a fully marked-up AutocompleteMatch from the user's input.
  // If |relevance| is negative, calculate a relevance based on heuristics.
  AutocompleteMatch CreateAutocompleteMatch(
      const TemplateURL* template_url,
      const AutocompleteInput& input,
      size_t prefix_length,
      const std::u16string& remaining_input,
      bool allowed_to_be_default_match,
      int relevance,
      bool deletable);

  // Fills in the "destination_url" and "contents" fields of |match| with the
  // provided user input and keyword data.
  void FillInURLAndContents(const std::u16string& remaining_input,
                            const TemplateURL* element,
                            AutocompleteMatch* match) const;

  TemplateURLService* GetTemplateURLService() const;

  // Removes any unnecessary characters from a user input keyword, returning
  // the resulting keyword.  Usually this means it does transformations such as
  // removing any leading scheme, "www." and trailing slash and returning the
  // resulting string regardless of whether it's a registered keyword.
  // However, if a |template_url_service| is provided and the function finds a
  // registered keyword at any point before finishing those transformations,
  // it'll return that keyword.
  // |template_url_service| must be non-null.
  static std::u16string CleanUserInputKeyword(
      const TemplateURLService* template_url_service,
      const std::u16string& keyword);

  // Input when searching against the keyword provider.
  AutocompleteInput keyword_input_;

  // Model for the keywords.
  raw_ptr<TemplateURLService, DanglingUntriaged> model_;

  // Delegate to handle the extensions-only logic for KeywordProvider.
  // NULL when extensions are not enabled. May be NULL for tests.
  std::unique_ptr<KeywordExtensionsDelegate> extensions_delegate_;

  raw_ptr<AutocompleteProviderClient, DanglingUntriaged> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_KEYWORD_PROVIDER_H_
