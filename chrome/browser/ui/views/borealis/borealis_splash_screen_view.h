// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_

#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// A splash screen for borealis, displays when borealis is clicked and closed
// when the first borealis window shows.
namespace borealis {
class BorealisSplashScreenView
    : public views::BubbleDialogDelegateView,
      public borealis::BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit BorealisSplashScreenView(Profile* profile);
  ~BorealisSplashScreenView() override;

  static void Show(Profile* profile);
  static BorealisSplashScreenView* GetActiveViewForTesting();

  // Overrides for AppWindowLifetimeObserver
  void OnWindowManagerDeleted(
      borealis::BorealisWindowManager* window_manager) override;
  // Close this view when borealis window launches
  void OnSessionStarted() override;

 private:
  Profile* profile_ = nullptr;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_
