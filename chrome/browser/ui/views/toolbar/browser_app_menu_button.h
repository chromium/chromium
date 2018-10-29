// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/views/controls/animated_icon_view.h"
#include "ui/views/view.h"

namespace views {
class LabelButtonBorder;
}  // namespace views

class ToolbarView;

// The app menu button in the main browser window (as opposed to hosted app
// windows, which is implemented in HostedAppMenuButton).
class BrowserAppMenuButton : public AppMenuButton,
                             public TabStripModelObserver,
                             public ui::MaterialDesignControllerObserver {
 public:
  explicit BrowserAppMenuButton(ToolbarView* toolbar_view);
  ~BrowserAppMenuButton() override;

  void SetSeverity(AppMenuIconController::IconType type,
                   AppMenuIconController::Severity severity,
                   bool animate);

  AppMenuIconController::Severity severity() { return severity_; }

  // Shows the app menu. |for_drop| indicates whether the menu is opened for a
  // drag-and-drop operation.
  void ShowMenu(bool for_drop);

  // Sets the background to a prominent color if |is_prominent| is true. This is
  // used for an experimental UI for In-Product Help.
  void SetIsProminent(bool is_prominent);

  // views::MenuButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnThemeChanged() override;

  // TabStripObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Updates the presentation according to |severity_| and the theme provider.
  // If |should_animate| is true, the icon should animate.
  void UpdateIcon(bool should_animate);

  // Sets |margin_trailing_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

  // Opens the app menu immediately during a drag-and-drop operation.
  // Used only in testing.
  static bool g_open_app_immediately_for_testing;

 protected:
  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

 private:
  // Animates the icon if possible. The icon will not animate if the severity
  // level is none, |animation_| is nullptr or |should_use_new_icon_| is false.
  // If |should_delay_animation_| and |with_delay| is true, then delay the
  // animation.
  void AnimateIconIfPossible(bool with_delay);

  // views::MenuButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  gfx::Rect GetThemePaintRect() const override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  AppMenuIconController::Severity severity_ =
      AppMenuIconController::Severity::NONE;
  AppMenuIconController::IconType type_ = AppMenuIconController::IconType::NONE;

  // Our owning toolbar view.
  ToolbarView* toolbar_view_;

  // The view that depicts and animates the icon. TODO(estade): rename to
  // |animated_icon_| when |should_use_new_icon_| defaults to true and is
  // removed.
  views::AnimatedIconView* new_icon_ = nullptr;

  // Used to delay the animation. Not used if |should_delay_animation_| is
  // false.
  base::OneShotTimer animation_delay_timer_;

  // True if the app menu should use the new animated icon.
  bool should_use_new_icon_ = false;

  // True if the kAnimatedAppMenuIcon feature's "HasDelay" param is true.
  bool should_delay_animation_ = false;

  // Any trailing margin to be applied. Used when the browser is in
  // a maximized state to extend to the full window width.
  int margin_trailing_ = 0;

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  // Used to spawn weak pointers for delayed tasks to open the overflow menu.
  base::WeakPtrFactory<BrowserAppMenuButton> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserAppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_APP_MENU_BUTTON_H_
