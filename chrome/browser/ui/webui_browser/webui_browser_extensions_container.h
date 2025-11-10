// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXTENSIONS_CONTAINER_H_

#include <map>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/webui_browser/extensions_bar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/interaction/element_tracker_views.h"

class Browser;
class WebUIBrowserWindow;

class WebUIBrowserExtensionsContainer
    : public ExtensionsContainerViews,
      public ToolbarActionsModel::Observer,
      public extensions_bar::mojom::PageHandler {
 public:
  WebUIBrowserExtensionsContainer(Browser& browser, WebUIBrowserWindow& window);
  ~WebUIBrowserExtensionsContainer() override;

  // ExtensionsContainerViews:
  ToolbarActionViewModel* GetActionForId(const std::string& action_id) override;
  std::optional<extensions::ExtensionId> GetPoppedOutActionId() const override;
  bool IsActionVisibleOnToolbar(const std::string& action_id) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewModel* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(const extensions::ExtensionId& action_id,
                    base::OnceClosure closure) override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;
  void ShowContextMenuAsFallback(
      const extensions::ExtensionId& action_id) override;
  void OnPopupShown(const extensions::ExtensionId& action_id,
                    bool by_user) override;
  void OnPopupClosed(const extensions::ExtensionId& action_id) override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::BubbleAnchor GetReferenceButtonForPopup(
      const extensions::ExtensionId& action_id) override;

  void CollapseConfirmation() override;

  // ToolbarActionsModel::Observer:
  void OnToolbarModelInitialized() override;
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionRemoved(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarActionUpdated(const ToolbarActionsModel::ActionId& id) override;
  void OnToolbarPinnedActionsChanged() override;

  void Bind(mojo::PendingRemote<extensions_bar::mojom::Page> page,
            mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver);

  void NotifyOfAllActions();
  void NotifyOfOneAction(const ToolbarActionsModel::ActionId& action_id);

  // extensions_bar::mojom::PageHandler:
  void ExecuteUserAction(const std::string& id) override;
  void ShowContextMenu(ui::mojom::MenuSourceType source,
                       const std::string& id) override;
  void ToggleExtensionsMenuFromWebUI() override;

 private:
  class ActionInfo;
  class ContextMenu;

  void NotifyActionPoppedOut(base::OnceClosure closure);

  void CreateActions();
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  void OnContextMenuShownFromToolbar(const std::string& action_id);
  void OnContextMenuClosedFromToolbar();

  const raw_ref<Browser> browser_;
  const raw_ref<WebUIBrowserWindow> window_;
  const raw_ref<ToolbarActionsModel> model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      observe_actions_{this};

  mojo::Receiver<extensions_bar::mojom::PageHandler> receiver_{this};
  mojo::Remote<extensions_bar::mojom::Page> page_;

  std::map<ToolbarActionsModel::ActionId, std::unique_ptr<ActionInfo>> actions_;
  std::optional<std::string> popped_out_action_;
  std::unique_ptr<ContextMenu> context_menu_;

  // The action that triggered the current popup, if any.
  raw_ptr<ToolbarActionViewModel> popup_owner_ = nullptr;

  // Coordinator to show and hide the ExtensionsMenuView.
  const std::unique_ptr<ExtensionsMenuCoordinator> extensions_menu_coordinator_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXTENSIONS_CONTAINER_H_
