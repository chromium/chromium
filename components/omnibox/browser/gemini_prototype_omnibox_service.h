// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_H_

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"

class AutocompleteInput;

// A prototype service to fetch suggestions from a Gemini backend.
class GeminiPrototypeOmniboxService : public KeyedService {
 public:
  using SuggestionCallback =
      base::OnceCallback<void(const std::u16string& suggestion)>;

  // Initiates a request for suggestions.
  // |input| is the autocomplete input.
  // |callback| is the callback to be invoked when the suggestions are ready.
  virtual void RequestSuggestions(const AutocompleteInput& input,
                                  SuggestionCallback callback) = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_H_
