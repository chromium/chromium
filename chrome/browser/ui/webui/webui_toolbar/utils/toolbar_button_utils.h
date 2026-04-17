// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_

#include <optional>

#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;

namespace webui_toolbar {

// Get the list of ElementIdentifiers for the WebUI-specific pinned toolbar
// actions.
std::vector<ui::ElementIdentifier> GetPinnedToolbarActionElementIds();

// Gets the pin state from user prefs.
bool IsButtonPinned(BrowserWindowInterface* browser_interface,
                    toolbar_ui_api::mojom::ToolbarButtonType type);

// Convert Pinned Toolbar Action `action` into an ElementIdentifier.
ui::ElementIdentifier ActionIdToElementIdentifier(actions::ActionId action);

// Convert Pinned Toolbar Action `action` to a mojo'able enum value.
std::optional<toolbar_ui_api::mojom::PinnedToolbarAction>
ActionIdToPinnedToolbarAction(actions::ActionId action);

// Convert Pinned Toolbar Action `action` from a mojo'able enum value.
std::optional<actions::ActionId> PinnedToolbarActionToActionId(
    toolbar_ui_api::mojom::PinnedToolbarAction action);

}  // namespace webui_toolbar

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_UTILS_TOOLBAR_BUTTON_UTILS_H_
