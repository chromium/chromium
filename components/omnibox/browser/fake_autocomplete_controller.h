// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_

#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"

class FakeAutocompleteController : public AutocompleteController {
 public:
  FakeAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            0) {}

  using AutocompleteController::done_;
  using AutocompleteController::in_start_;
  using AutocompleteController::input_;
  using AutocompleteController::metrics_;
  using AutocompleteController::providers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_
