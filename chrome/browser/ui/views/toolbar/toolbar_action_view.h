// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/drag_controller.h"

namespace content {
class WebContents;
}

// The View to display an action button in the browser's toolbar using the
// underlying `ToolbarActionViewModel`.
class ToolbarActionView : public views::MenuButton,
                          public ExtensionContextMenuController::Observer {
  METADATA_HEADER(ToolbarActionView, views::MenuButton)

 public:
  // Need DragController here because ToolbarActionView could be
  // dragged/dropped.
  class Delegate : public views::DragController {
   public:
    // Returns the current web contents.
    virtual content::WebContents* GetCurrentWebContents() = 0;

    // Returns the view of the toolbar actions overflow menu to use as a
    // reference point for a popup when this view isn't visible.
    virtual views::LabelButton* GetOverflowReferenceView() const = 0;

    // Returns the preferred size of the ToolbarActionView.
    virtual gfx::Size GetToolbarActionSize() = 0;

    // Instructs the delegate to move this action (as indicated by `action_id`)
    // by the specified `move_by` amount. It is the delegate's responsibility to
    // handle if that would go out-of-bounds, since this class does not know its
    // position.
    virtual void MovePinnedActionBy(const std::string& action_id,
                                    int move_by) = 0;

    // Updates the hover card for `action_view` based on `update_type`.
    virtual void UpdateHoverCard(
        ToolbarActionView* action_view,
        ToolbarActionHoverCardUpdateType update_type) = 0;

    // Called when a context menu is shown.
    virtual void OnContextMenuShown(const std::string& action_id) = 0;

    // Called when a context menu has closed.
    virtual void OnContextMenuClosed(const std::string& action_id) = 0;

   protected:
    ~Delegate() override = default;
  };

  ToolbarActionView(ToolbarActionViewModel* view_model, Delegate* delegate);
  ToolbarActionView(const ToolbarActionView&) = delete;
  ToolbarActionView& operator=(const ToolbarActionView&) = delete;
  ~ToolbarActionView() override;

  void MaybeUpdateHoverCardStatus(const ui::MouseEvent& event);

  // Shows the context menu for the action as a fallback for performing another
  // action.
  void ShowContextMenuAsFallback();

  // Called when a popup is shown. If |by_user| is true, then this was through
  // a direct user action (as opposed to, e.g., an API call).
  void OnPopupShown(bool by_user);

  // Called when a popup is closed.
  void OnPopupClosed();

  // Returns the reference button for the extension action's popup. Rather than
  // relying on the button being a MenuButton, the button returned should have a
  // MenuButtonController. This is part of the ongoing work from
  // http://crbug.com/901183 to simplify the button hierarchy by migrating
  // controller logic into a separate class leaving MenuButton as an empty class
  // to be deprecated.
  views::BubbleAnchor GetReferenceButtonForPopup();

  void UpdateState();

  // views::MenuButton:
  gfx::Rect GetAnchorBoundsInScreen() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

  ToolbarActionViewModel* view_model() { return view_model_; }

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

  // Calls views::View::GetDragOperations() (which is protected).
  int GetDragOperationsForTest(const gfx::Point& point);

 private:
  friend class ToolbarActionHoverCardBubbleViewUITest;

  // views::MenuButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnDragDone() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // ExtensionContextMenuController::Observer:
  void OnContextMenuShown() override;
  void OnContextMenuClosed() override;

  // Like GetReferenceButtonForPopup but with a more precise return type.
  views::Button* GetReferenceButtonForPopupInternal();

  void ButtonPressed();

  // A lock to keep the MenuButton pressed when a menu or popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  // The view model for this toolbar action view.
  raw_ptr<ToolbarActionViewModel> view_model_;

  // Delegate that usually represents a container for ToolbarActionView.
  raw_ptr<Delegate> delegate_;

  // Set to true by a mouse press that will hide a popup due to deactivation.
  // In this case, the next click should not trigger an action, so the popup
  // doesn't hide on mouse press and immediately reshow on mouse release.
  bool suppress_next_release_ = false;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  // The subscription to model updates.
  base::CallbackListSubscription model_subscription_;

  base::WeakPtrFactory<ToolbarActionView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
