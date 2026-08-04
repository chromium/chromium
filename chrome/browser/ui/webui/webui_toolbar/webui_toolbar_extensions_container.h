// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_

#include <map>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container_observer.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class ExtensionsMenuCoordinator;
class WebUIToolbarWebViewBrowserTest;
class WebUIToolbarWebViewInteractiveUiTest;

namespace webui_toolbar {
class IconTable;
}

class WebUIToolbarExtensionsContainer
    : public ExtensionsContainer,
      public ExtensionsContainerViews,
      public ToolbarActionsModel::Observer,
      public extensions_bar::mojom::PageHandler,
      public views::WidgetObserver {
 public:
  // `push_icon_table_updates` controls whether this instance is responsible for
  // pushing IconTable updates via Mojo.
  WebUIToolbarExtensionsContainer(
      BrowserWindowInterface& browser,
      views::Widget* widget,
      base::WeakPtr<content::WebContents> web_contents,
      webui_toolbar::IconTable* icon_table,
      bool push_icon_table_updates);
  ~WebUIToolbarExtensionsContainer() override;

  // Send extensions UI change notifications to `observer`. `Bind()` cannot be
  // called if an observer is set and an observer cannot be set if `Bind()` is
  // called.
  void SetObserver(WebUIToolbarExtensionsContainerObserver* observer);

  void MoveExtensionAction(const std::string& extension_id,
                           int32_t target_index);
  void MoveExtensionActionBy(const std::string& extension_id, int32_t delta);
  std::vector<std::string> GetOrderedActionIds() const;

  // ExtensionsContainer:
  ToolbarActionViewModel* GetActionForId(const std::string& action_id) override;
  void HideActivePopup() override;
  void CloseExtensionsMenuIfOpen() override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;

  // ExtensionsContainerViews:
  std::optional<extensions::ExtensionId> GetPoppedOutActionId() const override;
  bool IsVisible() const override;
  bool IsActionVisibleOnToolbar(const std::string& action_id) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewModel* popup_owner) override;
  void PopOutAction(const extensions::ExtensionId& action_id,
                    base::OnceClosure closure) override;
  void ShowWidgetForExtension(views::Widget* widget,
                              const std::string& extension_id) override;
  void ShowContextMenuAsFallback(
      const extensions::ExtensionId& action_id) override;
  void OnPopupShown(const extensions::ExtensionId& action_id,
                    bool by_user) override;
  void OnPopupClosed(const extensions::ExtensionId& action_id) override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::BubbleAnchor GetReferenceButtonForPopup(
      const extensions::ExtensionId& action_id) override;
  views::BubbleAnchor GetExtensionsButtonAnchor() override;

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

  // Returns the ElementIdentifier for the extension button with
  // `extension_id`, or kExtensionsMenuButtonElementId if `extension_id` is
  // empty.
  static ui::ElementIdentifier GetElementId(std::string_view extension_id);

  // Returns the TrackedElement representing the anchor for the extension button
  // with `extension_id`, or the extensions menu button (puzzle piece) if
  // `extension_id` is empty. Returns nullptr if the element has not registered
  // yet (e.g. while animating in).
  ui::TrackedElement* GetExtensionAnchor(std::string_view extension_id) const;

  // extensions_bar::mojom::PageHandler:
  void ExecuteUserAction(const std::string& id) override;
  void ShowContextMenu(ui::mojom::MenuSourceType source,
                       const std::string& id) override;
  void ToggleExtensionsMenuFromWebUI() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewInteractiveUiTest,
                           ExtensionUserActionsPlumbing);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewBrowserTest, ExtensionAnchoring);
  FRIEND_TEST_ALL_PREFIXES(WebUIToolbarWebViewBrowserTest,
                           ShowWidgetForExtension);
  class ActionInfo;
  class ContextMenu;

  // `AnchoredWidget` keeps track of `widget` anchoring to the extension button
  // for extension with ID `extension_id`. `subscription` is used to wait for
  // the button to become visible before actually showing and anchoring
  // `widget`.
  struct AnchoredWidget {
    AnchoredWidget(views::Widget* w, std::string id);
    ~AnchoredWidget();
    AnchoredWidget(AnchoredWidget&&);
    AnchoredWidget& operator=(AnchoredWidget&&);

    raw_ptr<views::Widget> widget;
    std::string extension_id;
    ui::ElementTracker::Subscription subscription;
  };

  views::Widget* GetWidget() const;
  ui::TrackedElement* GetExtensionsMenuButtonAnchor() const;

  void NotifyActionPoppedOut(base::OnceClosure closure);

  void CreateActions();
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  void OnContextMenuShownFromToolbar(const std::string& action_id);
  void OnContextMenuClosedFromToolbar();

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ptr<views::Widget> widget_;
  const base::WeakPtr<content::WebContents> web_contents_;
  const bool push_icon_table_updates_;
  const raw_ptr<webui_toolbar::IconTable> icon_table_;
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

  raw_ptr<WebUIToolbarExtensionsContainerObserver> observer_ = nullptr;

  // This function is called when an AnchoredWidget::subscription notifies us
  // that an anchor is potentially ready for use with `widget`.
  void AnchorAndShowWidgetImmediately(views::Widget* widget,
                                      ui::TrackedElement* unused_anchor);

  std::vector<AnchoredWidget> anchored_widgets_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_
