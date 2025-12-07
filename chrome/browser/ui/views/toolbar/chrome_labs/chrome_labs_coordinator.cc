// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_view_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

ChromeLabsCoordinator::ChromeLabsCoordinator(Browser* browser)
    : browser_(browser) {
  pinned_actions_observation_.Observe(
      PinnedToolbarActionsModel::Get(browser->profile()));

  chrome_labs_action_item_ = actions::ActionManager::Get().FindAction(
      kActionShowChromeLabs, browser->browser_actions()->root_action_item());
  CHECK(chrome_labs_action_item_);

  MaybeInstallDotIndicator();
}

ChromeLabsCoordinator::~ChromeLabsCoordinator() {
  TearDown();
}

void ChromeLabsCoordinator::TearDown() {
  if (BubbleExists()) {
    GetChromeLabsBubbleView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    chrome_labs_bubble_view_tracker_.SetView(nullptr);
  }
  pinned_actions_observation_.Reset();
  chrome_labs_action_item_ = nullptr;
}

bool ChromeLabsCoordinator::BubbleExists() {
  return chrome_labs_bubble_view_tracker_.view() != nullptr;
}

void ChromeLabsCoordinator::Show(ShowUserType user_type) {
#if BUILDFLAG(IS_CHROMEOS)
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile = browser_->profile()->GetOriginalProfile();
  if (user_type == ShowUserType::kChromeOsOwnerUserType) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    flags_storage_ = std::make_unique<ash::about_flags::OwnerFlagsStorage>(
        original_profile->GetPrefs(), service);
  } else {
    flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        original_profile->GetPrefs());
  }
#else
  flags_storage_ = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
      g_browser_process->local_state());
#endif  // BUILDFLAG(IS_CHROMEOS)

  flags_state_ = about_flags::GetCurrentFlagsState();

  browser_->GetFeatures()
      .pinned_toolbar_actions_controller()
      ->ShowActionEphemerallyInToolbar(kActionShowChromeLabs, true);

  auto chrome_labs_bubble_view =
      std::make_unique<ChromeLabsBubbleView>(GetChromeLabsButton(), browser_);
  chrome_labs_bubble_view_tracker_.SetView(chrome_labs_bubble_view.get());
  chrome_labs_action_item_->SetIsShowingBubble(true);
  chrome_labs_bubble_view_tracker_.SetIsDeletingCallback(
      base::BindOnce(&ChromeLabsCoordinator::OnChromeLabsBubbleClosing,
                     base::Unretained(this)));

  controller_ = std::make_unique<ChromeLabsViewController>(
      chrome_labs_bubble_view.get(), browser_, flags_state_,
      flags_storage_.get());

  // ChromeLabsButton should not appear in the toolbar if there are no
  // experiments to show. Therefore ChromeLabsBubble should not be created.
  DCHECK_GE(chrome_labs_bubble_view->GetNumLabItems(), 1u);

  views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(chrome_labs_bubble_view));
  widget->Show();

  // Hide dot indicator once bubble has been shown.
  views::DotIndicator* dot_indicator = GetDotIndicator();
  if (dot_indicator) {
    dot_indicator->SetVisible(false);
  }
}

void ChromeLabsCoordinator::Hide() {
  if (BubbleExists()) {
    GetChromeLabsBubbleView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void ChromeLabsCoordinator::ShowOrHide() {
#if BUILDFLAG(IS_CHROMEOS)
  if (is_waiting_to_show_) {
    return;
  }
#endif
  if (BubbleExists()) {
    Hide();
    return;
  }
  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
  // Asynchronously check if the user is the owner and show the Chrome Labs
  // bubble only after we have this information.
#if BUILDFLAG(IS_CHROMEOS)
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile = browser_->profile()->GetOriginalProfile();
  if ((base::SysInfo::IsRunningOnChromeOS() ||
       should_circumvent_device_check_for_testing_) &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    is_waiting_to_show_ = true;
    service->IsOwnerAsync(base::BindOnce(
        [](base::WeakPtr<BrowserView> browser_view,
           ChromeLabsCoordinator* coordinator, bool is_owner) {
          // BrowserView may have been destroyed before async function returns
          if (!browser_view) {
            return;
          }
          is_owner
              ? coordinator->Show(
                    ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType)
              : coordinator->Show();
          coordinator->is_waiting_to_show_ = false;
        },
        BrowserView::GetBrowserViewForBrowser(browser_)->GetAsWeakPtr(), this));
    return;
  }
#endif
  Show();
}

PinnedActionToolbarButton* ChromeLabsCoordinator::GetChromeLabsButton() {
  PinnedToolbarActionsController* pinned_toolbar_actions_controller =
      browser_->GetFeatures().pinned_toolbar_actions_controller();
  return pinned_toolbar_actions_controller
             ? pinned_toolbar_actions_controller->GetButtonFor(
                   kActionShowChromeLabs)
             : nullptr;
}

ChromeLabsBubbleView* ChromeLabsCoordinator::GetChromeLabsBubbleView() {
  return BubbleExists() ? static_cast<ChromeLabsBubbleView*>(
                              chrome_labs_bubble_view_tracker_.view())
                        : nullptr;
}

void ChromeLabsCoordinator::OnChromeLabsBubbleClosing() {
  chrome_labs_action_item_->SetIsShowingBubble(false);

  browser_->GetFeatures()
      .pinned_toolbar_actions_controller()
      ->ShowActionEphemerallyInToolbar(kActionShowChromeLabs, false);
}

void ChromeLabsCoordinator::MaybeInstallDotIndicator() {
  PinnedActionToolbarButton* button = GetChromeLabsButton();
  if (!button) {
    return;
  }
  views::View* anchor = button->GetImageContainerView();
  // Check to ensure there isn't already a dot indicator on the button.
  if (GetDotIndicator()) {
    return;
  }
  views::DotIndicator* dot_indicator = views::DotIndicator::Install(anchor);
  dot_indicator->SetVisible(
      AreNewChromeLabsExperimentsAvailable(browser_->profile()));

  gfx::Rect dot_rect(8, 8);
  dot_rect.set_origin(gfx::Point(anchor->GetPreferredSize().width(),
                                 anchor->GetPreferredSize().height()) -
                      dot_rect.bottom_right().OffsetFromOrigin());
  dot_indicator->SetBoundsRect(dot_rect);
}

views::DotIndicator* ChromeLabsCoordinator::GetDotIndicator() {
  PinnedActionToolbarButton* button = GetChromeLabsButton();
  if (!button) {
    return nullptr;
  }
  views::View* anchor = button->GetImageContainerView();
  // Check to ensure there isn't already a dot indicator on the button.
  for (auto& child : anchor->children()) {
    if (views::IsViewClass<views::DotIndicator>(child)) {
      return views::AsViewClass<views::DotIndicator>(child);
    }
  }
  return nullptr;
}

void ChromeLabsCoordinator::OnActionsChanged() {
  MaybeInstallDotIndicator();
}
