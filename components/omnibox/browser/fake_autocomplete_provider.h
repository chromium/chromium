// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_H_

#include "components/omnibox/browser/autocomplete_provider.h"

// A simple `AutocompleteProvider` that does nothing. Useful for creating
// `AutocompleteMatch`s in tests with specific provider types.
class FakeAutocompleteProvider : public AutocompleteProvider {
 public:
  explicit FakeAutocompleteProvider(Type type);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

  using AutocompleteProvider::done_;
  using AutocompleteProvider::matches_;
  using AutocompleteProvider::type_;

  ACMatches deleted_matches_;

 protected:
  ~FakeAutocompleteProvider() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_H_
