// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

// A splash screen for borealis, displays when borealis is clicked and closed
// when the first borealis window shows.
namespace borealis {

void ShowBorealisSplashScreenView(Profile* profile);
void CloseBorealisSplashScreenView();

class BorealisSplashScreenView
    : public views::DialogDelegateView,
      public borealis::BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit BorealisSplashScreenView(Profile* profile);
  ~BorealisSplashScreenView() override;

  static void Show(Profile* profile);
  static BorealisSplashScreenView* GetActiveViewForTesting();

  // views::DialogDelegateView:
  void OnThemeChanged() override;
  bool ShouldShowWindowTitle() const override;

  // Overrides for AppWindowLifetimeObserver
  void OnWindowManagerDeleted(
      borealis::BorealisWindowManager* window_manager) override;
  // Close this view when borealis window launches
  void OnSessionStarted() override;
  void OnGetRootPath(const std::string& path);

 private:
  void UpdateColors();

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<views::Label> title_label_;
  raw_ptr<views::Label> starting_label_;
  base::TimeTicks start_tick_;
  base::WeakPtrFactory<BorealisSplashScreenView> weak_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_SPLASH_SCREEN_VIEW_H_
