// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/common/channel_info.h"
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
#include "chrome/browser/ash/settings/owner_flags_storage.h"
#endif

ChromeLabsButton::ChromeLabsButton(Browser* browser,
                                   const ChromeLabsBubbleViewModel* model)
    : ToolbarButton(base::BindRepeating(&ChromeLabsButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
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
  if (ChromeLabsBubbleView::IsShowing()) {
    ChromeLabsBubbleView::Hide();
    return;
  }
  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
  // Asynchronously check if the user is the owner and show the Chrome Labs
  // bubble only after we have this information.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile = browser_->profile()->GetOriginalProfile();
  if (base::SysInfo::IsRunningOnChromeOS() &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    service->IsOwnerAsync(
        base::BindOnce(&ChromeLabsBubbleView::Show, this, browser_, model_));
    return;
  }
#endif
  ChromeLabsBubbleView::Show(this, browser_, model_,
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
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    if ((entry && (entry->supported_platforms &
                   flags_ui::FlagsState::GetCurrentPlatform()) != 0) &&
        chrome::GetChannel() <= lab.allowed_channel) {
      return true;
    }
  }
  return false;
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
