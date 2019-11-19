// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_

#include "base/callback.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

class ExtensionContextMenuController;

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView
// A wrapper around a ToolbarActionViewController to display a toolbar action
// action in the BrowserActionsContainer.
class ToolbarActionView : public views::MenuButton,
                          public ToolbarActionViewDelegateViews,
                          public views::ButtonListener {
 public:
  // Need DragController here because ToolbarActionView could be
  // dragged/dropped.
  class Delegate : public views::DragController {
   public:
    // Returns the current web contents.
    virtual content::WebContents* GetCurrentWebContents() = 0;

    // Whether the container for this button is shown inside a menu.
    virtual bool ShownInsideMenu() const = 0;

    // Whether the container supports showing extensions outside of the menu.
    virtual bool CanShowIconInToolbar() const;

    // Notifies that a drag completed.
    virtual void OnToolbarActionViewDragDone() = 0;

    // Returns the view of the toolbar actions overflow menu to use as a
    // reference point for a popup when this view isn't visible.
    virtual views::LabelButton* GetOverflowReferenceView() const = 0;

    // Returns the preferred size of the ToolbarActionView.
    virtual gfx::Size GetToolbarActionSize() = 0;

   protected:
    ~Delegate() override {}
  };

  // Callback type used for testing.
  using ContextMenuCallback = base::Callback<void(ToolbarActionView*)>;

  ToolbarActionView(ToolbarActionViewController* view_controller,
                    Delegate* delegate);
  ToolbarActionView(const ToolbarActionView&) = delete;
  ToolbarActionView& operator=(const ToolbarActionView&) = delete;
  ~ToolbarActionView() override;

  // views::MenuButton:
  gfx::Rect GetAnchorBoundsInScreen() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // ToolbarActionViewDelegateViews:
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

  ToolbarActionViewController* view_controller() {
    return view_controller_;
  }

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

  bool IsMenuRunningForTesting() const;

  bool wants_to_run_for_testing() const { return wants_to_run_; }

  ExtensionContextMenuController* context_menu_controller_for_testing() const {
    return context_menu_controller_.get();
  }

  static const char kClassName[];

 private:
  // views::MenuButton:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnDragDone() override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Button* GetReferenceButtonForPopup() override;
  bool IsMenuRunning() const override;
  bool CanShowIconInToolbar() const override;
  void OnPopupShown(bool by_user) override;
  void OnPopupClosed() override;

  // A lock to keep the MenuButton pressed when a menu or popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  // The controller for this toolbar action view.
  ToolbarActionViewController* view_controller_;

  // Delegate that usually represents a container for ToolbarActionView.
  Delegate* delegate_;

  // Used to make sure we only register the command once.
  bool called_register_command_ = false;

  // Set to true by a mouse press that will hide a popup due to deactivation.
  // In this case, the next click should not trigger an action, so the popup
  // doesn't hide on mouse press and immediately reshow on mouse release.
  bool suppress_next_release_ = false;

  // The cached value of whether or not the action wants to run on the current
  // tab.
  bool wants_to_run_ = false;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  base::WeakPtrFactory<ToolbarActionView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
