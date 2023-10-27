// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_provider.h"

FakeAutocompleteProvider::FakeAutocompleteProvider(Type type)
    : AutocompleteProvider(type) {}

void FakeAutocompleteProvider::Start(const AutocompleteInput& input,
                                     bool minimal_changes) {
  NotifyListeners(true);
}

void FakeAutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  for (auto i(matches_.begin()); i != matches_.end(); ++i) {
    if (i->destination_url == match.destination_url && i->type == match.type) {
      deleted_matches_.push_back(*i);
      matches_.erase(i);
      break;
    }
  }
}

FakeAutocompleteProvider::~FakeAutocompleteProvider() = default;
