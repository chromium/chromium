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
#include "components/omnibox/browser/autocomplete_enums.h"
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
// of a known "keyword". A keyword is some string that maps to a search query
// URL; the rest of the user's input is taken as the input to the query.  For
// example, the keyword "bug" might map to the URL "http://b/issue?id=%s", so
// input like "bug 123" would become "http://b/issue?id=123".
//
// Because we do prefix matching, user input could match more than one keyword
// at once. (Example: the input "f jazz" matches all keywords starting with
// "f".) We return the best matches, up to three.
//
// The resulting matches are shown with content specified by the keyword
// (usually "Search [name] for %s"), description ("Keyword: [keyword]"), and
// action ("[keyword] %s"). If the user has typed a (possibly partial) keyword
// but no search terms, the suggested result is shown greyed out, with
// "<enter term(s)>" as the substituted input, and does nothing when selected.
class KeywordProvider : public AutocompleteProvider {
 public:
  KeywordProvider(AutocompleteProviderClient* client,
                  AutocompleteProviderListener* listener);
  KeywordProvider(const KeywordProvider&) = delete;
  KeywordProvider& operator=(const KeywordProvider&) = delete;

  // If `text` corresponds to an eligible (e.g. enabled, substituting, etc)
  // `TemplateURL`, returns that `TemplateURL`; returns nullptr otherwise.
  const TemplateURL* GetTemplateUrlForText(
      const std::u16string& text,
      TemplateURLService* template_url_service) const;

  // Creates a fully marked-up AutocompleteMatch for a specific keyword.
  AutocompleteMatch CreateVerbatimMatch(const std::u16string& text,
                                        const std::u16string& keyword,
                                        const AutocompleteInput& input);

  // AutocompleteProvider:
  void DeleteMatch(const AutocompleteMatch& match) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;

 private:
  friend class KeywordExtensionsDelegateImpl;

  ~KeywordProvider() override;

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

  // Fills in the `destination_url` and `contents` fields of `match` with the
  // provided user input and keyword data.
  void FillInUrlAndContents(const std::u16string& remaining_input,
                            const TemplateURL* turl,
                            AutocompleteMatch* match) const;

  TemplateURLService* GetTemplateURLService() const;

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
