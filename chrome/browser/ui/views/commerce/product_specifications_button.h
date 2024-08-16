// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_BUTTON_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"

class TabStripController;

class ProductSpecificationsButton
    : public TabStripControlButton,
      public views::MouseWatcherListener,
      public commerce::ProductSpecificationsEntryPointController::Observer {
  METADATA_HEADER(ProductSpecificationsButton, TabStripControlButton)

 public:
  ProductSpecificationsButton(
      TabStripController* tab_strip_controller,
      TabStripModel* tab_strip_model,
      commerce::ProductSpecificationsEntryPointController*
          entry_point_controller,
      bool before_tab_strip,
      View* locked_expansion_view);
  ProductSpecificationsButton(const ProductSpecificationsButton&) = delete;
  ProductSpecificationsButton& operator=(const ProductSpecificationsButton&) =
      delete;
  ~ProductSpecificationsButton() override;

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // views::AnimationDelegateViews
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  float width_factor_for_testing() { return width_factor_; }
  gfx::SlideAnimation* expansion_animation_for_testing() {
    return &expansion_animation_;
  }

  // commerce::ProductSpecificationsEntryPointController::Observer
  void ShowEntryPointWithTitle(const std::u16string& title) override;
  void HideEntryPoint() override;

 protected:
  // TabStripControlButton:
  int GetCornerRadius() const override;

 private:
  friend class ProductSpecificationsButtonBrowserTest;
  friend class ProductSpecificationsButtonTest;

  // Trigger ProductSpecificationsButton show. Whether it's actually showing
  // depends on if the |locked_expansion_view| is hovered.
  void Show();

  // Trigger ProductSpecificationsButton hide. Whether it's actually hiding
  // depends on if the |locked_expansion_view| is hovered.
  void Hide();

  void ApplyAnimationValue(const gfx::Animation* animation);
  void ExecuteShow();
  void ExecuteHide();

  void OnClicked();
  void OnDismissed();
  void OnTimeout();

  void SetCloseButton(PressedCallback callback);
  void SetLockedExpansionMode(LockedExpansionMode mode);
  void ShowOpacityAnimation();
  void SetOpacity(float opacity);
  void SetWidthFactor(float factor);
  void SetEntryPointControllerForTesting(
      commerce::ProductSpecificationsEntryPointController* controller) {
    entry_point_controller_ = controller;
  }

  base::TimeDelta GetAnimationDuration(base::TimeDelta duration);

  // View where, if the mouse is currently over its bounds, the expansion state
  // will not change. Changes will be staged until after the mouse exits the
  // bounds of this View.
  raw_ptr<View, DanglingUntriaged> locked_expansion_view_;
  const raw_ptr<TabStripModel> tab_strip_model_;
  raw_ptr<commerce::ProductSpecificationsEntryPointController,
          DanglingUntriaged>
      entry_point_controller_ = nullptr;

  // Animations controlling showing and hiding of the button.
  gfx::SlideAnimation expansion_animation_{this};
  gfx::SlideAnimation opacity_animation_{this};

  // Timer for hiding the button after show.
  base::OneShotTimer hide_button_timer_;
  // Timer for initiating the opacity animation during show.
  base::OneShotTimer opacity_animation_delay_timer_;

  // When locked, the container is unable to change its expanded state. Changes
  // will be staged until after this is unlocked.
  LockedExpansionMode locked_expansion_mode_ = LockedExpansionMode::kNone;

  // MouseWatcher is used to lock and unlock the expansion state of this
  // container.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Preferred width multiplier, between 0-1. Used to animate button size.
  float width_factor_ = 0;
  raw_ptr<views::LabelButton> close_button_;

  // Prevents other features from showing tabstrip-modal UI.
  std::unique_ptr<ScopedTabStripModalUI> scoped_tab_strip_modal_ui_;

  // Observer to listen to signals to show, hide or update the button.
  base::ScopedObservation<
      commerce::ProductSpecificationsEntryPointController,
      commerce::ProductSpecificationsEntryPointController::Observer>
      entry_point_controller_observations_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRODUCT_SPECIFICATIONS_BUTTON_H_
