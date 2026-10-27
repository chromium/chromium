// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_

#include <map>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container_observer.h"
#include "components/browser_apis/ui_controllers/toolbar/extensions_bar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/image/image_skia.h"
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
      public ExtensionsToolbarViewModel::Delegate,
      public ExtensionsToolbarViewModel::Observer,
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

  ExtensionsToolbarViewModel* GetToolbarViewModel() {
    return &toolbar_view_model_;
  }

  // ExtensionsContainer:
  ToolbarActionViewModel* GetActionForId(const std::string& action_id) override;
  void HideActivePopup() override;
  void CloseExtensionsMenuIfOpen() override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;
  void ShowPinnedByDefaultIPH(const std::string& extension_id) override;

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
  views::BubbleBorder::Arrow GetPopupArrow() const override;

  void CollapseConfirmation() override;

  // ExtensionsToolbarViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const ToolbarActionsModel::ActionId& action_id,
      ExtensionsContainer* extensions_container) override;
  bool CanShowToolbarActionPopupForAPICall(
      const ToolbarActionsModel::ActionId& action_id) override;

  // ExtensionsToolbarViewModel::Observer:
  void OnActionsInitialized() override;
  void OnActionAdded(const ToolbarActionsModel::ActionId& id) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& id) override;
  void OnActionUpdated(const ToolbarActionsModel::ActionId& id) override;
  void OnPinnedActionsChanged() override;
  void OnActiveWebContentsChanged(bool is_same_document,
                                  content::WebContents* web_contents) override;

  void Bind(mojo::PendingRemote<extensions_bar::mojom::Page> page,
            mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver);

  void NotifyOfAllActions();
  void NotifyOfOneAction(const ToolbarActionsModel::ActionId& action_id);

  // Returns the ElementIdentifier for the extension button with
  // `extension_id`, or kExtensionsMenuButtonElementId if `extension_id` is
  // empty.
  static ui::ElementIdentifier GetElementId(std::string_view extension_id);

  // Returns the secondary identifier of TrackedElements registered by the
  // extension button with `extension_id`.
  static std::string GetSecondaryElementId(std::string_view extension_id);

  // Returns the TrackedElement representing the anchor for the extension button
  // with `extension_id`, or the extensions menu button (puzzle piece) if
  // `extension_id` is empty. Returns nullptr if the element has not registered
  // yet (e.g. while animating in).
  ui::TrackedElement* GetExtensionAnchor(std::string_view extension_id) const;

  // Returns the TrackedElement registered with `element_id` by the extension
  // button with `extension_id`, or nullptr if there is none.
  ui::TrackedElement* GetExtensionElement(ui::ElementIdentifier element_id,
                                          std::string_view extension_id) const;

  // extensions_bar::mojom::PageHandler:
  void ExecuteUserAction(const std::string& id) override;
  void ShowContextMenu(ui::mojom::MenuSourceType source,
                       const std::string& id) override;
  void ToggleExtensionsMenuFromWebUI() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns the extension that's the pinned-by-default IPH anchor, if any. It's
  // set when ShowPinnedByDefaultIPH() accepts a request (which it doesn't do
  // while an action is popped out or there's already an IPH anchor), and
  // cleared when the IPH is closed, fails to show, or is cancelled.
  const std::optional<std::string>&
  pinned_by_default_iph_extension_id_for_testing() const {
    return pinned_by_default_iph_extension_id_;
  }

  // Accessors used by ExtensionsMenuTestUtil to drive this container the same
  // way it drives ExtensionsToolbarDesktop.
  ExtensionsMenuCoordinator* GetExtensionsMenuCoordinatorForTesting() {
    return extensions_menu_coordinator_.get();
  }
  ToolbarActionViewModel* popup_owner_for_testing() { return popup_owner_; }
  int GetNumberOfActionsForTesting() const { return actions_.size(); }

  // Returns the icon currently shown in the toolbar for the action with
  // `action_id`, rasterized with the widget's color provider. The action must
  // exist.
  gfx::ImageSkia GetActionIconForTesting(const std::string& action_id);

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

  void NotifyActionPoppedOut(const std::string& action_id,
                             base::OnceClosure closure);

  void CreateActions();
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  void OnContextMenuShownFromToolbar(const std::string& action_id);
  void OnContextMenuClosedFromToolbar();

  // Returns true if the action must be visible on the toolbar, even if there's
  // not enough room for it, e.g., because it's popped out, has an open context
  // menu, has a widget anchored to it, or is the pinned-by-default IPH anchor.
  // Doesn't consider whether the action is pinned.
  bool ShouldForceVisibility(const std::string& action_id) const;

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ptr<views::Widget> widget_;
  const base::WeakPtr<content::WebContents> web_contents_;
  const bool push_icon_table_updates_;
  const raw_ptr<webui_toolbar::IconTable> icon_table_;
  ExtensionsToolbarViewModel toolbar_view_model_;
  base::ScopedObservation<ExtensionsToolbarViewModel,
                          ExtensionsToolbarViewModel::Observer>
      toolbar_view_model_observation_{this};

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

  // Pinned-by-default IPH support. The flow is:
  //  1. ShowPinnedByDefaultIPH() marks the extension as the IPH anchor, which
  //     causes the WebUI button to register
  //     kExtensionsPinnedByDefaultElementId, then waits, via
  //     NotifyActionPoppedOut(), for that element to be registered and any
  //     animations to finish.
  //  2. ShowPinnedByDefaultIPHNow() requests the promo.
  //  3. When the promo closes (or fails to show) the anchor is cleared.
  //
  // IPH and PopOutAction() requests share the observer's single pending pop
  // out callback. PopOutAction() takes priority: it cancels any IPH, and
  // ShowPinnedByDefaultIPH() does nothing while an action is popped out. The
  // first IPH request also takes priority over later ones, which are ignored
  // until it's closed, fails to show, or is cancelled.
  void ShowPinnedByDefaultIPHNow(const std::string& extension_id);

  // If there's an IPH anchor, clears it and tells the observer to cancel the
  // IPH's pending NotifyActionPoppedOut() callback, if it hasn't been run yet.
  // Does nothing if there's no IPH anchor. Doesn't affect the promo itself, if
  // it's already been requested.
  void ClearPinnedByDefaultIphAnchor();

  // Same as ClearPinnedByDefaultIphAnchor(), but also aborts the promo if it's
  // already been requested, whether it's queued or showing.
  void CancelPinnedByDefaultIph();

  // The extension currently designated as the pinned-by-default IPH anchor.
  std::optional<std::string> pinned_by_default_iph_extension_id_;

  base::WeakPtrFactory<WebUIToolbarExtensionsContainer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_EXTENSIONS_CONTAINER_H_
