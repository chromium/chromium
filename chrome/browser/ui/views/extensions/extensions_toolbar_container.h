// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "extensions/common/extension.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionsToolbarButton;
class ToolbarActionViewController;
class ExtensionsMenuCoordinator;

// Container for extensions shown in the toolbar. These include pinned
// extensions and extensions that are 'popped out' transitively to show dialogs
// or be called out to the user.
class ExtensionsToolbarContainer : public ToolbarIconContainerView,
                                   public ExtensionsContainer,
                                   public ToolbarActionView::Delegate,
                                   public views::WidgetObserver {
  METADATA_HEADER(ExtensionsToolbarContainer, ToolbarIconContainerView)

 public:
  using ToolbarIcons =
      std::map<ToolbarActionsModel::ActionId, ToolbarActionView*>;

  // Determines how the container displays - specifically whether the menu and
  // popped out action can be hidden.
  enum class DisplayMode {
    // In normal mode, the menu icon and popped-out action is always visible.
    // Normal mode is used for the main toolbar and in windows where there is
    // always enough space to show at least two icons.
    kNormal,
    // In compact mode, one or both of the menu icon and popped-out action may
    // be hidden if the available space does not allow for them. Compact mode is
    // used in smaller windows (e.g. web apps) where
    // there may not be enough space to display the buttons.
    // TODO(crbug.com/40159931): Remove kCompact in favour of kAutoHide once the
    // |kDesktopPWAsElidedExtensionsMenu| flag is removed.
    kCompact,
    // In auto hide mode the menu icon is hidden until
    // extensions_button()->ToggleExtensionsMenu() is called by the embedder.
    // This is used for windows that want to minimize the number of visible
    // icons in their toolbar (e.g. web apps).
    kAutoHide,
  };

  static void SetOnVisibleCallbackForTesting(base::OnceClosure callback);

  ExtensionsMenuCoordinator* GetExtensionsMenuCoordinatorForTesting() {
    return extensions_menu_coordinator_.get();
  }

  explicit ExtensionsToolbarContainer(
      Browser* browser,
      DisplayMode display_mode = DisplayMode::kNormal);
  ExtensionsToolbarContainer(const ExtensionsToolbarContainer&) = delete;
  ExtensionsToolbarContainer& operator=(const ExtensionsToolbarContainer&) =
      delete;
  ~ExtensionsToolbarContainer() override;

  // Creates toolbar actions and icons corresponding to the model. This is only
  // called in the constructor or when the model initializes and should not be
  // called for subsequent changes to the model.
  void CreateActions();

  // Adds the action view corresponding to `action_id` to the toolbar and
  // updates the container visibility, reordering views if necessary.
  void AddAction(const ToolbarActionsModel::ActionId& action_id);

  // Removes the action view corresponding to `action_id` to the toolbar and
  // updates the container visibility, reordering views if necessary.
  void RemoveAction(const ToolbarActionsModel::ActionId& action_id);

  // Updates the action view corresponding to `action_id` to the toolbar and
  // updates the container visibility, reordering views if necessary.
  void UpdateAction(const ToolbarActionsModel::ActionId& action_id);

  // Adds the visible action views the toolbar and updates the container
  // visibility, reordering views if necessary.
  void UpdatePinnedActions();

  // Updates `extensions_button_` icon given user `site_setting` and whether
  // `is_restricted_url` in `web_contents`.
  void UpdateExtensionsButton(
      extensions::PermissionsManager::UserSiteSetting site_setting,
      content::WebContents* web_contents,
      bool is_restricted_url);

  // Updates the `request_access_button_` given user `site_setting` for the
  // current `web_contents`.
  void UpdateRequestAccessButton(
      extensions::PermissionsManager::UserSiteSetting site_setting,
      content::WebContents* web_contents);

  // Updates the container visibility and animation as needed.
  void UpdateContainerVisibility();

  // Updates the controls visibility.
  void UpdateControlsVisibility();

  ToolbarActionViewController* popup_owner_for_testing() {
    return popup_owner_;
  }

  // Gets the extension menu button for the toolbar.
  ExtensionsToolbarButton* GetExtensionsButton() const {
    return extensions_button_;
  }

  // Get the request access button for the toolbar.
  ExtensionsRequestAccessButton* GetRequestAccessButton() const {
    return request_access_button_;
  }

  // Get the view corresponding to the extension |id|, if any.
  ToolbarActionView* GetViewForId(const std::string& id);

  // Pop out and show the extension corresponding to |extension_id|, then show
  // the Widget when the icon is visible. If the icon is already visible the
  // action will be posted immediately (not run synchronously).
  void ShowWidgetForExtension(views::Widget* widget,
                              const std::string& extension_id);

  // Check if the extensions menu is showing.
  // TODO(crbug.com/40811196): This method will be removed once extensions menu
  // under kExtensionsMenuAccessControl feature is fully rolled out and we can
  // call directly into the menu coordinator.
  bool IsExtensionsMenuShowing() const;

  // Event handler for when the extensions menu is opened.
  void OnMenuOpening();

  // Event handler for when the extensions menu is closed.
  void OnMenuClosed();

  // Gets the widget that anchors to the extension (or is about to anchor to the
  // extension, pending pop-out).
  views::Widget* GetAnchoredWidgetForExtensionForTesting(
      const std::string& extension_id);

  std::optional<extensions::ExtensionId>
  GetExtensionWithOpenContextMenuForTesting() {
    return extension_with_open_context_menu_id_;
  }

  int GetNumberOfActionsForTesting() { return actions_.size(); }

  ToolbarButton* GetCloseSidePanelButtonForTesting() {
    return close_side_panel_button_;
  }

  // Called when the side panel state has changed for an extensions side panel
  // to pop out button reflecting the side panel being open.
  void UpdateSidePanelState(bool is_active);

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;

  // ExtensionsContainer:
  ToolbarActionViewController* GetActionForId(
      const std::string& action_id) override;
  std::optional<extensions::ExtensionId> GetPoppedOutActionId() const override;
  void OnContextMenuShownFromToolbar(const std::string& action_id) override;
  void OnContextMenuClosedFromToolbar() override;
  bool IsActionVisibleOnToolbar(const std::string& action_id) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewController* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(const extensions::ExtensionId& action_id,
                    base::OnceClosure closure) override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;
  void UpdateToolbarActionHoverCard(
      ToolbarActionView* action_view,
      ToolbarActionHoverCardUpdateType update_type) override;
  void CollapseConfirmation() override;

  // ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override;
  views::LabelButton* GetOverflowReferenceView() const override;
  gfx::Size GetToolbarActionSize() override;
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

 private:
  friend class ToolbarActionHoverCardBubbleViewUITest;

  // A struct representing the position and action being dragged.
  struct DropInfo;

  // Pairing of widgets associated with this container and the extension they
  // are associated with. This is used to keep track of icons that are popped
  // out due to a widget showing (or being queued to show).
  struct AnchoredWidget {
    raw_ptr<views::Widget> widget;
    std::string extension_id;
  };

  // Hides the currently-showing extensions menu, if it exists.
  // TODO(crbug.com/40811196): This method will be removed once extensions menu
  // under kExtensionsMenuAccessControl feature is fully rolled out and we can
  // call directly into the menu coordinator.
  void HideExtensionsMenu();

  // Determines whether an action must be visible (i.e. cannot be hidden for any
  // reason). Returns true if the action is popped out or has an attached
  // bubble.
  bool ShouldForceVisibility(const std::string& extension_id) const;

  // Updates the view's visibility state according to
  // IsActionVisibleOnToolbar(). Note that IsActionVisibleOnToolbar() does not
  // return View visibility but whether the action should be visible or not
  // (according to pin and pop-out state).
  void UpdateIconVisibility(const std::string& extension_id);

  // Set |widget|'s anchor (to the corresponding extension) and then show it.
  // Posted from |ShowWidgetForExtension|.
  void AnchorAndShowWidgetImmediately(MayBeDangling<views::Widget> widget);

  // Creates an action and toolbar button for the corresponding ID.
  void CreateActionForId(const ToolbarActionsModel::ActionId& action_id);

  // Sorts child views to display them in the correct order (pinned actions,
  // popped out actions, other buttons).
  void ReorderAllChildViews();

  // Utility function for going from width to icon counts.
  size_t WidthToIconCount(int x_offset);

  ui::ImageModel GetExtensionIcon(ToolbarActionView* extension_view);

  // Sets a pinned extension button's image to be shown/hidden.
  void SetExtensionIconVisibility(ToolbarActionsModel::ActionId id,
                                  bool visible);

  // Returns whether the contianer should be showing, e.g. not if there are no
  // extensions installed, nor if the container is inactive in kAutoHide mode.
  bool ShouldContainerBeVisible() const;

  // Queues up a call to UpdateContainerVisibility() for when the current layout
  // animation ends.
  void UpdateContainerVisibilityAfterAnimation();

  // Triggers the side panel to close.
  void CloseSidePanelButtonPressed();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Moves the dragged extension `action_id`.
  void MovePinnedAction(
      const ToolbarActionsModel::ActionId& action_id,
      size_t index,
      base::ScopedClosureRunner cleanup,
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Performs clean up after dragging.
  void DragDropCleanup(
      const ToolbarActionsModel::ActionId& dragged_extension_id);

  // Updates the vector icon used when the PrefChangeRegistrar listens to a
  // change. When the side panel should open to the right side of the browser
  // the default vector icon is used. When the side panel should open to the
  // left side of the browser the flipped vector icon is used.
  void UpdateCloseSidePanelButtonIcon();

  const raw_ptr<Browser> browser_;
  const raw_ptr<ToolbarActionsModel> model_;

  // Coordinator to show and hide the ExtensionsMenuView.
  const std::unique_ptr<ExtensionsMenuCoordinator> extensions_menu_coordinator_;

  const raw_ptr<ExtensionsToolbarButton, AcrossTasksDanglingUntriaged>
      extensions_button_;
  raw_ptr<ExtensionsRequestAccessButton, DanglingUntriaged>
      request_access_button_ = nullptr;

  DisplayMode display_mode_;

  // Controller for showing the toolbar action hover card.
  std::unique_ptr<ToolbarActionHoverCardController>
      action_hover_card_controller_;

  // TODO(pbos): Create actions and icons only for pinned pinned / popped out
  // actions (lazily). Currently code expects GetActionForId() to return
  // actions for extensions that aren't visible.
  // Actions for all extensions.
  std::vector<std::unique_ptr<ToolbarActionViewController>> actions_;
  // View for every action, does not imply pinned or currently shown.
  ToolbarIcons icons_;
  // Popped-out extension, if any.
  std::optional<extensions::ExtensionId> popped_out_action_;
  // The action that triggered the current popup, if any.
  raw_ptr<ToolbarActionViewController> popup_owner_ = nullptr;
  // Extension with an open context menu, if any.
  std::optional<extensions::ExtensionId> extension_with_open_context_menu_id_;
  // View for closing the extension side panel.
  raw_ptr<ToolbarButton> close_side_panel_button_ = nullptr;
  // Used to ensure the button remains highlighted while active.
  std::optional<views::Button::ScopedAnchorHighlight>
      close_side_panel_button_anchor_highlight_;

  // The widgets currently popped out and, for each, the extension it is
  // associated with. See AnchoredWidget.
  std::vector<AnchoredWidget> anchored_widgets_;

  // The DropInfo for the current drag-and-drop operation, or a null pointer if
  // there is none.
  std::unique_ptr<DropInfo> drop_info_;

  // Observes and listens to side panel alignment changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ExtensionsToolbarContainer> weak_ptr_factory_{this};

  base::WeakPtrFactory<ExtensionsToolbarContainer> drop_weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
