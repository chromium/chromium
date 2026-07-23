// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/webui_omnibox_full_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "components/omnibox/common/omnibox_features.h"

// TODO(crbug.com/537796795): Use this handler by default for full webui
//  omnibox. For now it requires two features, see use in omnibox_popup_ui.cc
WebuiOmniboxFullHandler::WebuiOmniboxFullHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxClient> client,
    GetSessionHandleCallback get_session_callback)
    : ContextualSearchboxHandler(std::move(pending_searchbox_handler),
                                 std::move(pending_page),
                                 profile,
                                 web_contents,
                                 std::move(client),
                                 std::move(get_session_callback)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());

  // Ensure the page receives the current autocomplete state on startup.
  // This handles the case where results are generated before the remote is
  // bound and the handler is created and starts observing the
  // AutocompleteController.
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupStabilizeStartupShow)) {
    OnResultChanged(autocomplete_controller(), false);
  }
}

WebuiOmniboxFullHandler::~WebuiOmniboxFullHandler() = default;
