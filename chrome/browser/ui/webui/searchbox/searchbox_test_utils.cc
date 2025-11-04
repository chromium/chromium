// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/tabs/public/tab_interface.h"

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
    : AutocompleteController(
          std::move(provider_client),
          AutocompleteControllerConfig{.provider_types = provider_types}) {}
MockAutocompleteController::~MockAutocompleteController() = default;

MockOmniboxEditModel::MockOmniboxEditModel(
    OmniboxController* omnibox_controller)
    : OmniboxEditModel(omnibox_controller) {}
MockOmniboxEditModel::~MockOmniboxEditModel() = default;

MockLensSearchboxClient::MockLensSearchboxClient() = default;
MockLensSearchboxClient::~MockLensSearchboxClient() = default;

MockTabContextualizationController::MockTabContextualizationController(
    tabs::TabInterface* tab_interface)
    : lens::TabContextualizationController(tab_interface) {}
MockTabContextualizationController::~MockTabContextualizationController() =
    default;
