// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class TemplateURLService;

// This is the provider for built-in URLs, such as about:settings and
// chrome://version.
class BuiltinProvider : public AutocompleteProvider {
 public:
  explicit BuiltinProvider(AutocompleteProviderClient* client);
  BuiltinProvider(const BuiltinProvider&) = delete;
  BuiltinProvider& operator=(const BuiltinProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~BuiltinProvider() override;

  typedef std::vector<std::u16string> Builtins;

  static const int kRelevance;

  // Populates `matches_` with matching built-in URLs such as about:settings and
  // chrome://version.
  void DoBuiltinAutocompletion(const std::u16string& text);

  // De-deupes the relevance scores, determines if a match can be default, and
  // if a match can be default, updates its relevance score accordingly.
  void UpdateRelevanceScores(const AutocompleteInput& input);

  // Constructs an AutocompleteMatch for built-in URLs such as
  // chrome://settings, etc. and adds it to `matches_`.
  void AddBuiltinMatch(const std::u16string& match_string,
                       const std::u16string& inline_completion,
                       const ACMatchClassifications& styles);

  // Returns true if |matches_| contains a match that should be allowed to be
  // the default match. If true, the index of that match in |matches_| is
  // returned in |index|.
  bool HasMatchThatShouldBeDefault(size_t* index) const;

  raw_ptr<AutocompleteProviderClient> client_;
  Builtins builtins_;
  raw_ptr<TemplateURLService> template_url_service_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_
