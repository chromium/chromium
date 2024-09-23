// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_view_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

ChromeLabsCoordinator::ChromeLabsCoordinator(Browser* browser)
    : ChromeLabsCoordinator(browser, nullptr) {}

ChromeLabsCoordinator::ChromeLabsCoordinator(
    Browser* browser,
    std::unique_ptr<ChromeLabsModel> model)
    : browser_(browser), model_(std::move(model)) {
  if (!model) {
    model_ = std::make_unique<ChromeLabsModel>();
  }
}

ChromeLabsCoordinator::~ChromeLabsCoordinator() {
  if (BubbleExists()) {
    GetChromeLabsBubbleView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    chrome_labs_bubble_view_tracker_.SetView(nullptr);
  }
}

bool ChromeLabsCoordinator::BubbleExists() {
  return chrome_labs_bubble_view_tracker_.view() != nullptr;
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

  if (features::IsToolbarPinningEnabled()) {
    BrowserView::GetBrowserViewForBrowser(browser_)
        ->toolbar()
        ->pinned_toolbar_actions_container()
        ->ShowActionEphemerallyInToolbar(kActionShowChromeLabs, true);
  }

  auto chrome_labs_bubble_view =
      std::make_unique<ChromeLabsBubbleView>(GetChromeLabsButton(), browser_);
  chrome_labs_bubble_view_tracker_.SetView(chrome_labs_bubble_view.get());

  controller_ = std::make_unique<ChromeLabsViewController>(
      model_.get(), chrome_labs_bubble_view.get(), browser_, flags_state_,
      flags_storage_.get());

  // ChromeLabsButton should not appear in the toolbar if there are no
  // experiments to show. Therefore ChromeLabsBubble should not be created.
  DCHECK_GE(chrome_labs_bubble_view->GetNumLabItems(), 1u);

  views::Widget* const widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(chrome_labs_bubble_view));
  widget->Show();

  // TODO(b/354207075): Figure out how to get the dot indicator to show on the
  // pinned toolbar button.
  // Hide dot indicator once bubble has been shown.
  if (!features::IsToolbarPinningEnabled()) {
    static_cast<ChromeLabsButton*>(GetChromeLabsButton())->HideDotIndicator();
  }
}

void ChromeLabsCoordinator::Hide() {
  if (BubbleExists()) {
    GetChromeLabsBubbleView()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    // Closing the widget will eventually result in the view tracked being set
    // to nullptr, but we also set it to nullptr here since we know the widget
    // will now be destroyed and we shouldn't be accessing the
    // ChromeLabsBubbleView anymore.
    chrome_labs_bubble_view_tracker_.SetView(nullptr);
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
    return;
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

views::Button* ChromeLabsCoordinator::GetChromeLabsButton() {
  views::Button* button;
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser_)->toolbar();

  if (features::IsToolbarPinningEnabled()) {
    button = toolbar->pinned_toolbar_actions_container()->GetButtonFor(
        kActionShowChromeLabs);
  } else {
    button = toolbar->chrome_labs_button();
  }

  return button;
}

ChromeLabsBubbleView* ChromeLabsCoordinator::GetChromeLabsBubbleView() {
  return BubbleExists() ? static_cast<ChromeLabsBubbleView*>(
                              chrome_labs_bubble_view_tracker_.view())
                        : nullptr;
}
