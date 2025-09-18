// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/gemini_prototype_omnibox_service.h"

class AutocompleteProviderClient;

// An autocomplete provider that suggests a query based on the current page
// context using a Gemini backend. This provider is intended for prototyping
// and is gated by the `kGeminiPrototypeOmniboxProvider` feature flag.
class GeminiPrototypeOmniboxProvider : public AutocompleteProvider {
 public:
  GeminiPrototypeOmniboxProvider(AutocompleteProviderClient* client,
                                 AutocompleteProviderListener* listener);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;

 private:
  ~GeminiPrototypeOmniboxProvider() override;

  // Callback for when the Gemini backend returns a suggestion.
  void OnSuggestionReceived(const std::u16string& suggestion);

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<GeminiPrototypeOmniboxService> service_;
  std::u16string last_suggestion_;
  GURL last_url_;
  // Weak pointers are used to safely cancel callbacks if the provider is
  // stopped.
  base::WeakPtrFactory<GeminiPrototypeOmniboxProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_PROVIDER_H_
