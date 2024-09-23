// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/drag_controller.h"

class ExtensionContextMenuController;

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView
// A wrapper around a ToolbarActionViewController to display a toolbar action
// action in the browser's toolbar.
class ToolbarActionView : public views::MenuButton,
                          public ToolbarActionViewDelegateViews {
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

   protected:
    ~Delegate() override = default;
  };

  ToolbarActionView(ToolbarActionViewController* view_controller,
                    Delegate* delegate);
  ToolbarActionView(const ToolbarActionView&) = delete;
  ToolbarActionView& operator=(const ToolbarActionView&) = delete;
  ~ToolbarActionView() override;

  void MaybeUpdateHoverCardStatus(const ui::MouseEvent& event);

  // views::MenuButton:
  gfx::Rect GetAnchorBoundsInScreen() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

  // ToolbarActionViewDelegateViews:
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;

  ToolbarActionViewController* view_controller() {
    return view_controller_;
  }

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

  // ToolbarActionViewDelegateViews:
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Button* GetReferenceButtonForPopup() override;
  void ShowContextMenuAsFallback() override;
  void OnPopupShown(bool by_user) override;
  void OnPopupClosed() override;

  void ButtonPressed();

  // A lock to keep the MenuButton pressed when a menu or popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  // The controller for this toolbar action view.
  raw_ptr<ToolbarActionViewController> view_controller_;

  // Delegate that usually represents a container for ToolbarActionView.
  raw_ptr<Delegate> delegate_;

  // Set to true by a mouse press that will hide a popup due to deactivation.
  // In this case, the next click should not trigger an action, so the popup
  // doesn't hide on mouse press and immediately reshow on mouse release.
  bool suppress_next_release_ = false;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  base::WeakPtrFactory<ToolbarActionView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
