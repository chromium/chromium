// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"

#include "base/timer/elapsed_timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

ChromeLabsButton::ChromeLabsButton(BrowserView* browser_view,
                                   const ChromeLabsBubbleViewModel* model)
    : ToolbarButton(base::BindRepeating(&ChromeLabsButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_view_(browser_view),
      model_(model) {
  SetVectorIcons(kChromeLabsIcon, kChromeLabsTouchIcon);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CHROMELABS_BUTTON));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_CHROMELABS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kPopUpButton);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
}

ChromeLabsButton::~ChromeLabsButton() {
  // Make sure the bubble is destroyed if the button is being destroyed.
  ChromeLabsBubbleView::Hide();
}

void ChromeLabsButton::ButtonPressed() {
  // On Chrome OS if we are still waiting for IsOwnerAsync to return abort
  // button clicks.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_waiting_to_show) {
    return;
  }
#endif

  if (ChromeLabsBubbleView::IsShowing()) {
    ChromeLabsBubbleView::Hide();
    return;
  }

  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
  // Asynchronously check if the user is the owner and show the Chrome Labs
  // bubble only after we have this information.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Reset timer.
  ash_owner_check_timer_ = nullptr;
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile =
      browser_view_->browser()->profile()->GetOriginalProfile();
  if ((base::SysInfo::IsRunningOnChromeOS() ||
       should_circumvent_device_check_for_testing_) &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    ash_owner_check_timer_ = std::make_unique<base::ElapsedTimer>();
    is_waiting_to_show = true;
    service->IsOwnerAsync(base::BindOnce(
        [](ChromeLabsButton* button, base::WeakPtr<BrowserView> browser_view,
           const ChromeLabsBubbleViewModel* model, bool is_owner) {
          if (!browser_view)
            return;
          ChromeLabsBubbleView::Show(button, browser_view->browser(), model,
                                     is_owner);
          button->is_waiting_to_show = false;
        },
        this, browser_view_->GetAsWeakPtr(), model_));
    return;
  }
#endif
  ChromeLabsBubbleView::Show(this, browser_view_->browser(), model_,
                             /*user_is_chromeos_owner=*/false);
}

// static
bool ChromeLabsButton::ShouldShowButton(const ChromeLabsBubbleViewModel* model,
                                        Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode) ||
      !ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }
#endif
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();

  return std::any_of(all_labs.begin(), all_labs.end(),
                     [&profile](const LabInfo& lab) {
                       return IsChromeLabsFeatureValid(lab, profile);
                     });
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
