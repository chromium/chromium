// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_WRAPPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container_observer.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar_data_model.mojom.h"

class BrowserWindowInterface;
class ExtensionsContainer;
class WebUIToolbarControlDelegate;
class WebUIToolbarExtensionsContainer;

namespace content {
class WebContents;
}

namespace ui {
template <typename T>
class ScopedUnownedUserData;
}

// A wrapper class for WebUIToolbarExtensionsContainer to manage its lifecycle
// and initialization. It implements WebUIToolbarExtensionsContainer::Observer
// to cache extensions state and notify the delegate.
class WebUIToolbarExtensionsContainerWrapper
    : public WebUIToolbarExtensionsContainerObserver {
 public:
  explicit WebUIToolbarExtensionsContainerWrapper(
      WebUIToolbarControlDelegate* delegate);
  WebUIToolbarExtensionsContainerWrapper(
      const WebUIToolbarExtensionsContainerWrapper&) = delete;
  WebUIToolbarExtensionsContainerWrapper& operator=(
      const WebUIToolbarExtensionsContainerWrapper&) = delete;
  ~WebUIToolbarExtensionsContainerWrapper() override;

  void Init(content::WebContents* web_contents);
  void OnThemeChanged();

  void ExecuteUserAction(const std::string& extension_id);
  void ShowContextMenu(ui::mojom::MenuSourceType source,
                       const std::string& extension_id);

  // WebUIToolbarExtensionsContainer::Observer:
  void OnActionsAddedOrUpdated(
      std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
      std::vector<extensions_bar::mojom::ExtensionActionInfoPtr> actions)
      override;
  void OnActionRemoved(std::vector<toolbar_ui_api::mojom::IconUpdatePtr> icons,
                       const std::string& id) override;
  void OnActionPoppedOut(base::OnceClosure callback) override;

 private:
  // This string is used as the extensions_bar::mojom::ExtensionActionInfo::id
  // value to indicate the extensions button (not actually an extension).
  // Empty string should not overlap with actual extension IDs.
  static constexpr char kExtensionsButtonId[] = "";

  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);
  void SendExtensionsState();
  // Returns whether any of `cached_actions_` have access to `web_contents`.
  bool AnyActionHasCurrentSiteAccess(content::WebContents& web_contents);
  // Compute WebUI state for extensions button.
  extensions_bar::mojom::ExtensionActionInfoPtr GetExtensionsButton();

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;

  std::unique_ptr<WebUIToolbarExtensionsContainer> extensions_container_;
  std::unique_ptr<ui::ScopedUnownedUserData<ExtensionsContainer>>
      scoped_extensions_container_user_data_;
  base::CallbackListSubscription active_tab_subscription_;

  // Current state of extensions UI. Map from extension ID to extension state.
  std::map<std::string, extensions_bar::mojom::ExtensionActionInfoPtr>
      cached_actions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_WRAPPER_H_
