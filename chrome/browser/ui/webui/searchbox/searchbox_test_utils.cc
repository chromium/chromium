// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/tabs/public/tab_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/mock_tab_list_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/webui/webui_embedding_context.h"  // nogncheck crbug.com/40147906

void SetupMockBrowserWindowInterface(
    MockBrowserWindowInterface& window_interface,
    Profile* profile,
    BrowserWindowFeatures& features,
    ui::UnownedUserDataHost& user_data_host,
    tabs::MockTabInterface* active_tab,
    content::WebContents* web_contents) {
  ON_CALL(window_interface, GetFeatures())
      .WillByDefault(testing::ReturnRef(features));
  ON_CALL(window_interface, GetProfile())
      .WillByDefault(testing::Return(profile));
  ON_CALL(window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  if (active_tab) {
    ON_CALL(window_interface, GetActiveTabInterface())
        .WillByDefault(testing::Return(active_tab));
  }
  if (web_contents) {
    webui::SetBrowserWindowInterface(web_contents, &window_interface);
  }
}

void SetupMockTabListInterface(MockTabListInterface& tab_list,
                               tabs::MockTabInterface* active_tab) {
  if (active_tab) {
    ON_CALL(tab_list, GetActiveTab())
        .WillByDefault(testing::Return(active_tab));
  }
}

void SetupMockTabInterface(tabs::MockTabInterface& mock_tab,
                           content::WebContents* contents,
                           Profile* profile,
                           BrowserWindowInterface* window_interface,
                           ui::UnownedUserDataHost* user_data_host) {
  ON_CALL(mock_tab, GetContents()).WillByDefault(testing::Return(contents));
  ON_CALL(mock_tab, GetProfile()).WillByDefault(testing::Return(profile));
  if (window_interface) {
    ON_CALL(mock_tab, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(window_interface));
  }
  if (user_data_host) {
    ON_CALL(mock_tab, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(*user_data_host));
  }
}
#endif

MockSearchboxPage::MockSearchboxPage() = default;
MockSearchboxPage::~MockSearchboxPage() = default;

mojo::PendingRemote<searchbox::mojom::Page>
MockSearchboxPage::BindAndGetRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}

#if !BUILDFLAG(IS_ANDROID)
MockOmniboxPopupPage::MockOmniboxPopupPage() = default;
MockOmniboxPopupPage::~MockOmniboxPopupPage() = default;

mojo::PendingRemote<omnibox_popup::mojom::Page>
MockOmniboxPopupPage::BindAndGetRemote() {
  DCHECK(!receiver_.is_bound());
  return receiver_.BindNewPipeAndPassRemote();
}
#endif

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
