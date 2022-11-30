// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_VERBATIM_MATCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_VERBATIM_MATCH_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"

class AutocompleteProviderClient;

// An autocomplete provider that supplies the Verbatim Match for websites.
// The match is provided when the user focuses the omnibox, or if the user
// clears the contents of the Omnibox while visiting a website.
// This provider is intended for use on all platforms that always need the
// verbatim match on focus, like the Android Search Ready Omnibox.
// Note that the verbatim match may be also supplied by other providers,
// eg. ZeroSuggestProvider, but it will be deduplicated and the higher score
// from this provider will be used to boost it up to the top.
class ZeroSuggestVerbatimMatchProvider : public AutocompleteProvider {
 public:
  explicit ZeroSuggestVerbatimMatchProvider(AutocompleteProviderClient* client);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~ZeroSuggestVerbatimMatchProvider() override;
  const raw_ptr<AutocompleteProviderClient> client_{nullptr};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_VERBATIM_MATCH_PROVIDER_H_
