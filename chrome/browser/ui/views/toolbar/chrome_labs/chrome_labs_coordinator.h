// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "components/webui/flags/flags_state.h"
#include "components/webui/flags/flags_storage.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

class Browser;
class ChromeLabsBubbleView;
class ChromeLabsViewController;
class PinnedActionToolbarButton;

class ChromeLabsCoordinator : public PinnedToolbarActionsModel::Observer {
 public:
  enum class ShowUserType {
    // The default user type that accounts for most users.
    kDefaultUserType,
    // Indicates that the user is the device owner on ChromeOS. The
    // OwnerFlagsStorage will be used in this case.
    kChromeOsOwnerUserType,
  };

  explicit ChromeLabsCoordinator(Browser* browser);
  ~ChromeLabsCoordinator() override;

  void TearDown();

  bool BubbleExists();

  void Show(ShowUserType user_type = ShowUserType::kDefaultUserType);

  void Hide();

  // Toggles the visibility of the bubble.
  void ShowOrHide();

  PinnedActionToolbarButton* GetChromeLabsButton();

  ChromeLabsBubbleView* GetChromeLabsBubbleView();

  void OnChromeLabsBubbleClosing();

  void MaybeInstallDotIndicator();

  views::DotIndicator* GetDotIndicator();

  // PinnedToolbarActionsModel::Observer:
  void OnActionsChanged() override;

  flags_ui::FlagsState* GetFlagsStateForTesting() { return flags_state_; }

  ChromeLabsViewController* GetViewControllerForTesting() {
    return controller_.get();
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetShouldCircumventDeviceCheckForTesting(bool should_circumvent) {
    should_circumvent_device_check_for_testing_ = should_circumvent;
  }
#endif

 private:
  raw_ptr<Browser, DanglingUntriaged> browser_;
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
  raw_ptr<flags_ui::FlagsState, DanglingUntriaged> flags_state_;
  std::unique_ptr<ChromeLabsViewController> controller_;
  views::ViewTracker chrome_labs_bubble_view_tracker_;
  raw_ptr<actions::ActionItem> chrome_labs_action_item_;
  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_actions_observation_{this};
#if BUILDFLAG(IS_CHROMEOS)
  bool is_waiting_to_show_ = false;
  bool should_circumvent_device_check_for_testing_ = false;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_COORDINATOR_H_
