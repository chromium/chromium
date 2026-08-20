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
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

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
    hotkey_string_pref_member_.Init(
        prefs::kOmniboxEverywhereHotkey, g_browser_process->local_state(),
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

bool OmniboxEverywhereController::InvokeForProfilePath(
    const base::FilePath& profile_path,
    InvocationSource source,
    gfx::NativeWindow context) {
  if (!g_browser_process || !g_browser_process->profile_manager()) {
    return false;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* persisted_profile = profile_manager->GetProfileByPath(profile_path);

  // If the profile persisted in local pref is already loaded, check
  // eligibility.
  if (persisted_profile) {
    if (IsProfileEligible(persisted_profile)) {
      OnInvoke(source, persisted_profile, context);
      return true;
    }
    return false;
  }

  // Check whether `profile_path` points to a valid Profile on disk.
  const bool is_valid_profile_path =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path) != nullptr;
  if (!is_valid_profile_path) {
    return false;
  }

  // Hold a browser keep-alive while we asynchronously load the persisted
  // profile and attempt to invoke the UI.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE_STARTUP,
      KeepAliveRestartOption::DISABLED);

  views::Widget* widget =
      context ? views::Widget::GetWidgetForNativeWindow(context) : nullptr;
  base::WeakPtr<views::Widget> context_widget =
      widget ? widget->GetWeakPtr() : nullptr;

  profile_manager->CreateProfileAsync(
      profile_path,
      base::BindOnce(
          [](base::WeakPtr<OmniboxEverywhereController> controller,
             std::unique_ptr<ScopedKeepAlive> /*keep_alive*/,
             base::WeakPtr<views::Widget> context_widget,
             InvocationSource source, Profile* profile) {
            if (controller && controller->IsProfileEligible(profile)) {
              gfx::NativeWindow safe_context =
                  context_widget ? context_widget->GetNativeWindow()
                                 : gfx::NativeWindow();
              controller->OnInvoke(source, profile, safe_context);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(keep_alive), context_widget,
          source));
  return true;
}

bool OmniboxEverywhereController::InvokeForStartup(InvocationSource source,
                                                   Profile* fallback_profile,
                                                   gfx::NativeWindow context) {
  if (auto* target_profile = GetTargetProfile()) {
    OnInvoke(source, target_profile, context);
    return true;
  }

  base::FilePath persisted_path =
      g_browser_process && g_browser_process->local_state()
          ? g_browser_process->local_state()->GetFilePath(
                prefs::kLastTargetProfileDir)
          : base::FilePath();
  if (!persisted_path.empty() &&
      InvokeForProfilePath(persisted_path, source, context)) {
    return true;
  }

  // Cannot invoke with the persisted profile so try invoking with
  // `fallback_profile`.
  if (IsProfileEligible(fallback_profile)) {
    OnInvoke(source, fallback_profile, context);
    return true;
  }

  return false;
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

  // When the user enters a new shortcut in the settings WebUI,
  // <cr-shortcut-input> suspends global shortcut handling via
  // SetShortcutHandlingSuspended(true) so that typing key combinations in
  // the input field does not trigger global listeners or native system
  // actions.
  //
  // However, `GlobalAcceleratorListener::RegisterAccelerator()` rejects new
  // registrations while suspended (it returns false if
  // `IsShortcutHandlingSuspended()` is true). To ensure the hotkey is
  // cleanly unregistered and re-registered while the user is still
  // interacting with the settings UI, we temporarily restore the suspension
  // state to false for the registration updates, and then immediately reinstate
  // the original suspension state until the user finishes key capture.
  const bool shortcut_handling_suspended =
      listener_->IsShortcutHandlingSuspended();
  listener_->SetShortcutHandlingSuspended(false);

  listener_->UnregisterAccelerators(this);

  const bool is_enabled =
      hotkey_pref_member_.prefs() && hotkey_pref_member_.GetValue();
  if (is_enabled) {
    PrefService* local_state =
        g_browser_process ? g_browser_process->local_state() : nullptr;
    const ui::Accelerator hotkey =
        prefs::GetOmniboxEverywhereHotkey(local_state);
    if (!hotkey.IsEmpty()) {
      listener_->RegisterAccelerator(hotkey, this);
    }
  }

  listener_->SetShortcutHandlingSuspended(shortcut_handling_suspended);
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
    case InvocationSource::kCommandLine:
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
  if (target_profile_ && IsProfileEligible(target_profile_)) {
    return target_profile_;
  }
  return nullptr;
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
