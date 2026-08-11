// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace omnibox_everywhere {

OmniboxEverywhereController::OmniboxEverywhereController(
    OmniboxEverywhereUIManager::ContentsWrapperFactory contents_wrapper_factory,
    ui::GlobalAcceleratorListener* listener)
    : ui_manager_(std::make_unique<OmniboxEverywhereUIManager>(
          std::move(contents_wrapper_factory))),
      background_mode_manager_(
          std::make_unique<OmniboxEverywhereBackgroundModeManager>(
              base::BindRepeating(
                  &OmniboxEverywhereController::OnStatusIconClicked,
                  base::Unretained(this)))),
      listener_(listener ? listener
                         : ui::GlobalAcceleratorListener::GetInstance()) {
  CHECK(base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere));
  if (g_browser_process && g_browser_process->local_state()) {
    hotkey_pref_member_.Init(
        prefs::kHotkeyEnabled, g_browser_process->local_state(),
        base::BindRepeating(
            &OmniboxEverywhereController::UpdateHotkeyRegistration,
            base::Unretained(this)));
  }
  UpdateHotkeyRegistration();
}

OmniboxEverywhereController::~OmniboxEverywhereController() {
  if (listener_) {
    listener_->UnregisterAccelerators(this);
  }
}

void OmniboxEverywhereController::UpdateHotkeyRegistration() {
  // `GlobalAcceleratorListener::GetInstance()` may return null on platforms
  // where global accelerators are not supported or unavailable (e.g. Wayland).
  if (!listener_) {
    return;
  }

  listener_->UnregisterAccelerators(this);

  const bool is_enabled =
      hotkey_pref_member_.prefs() && hotkey_pref_member_.GetValue();
  if (is_enabled) {
    listener_->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_SPACE,
                        ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR),
        this);
  }
}

void OmniboxEverywhereController::OnInvoke(InvocationSource source,
                                           Profile* profile,
                                           gfx::NativeWindow context) {
  if (!omnibox::IsOmniboxEverywhereEnabled(profile)) {
    return;
  }
  switch (source) {
    case InvocationSource::kGlobalHotkey:
    case InvocationSource::kStatusTrayIcon:
      if (IsVisible() && ui_manager_->profile() == profile) {
        Close();
      } else {
        ui_manager_->ShowForProfile(profile, context);
      }
      break;
    case InvocationSource::kProfilePicker:
      ui_manager_->ShowForProfile(profile, context);
      break;
  }
}

void OmniboxEverywhereController::Close() {
  ui_manager_->Close();
}

bool OmniboxEverywhereController::IsVisible() const {
  return ui_manager_->IsVisible();
}

void OmniboxEverywhereController::ShowProfilePicker() {
  Close();

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/527183107): Filter out or disallow selecting profiles where
  // Google is not the default search engine (DSE) in the Profile Picker.
  ProfilePicker::Show(ProfilePicker::Params::ForOmniboxEverywhere(
      base::BindOnce(&OmniboxEverywhereController::OnProfilePicked,
                     weak_factory_.GetWeakPtr())));
#endif
}

void OmniboxEverywhereController::InvokeForActiveBrowserProfile(
    InvocationSource source) {
  BrowserWindowInterface* active_bwi =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  Profile* target_profile = GetTargetProfile();
  if (target_profile) {
    gfx::NativeWindow context = active_bwi && active_bwi->GetWindow()
                                    ? active_bwi->GetWindow()->GetNativeWindow()
                                    : gfx::NativeWindow();
    OnInvoke(source, target_profile, context);
  }
}

void OmniboxEverywhereController::OnStatusIconClicked() {
  InvokeForActiveBrowserProfile(InvocationSource::kStatusTrayIcon);
}

void OmniboxEverywhereController::OnProfilePicked(Profile* new_profile) {
  if (!new_profile) {
    return;
  }
  OnInvoke(InvocationSource::kProfilePicker, new_profile);
}

void OmniboxEverywhereController::ShutdownForProfile(Profile* profile) {
  if (profile == ui_manager_->profile()) {
    ui_manager_->Shutdown();
  }
}

// TODO(crbug.com/527183107): Implement a better profile selection heuristic.
Profile* OmniboxEverywhereController::GetTargetProfile() {
  BrowserWindowInterface* active_bwi =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  Profile* target_profile = active_bwi ? active_bwi->GetProfile() : nullptr;

  // Only use the profile of the last active browser window. If no browser
  // window is active (e.g. on the profile selection screen), return nullptr.
  // Also check that the profile has the required service.
  if (target_profile &&
      !OmniboxEverywhereServiceFactory::GetForProfile(target_profile)) {
    target_profile = nullptr;
  }
  return target_profile;
}

void OmniboxEverywhereController::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  ui_manager_->SetIsNavigating(false);
  InvokeForActiveBrowserProfile(InvocationSource::kGlobalHotkey);
}

void OmniboxEverywhereController::ExecuteCommand(
    const std::string& accelerator_group_id,
    const std::string& command_id) {}

}  // namespace omnibox_everywhere
