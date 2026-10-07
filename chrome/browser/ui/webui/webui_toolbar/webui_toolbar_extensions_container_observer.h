// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_OBSERVER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar.mojom.h"

// Observer that gets notifications when the extension UI should be updated.
class WebUIToolbarExtensionsContainerObserver {
 public:
  virtual ~WebUIToolbarExtensionsContainerObserver() = default;

  // The UI for all extensions in `actions` should be updated.
  // If `push_icon_table_updates` was `true`, then icon table updates are
  // placed in `icons`.
  virtual void OnActionsAddedOrUpdated(
      std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
      std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> actions) = 0;

  // The UI for extension `id` should be removed.
  // If `push_icon_table_updates` was `true`, then icon table updates are
  // placed in `icons`.
  virtual void OnActionRemoved(
      std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
      const std::string& id) = 0;

  // Called when an action has been popped out. `callback` is expected to
  // be called when it's safe to anchor things to newly-visible button.
  //
  // There will also be an ActionsAddedOrUpdated before this message.
  virtual void OnActionPoppedOut(base::OnceClosure callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_OBSERVER_H_
