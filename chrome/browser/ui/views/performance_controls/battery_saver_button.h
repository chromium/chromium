// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserView;

namespace views {
class BubbleDialogModelHost;
}  // namespace views

// This class represents the view of the button in the toolbar that provides
// users with visual indication of when the battery saver mode is active.
// The mode is active when either the battery is low or when the device is
// unplugged depending on the battery saver mode settings.
class BatterySaverButton : public ToolbarButton,
                           public BatterySaverBubbleObserver,
                           public BatterySaverButtonControllerDelegate {
  METADATA_HEADER(BatterySaverButton, ToolbarButton)

 public:
  explicit BatterySaverButton(BrowserView* browser_view);
  ~BatterySaverButton() override;

  BatterySaverButton(const BatterySaverButton&) = delete;
  BatterySaverButton& operator=(const BatterySaverButton&) = delete;

  views::BubbleDialogModelHost* GetBubble() const { return bubble_; }

  bool IsBubbleShowing() const;

  // BatterySaverButtonControllerDelegate:
  void Show() override;
  void Hide() override;

  // BatterySaverBubbleObserver:
  void OnBubbleShown() override {}
  void OnBubbleHidden() override;

  // ToolbarButton:
  bool ShouldShowInkdropAfterIphInteraction() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // Handles press events from the button
  void OnClicked();

  void MaybeShowFeaturePromo();
  void CloseFeaturePromo(bool engaged);

 private:
  const raw_ptr<BrowserView> browser_view_;
  BatterySaverButtonController controller_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
  bool pending_promo_ = false;
  base::WeakPtrFactory<BatterySaverButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_BATTERY_SAVER_BUTTON_H_
