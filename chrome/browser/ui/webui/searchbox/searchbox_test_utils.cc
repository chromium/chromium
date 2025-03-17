// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"

MockSearchboxPage::MockSearchboxPage() = default;
MockSearchboxPage::~MockSearchboxPage() = default;

mojo::PendingRemote<searchbox::mojom::Page>
MockSearchboxPage::BindAndGetRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

MockAutocompleteController::MockAutocompleteController(
    std::unique_ptr<AutocompleteProviderClient> provider_client,
    int provider_types)
    : AutocompleteController(std::move(provider_client), provider_types) {}
MockAutocompleteController::~MockAutocompleteController() = default;

MockOmniboxEditModel::MockOmniboxEditModel(
    OmniboxController* omnibox_controller,
    OmniboxView* view)
    : OmniboxEditModel(omnibox_controller, view) {}
MockOmniboxEditModel::~MockOmniboxEditModel() = default;

MockLensSearchboxClient::MockLensSearchboxClient() = default;
MockLensSearchboxClient::~MockLensSearchboxClient() = default;
