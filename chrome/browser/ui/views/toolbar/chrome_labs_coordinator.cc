// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_view_controller.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

ChromeLabsCoordinator::ChromeLabsCoordinator(ChromeLabsButton* anchor_view,
                                             Browser* browser,
                                             const ChromeLabsModel* model)
    : anchor_view_(anchor_view), browser_(browser), chrome_labs_model_(model) {}

ChromeLabsCoordinator::~ChromeLabsCoordinator() {
  if (BubbleExists()) {
    chrome_labs_bubble_view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    chrome_labs_bubble_view_ = nullptr;
  }
}

bool ChromeLabsCoordinator::BubbleExists() {
  return chrome_labs_bubble_view_ != nullptr;
}

void ChromeLabsCoordinator::Show(ShowUserType user_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  flags_state_ = about_flags::GetCurrentFlagsState();

  auto chrome_labs_bubble_view =
      std::make_unique<ChromeLabsBubbleView>(anchor_view_);
  chrome_labs_bubble_view_ = chrome_labs_bubble_view.get();
  chrome_labs_bubble_view_->View::AddObserver(this);

  controller_ = std::make_unique<ChromeLabsViewController>(
      chrome_labs_model_, chrome_labs_bubble_view_, browser_, flags_state_,
      flags_storage_.get());

  // ChromeLabsButton should not appear in the toolbar if there are no
  // experiments to show. Therefore ChromeLabsBubble should not be created.
  DCHECK_GE(chrome_labs_bubble_view_->GetNumLabItems(), 1u);

  views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(chrome_labs_bubble_view));
  widget->Show();

  // Hide dot indicator once bubble has been shown.
  anchor_view_->HideDotIndicator();
}

void ChromeLabsCoordinator::Hide() {
  if (BubbleExists()) {
    chrome_labs_bubble_view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    // Closing the widget will eventually result in chrome_labs_bubble_view_
    // being set to nullptr, but we also set it to nullptr here since we know
    // the widget will now be destroyed and we shouldn't be accessing
    // chrome_labs_bubble_ anymore.
    chrome_labs_bubble_view_ = nullptr;
  }
}

void ChromeLabsCoordinator::ShowOrHide() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_waiting_to_show_) {
    return;
  }
#endif
  if (BubbleExists()) {
    Hide();
  }
  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
  // Asynchronously check if the user is the owner and show the Chrome Labs
  // bubble only after we have this information.
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

void ChromeLabsCoordinator::OnViewIsDeleting(views::View* observed_view) {
  chrome_labs_bubble_view_ = nullptr;
}
