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
  explicit FakeAutocompleteProvider(Type type) : AutocompleteProvider(type) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override {}

  // Used by some tests that create providers ahead of time and later set the
  // specific type needed.
  void SetType(Type type) { type_ = type; }

  using AutocompleteProvider::done_;
  using AutocompleteProvider::matches_;

 private:
  ~FakeAutocompleteProvider() override = default;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_H_
