// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/toolbar/chrome_labs_model.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "ui/views/view_observer.h"

class Browser;
class ChromeLabsButton;
class ChromeLabsBubbleView;
class ChromeLabsViewController;

class ChromeLabsCoordinator : public views::ViewObserver {
 public:
  enum class ShowUserType {
    // The default user type that accounts for most users.
    kDefaultUserType,
    // Indicates that the user is the device owner on ChromeOS. The
    // OwnerFlagsStorage will be used in this case.
    kChromeOsOwnerUserType,
  };

  ChromeLabsCoordinator(ChromeLabsButton* anchor_view,
                        Browser* browser,
                        const ChromeLabsModel* model);
  ~ChromeLabsCoordinator() override;

  bool BubbleExists();

  void Show(ShowUserType user_type = ShowUserType::kDefaultUserType);

  void Hide();

  // Toggles the visibility of the bubble.
  void ShowOrHide();

  ChromeLabsBubbleView* GetChromeLabsBubbleViewForTesting() {
    return chrome_labs_bubble_view_;
  }

  flags_ui::FlagsState* GetFlagsStateForTesting() { return flags_state_; }

  ChromeLabsViewController* GetViewControllerForTesting() {
    return controller_.get();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetShouldCircumventDeviceCheckForTesting(bool should_circumvent) {
    should_circumvent_device_check_for_testing_ = should_circumvent;
  }
#endif

 private:
  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  raw_ptr<ChromeLabsButton, DanglingUntriaged> anchor_view_;
  raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<const ChromeLabsModel, AcrossTasksDanglingUntriaged>
      chrome_labs_model_;
  raw_ptr<ChromeLabsBubbleView, DanglingUntriaged> chrome_labs_bubble_view_ =
      nullptr;

  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
  raw_ptr<flags_ui::FlagsState, DanglingUntriaged> flags_state_;
  std::unique_ptr<ChromeLabsViewController> controller_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_waiting_to_show_ = false;
  bool should_circumvent_device_check_for_testing_ = false;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_COORDINATOR_H_
