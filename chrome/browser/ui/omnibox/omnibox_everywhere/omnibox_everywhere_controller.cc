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

  if (g_browser_process && g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
    for (auto* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      OnProfileAdded(profile);
    }
  }

  if (GlobalBrowserCollection::GetInstance()) {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }
}

OmniboxEverywhereController::~OmniboxEverywhereController() {
  browser_collection_observation_.Reset();
  if (listener_) {
    listener_->UnregisterAccelerators(this);
  }
}

void OmniboxEverywhereController::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (browser) {
    SetTargetProfile(browser->GetProfile());
  }
}

void OmniboxEverywhereController::SetTargetProfile(Profile* profile) {
  if (profile) {
    if (profile->IsOffTheRecord()) {
      profile = profile->GetOriginalProfile();
    }
    if (!IsProfileEligible(profile)) {
      return;
    }
  }

  if (target_profile_ == profile) {
    return;
  }

  target_profile_ = profile;
  background_mode_manager_->SetProfile(target_profile_);

  if (target_profile_) {
    PersistTargetProfilePath(target_profile_->GetPath());
  }
}

void OmniboxEverywhereController::OnProfileAdded(Profile* profile) {
  if (target_profile_ || !IsProfileEligible(profile)) {
    return;
  }

  // TODO(crbug.com/532190282): Handle locked profiles (e.g. show profile picker
  // if locked) and deleted (or no longer eligible) profiles (e.g. clear the
  // persisted target profile pref).
  const base::FilePath persisted_path = GetPersistedTargetProfilePath();
  if (persisted_path.empty() || profile->GetPath() == persisted_path) {
    SetTargetProfile(profile);
  }
}

void OmniboxEverywhereController::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

bool OmniboxEverywhereController::IsProfileEligible(Profile* profile) const {
  return profile && !profile->IsOffTheRecord() &&
         omnibox::IsOmniboxEverywhereEnabled(profile) &&
         OmniboxEverywhereServiceFactory::GetForProfile(profile);
}

base::FilePath OmniboxEverywhereController::GetPersistedTargetProfilePath()
    const {
  if (g_browser_process && g_browser_process->local_state()) {
    return g_browser_process->local_state()->GetFilePath(
        prefs::kLastTargetProfileDir);
  }
  return base::FilePath();
}

void OmniboxEverywhereController::PersistTargetProfilePath(
    const base::FilePath& path) {
  if (!g_browser_process || !g_browser_process->local_state()) {
    return;
  }

  if (path.empty()) {
    g_browser_process->local_state()->ClearPref(prefs::kLastTargetProfileDir);
  } else {
    g_browser_process->local_state()->SetFilePath(prefs::kLastTargetProfileDir,
                                                  path);
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
    listener_->RegisterAccelerator(GetHotkey(), this);
  }
}

void OmniboxEverywhereController::OnInvoke(InvocationSource source,
                                           Profile* profile,
                                           gfx::NativeWindow context) {
  if (!IsProfileEligible(profile)) {
    return;
  }

  SetTargetProfile(profile);
  switch (source) {
    case InvocationSource::kGlobalHotkey:
    case InvocationSource::kStatusTrayIcon:
      if (ui_manager_->IsVisible() && ui_manager_->profile() == profile) {
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
  Profile* target_profile = GetTargetProfile();
  if (target_profile) {
    OnInvoke(source, target_profile, gfx::NativeWindow());
  }
}

void OmniboxEverywhereController::OnStatusIconClicked() {
  InvokeForActiveBrowserProfile(InvocationSource::kStatusTrayIcon);
}

void OmniboxEverywhereController::OnProfilePicked(Profile* new_profile) {
  if (!new_profile) {
    return;
  }
  SetTargetProfile(new_profile);
  OnInvoke(InvocationSource::kProfilePicker, new_profile);
}

void OmniboxEverywhereController::ShutdownForProfile(Profile* profile) {
  if (profile == target_profile_) {
    SetTargetProfile(nullptr);
  }
  if (profile == ui_manager_->profile()) {
    ui_manager_->Shutdown();
  }
}

Profile* OmniboxEverywhereController::GetTargetProfile() const {
  return target_profile_;
}

void OmniboxEverywhereController::ExitBackgroundMode() {
  if (background_mode_manager_) {
    background_mode_manager_->ExitBackgroundMode();
  }
}

void OmniboxEverywhereController::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  InvokeForActiveBrowserProfile(InvocationSource::kGlobalHotkey);
}

void OmniboxEverywhereController::ExecuteCommand(
    const std::string& accelerator_group_id,
    const std::string& command_id) {}

}  // namespace omnibox_everywhere
