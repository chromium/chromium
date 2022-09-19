// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/dot_indicator.h"

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
  new_experiments_indicator_ = views::DotIndicator::Install(image());
  UpdateDotIndicator();

  chrome_labs_coordinator_ = std::make_unique<ChromeLabsCoordinator>(
      this, browser_view_->browser(), model_);
}

ChromeLabsButton::~ChromeLabsButton() = default;

void ChromeLabsButton::Layout() {
  ToolbarButton::Layout();
  gfx::Rect dot_rect(8, 8);
  if (ui::TouchUiController::Get()->touch_ui()) {
    dot_rect = ScaleToEnclosingRect(
        dot_rect, float{kDefaultTouchableIconSize} / kDefaultIconSize);
  }
  dot_rect.set_origin(image()->GetImageBounds().bottom_right() -
                      dot_rect.bottom_right().OffsetFromOrigin());
  new_experiments_indicator_->SetBoundsRect(dot_rect);
}

void ChromeLabsButton::HideDotIndicator() {
  new_experiments_indicator_->Hide();
}

void ChromeLabsButton::ButtonPressed() {
  // On Chrome OS if we are still waiting for IsOwnerAsync to return abort
  // button clicks.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_waiting_to_show) {
    return;
  }
#endif

  if (chrome_labs_coordinator_->BubbleExists()) {
    chrome_labs_coordinator_->Hide();
    return;
  }

  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
  // Asynchronously check if the user is the owner and show the Chrome Labs
  // bubble only after we have this information.
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    is_waiting_to_show = true;
    service->IsOwnerAsync(base::BindOnce(
        [](ChromeLabsButton* button, base::WeakPtr<BrowserView> browser_view,
           ChromeLabsCoordinator* coordinator, bool is_owner) {
          // BrowserView may have been destroyed before async function returns
          if (!browser_view)
            return;
          is_owner
              ? coordinator->Show(
                    ChromeLabsCoordinator::ShowUserType::kChromeOsOwnerUserType)
              : coordinator->Show();
          button->is_waiting_to_show = false;
        },
        this, browser_view_->GetAsWeakPtr(), chrome_labs_coordinator_.get()));
    return;
  }
#endif
  chrome_labs_coordinator_->Show();
}

void ChromeLabsButton::UpdateDotIndicator() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedDictPrefUpdate update(
      browser_view_->browser()->profile()->GetPrefs(),
      chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  base::Value::Dict& new_badge_prefs = update.Get();

  std::vector<std::string> lab_internal_names;
  const std::vector<LabInfo>& all_labs = model_->GetLabInfo();

  bool should_show_dot_indicator = base::ranges::any_of(
      all_labs.begin(), all_labs.end(), [&new_badge_prefs](const LabInfo& lab) {
        absl::optional<int> new_badge_pref_value =
            new_badge_prefs.FindInt(lab.internal_name);
        // Show the dot indicator if new experiments have not been seen yet.
        return new_badge_pref_value == chrome_labs_prefs::kChromeLabsNewExperimentPrefValue;
      });

  if (should_show_dot_indicator)
    new_experiments_indicator_->Show();
  else
    new_experiments_indicator_->Hide();
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

  return base::ranges::any_of(model->GetLabInfo(),
                              [&profile](const LabInfo& lab) {
                                return IsChromeLabsFeatureValid(lab, profile);
                              });
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
