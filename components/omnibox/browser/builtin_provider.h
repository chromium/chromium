// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

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

  typedef std::vector<base::string16> Builtins;

  static const int kRelevance;

  void AddMatch(const base::string16& match_string,
                const base::string16& inline_completion,
                const ACMatchClassifications& styles);

  // Returns true if |matches_| contains a match that should be allowed to be
  // the default match. If true, the index of that match in |matches_| is
  // returned in |index|.
  bool HasMatchThatShouldBeDefault(size_t* index) const;

  AutocompleteProviderClient* client_;
  Builtins builtins_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_BUILTIN_PROVIDER_H_
