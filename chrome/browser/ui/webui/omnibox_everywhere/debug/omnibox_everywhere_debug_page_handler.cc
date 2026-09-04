// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug_page_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_page_handler_impl.h"
#include "components/prefs/pref_service.h"

namespace omnibox_everywhere_debug {

OmniboxEverywhereDebugPageHandler::OmniboxEverywhereDebugPageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      user_education_internals_page_handler_(
          std::make_unique<UserEducationInternalsPageHandlerImpl>(
              web_ui,
              profile,
              mojo::NullReceiver())) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    pref_change_registrar_.Init(local_state);
    pref_change_registrar_.Add(
        omnibox_everywhere::prefs::kOmniboxEverywhereBackgroundMode,
        base::BindRepeating(&OmniboxEverywhereDebugPageHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        omnibox_everywhere::prefs::kOmniboxEverywhereLaunchOnStartup,
        base::BindRepeating(&OmniboxEverywhereDebugPageHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        omnibox_everywhere::prefs::kHotkeyEnabled,
        base::BindRepeating(&OmniboxEverywhereDebugPageHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel,
        base::BindRepeating(&OmniboxEverywhereDebugPageHandler::OnPrefChanged,
                            base::Unretained(this)));
  }
}

OmniboxEverywhereDebugPageHandler::~OmniboxEverywhereDebugPageHandler() =
    default;

void OmniboxEverywhereDebugPageHandler::SetBackgroundModeEnabled(bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_state->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereBackgroundMode, enabled);
  }
}

void OmniboxEverywhereDebugPageHandler::GetBackgroundModeEnabled(
    GetBackgroundModeEnabledCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  bool enabled =
      local_state
          ? local_state->GetBoolean(
                omnibox_everywhere::prefs::kOmniboxEverywhereBackgroundMode)
          : false;
  std::move(callback).Run(enabled);
}

void OmniboxEverywhereDebugPageHandler::SetLaunchOnStartupEnabled(
    bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_state->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereLaunchOnStartup, enabled);
  }
}

void OmniboxEverywhereDebugPageHandler::GetLaunchOnStartupEnabled(
    GetLaunchOnStartupEnabledCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  bool enabled =
      local_state
          ? local_state->GetBoolean(
                omnibox_everywhere::prefs::kOmniboxEverywhereLaunchOnStartup)
          : false;
  std::move(callback).Run(enabled);
}

void OmniboxEverywhereDebugPageHandler::SetHotkeyEnabled(bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_state->SetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled, enabled);
  }
}

void OmniboxEverywhereDebugPageHandler::GetHotkeyEnabled(
    GetHotkeyEnabledCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  bool enabled =
      local_state
          ? local_state->GetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled)
          : true;
  std::move(callback).Run(enabled);
}

void OmniboxEverywhereDebugPageHandler::SetEphemeralModelEnabled(bool enabled) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_state->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, enabled);
  }
}

void OmniboxEverywhereDebugPageHandler::GetEphemeralModelEnabled(
    GetEphemeralModelEnabledCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  bool enabled =
      local_state
          ? local_state->GetBoolean(
                omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel)
          : false;
  std::move(callback).Run(enabled);
}

void OmniboxEverywhereDebugPageHandler::InvokeOmniboxEverywhere(
    mojom::InvocationSource source) {
  if (g_browser_process && g_browser_process->GetFeatures()) {
    auto* controller =
        g_browser_process->GetFeatures()->omnibox_everywhere_controller();
    if (controller) {
      omnibox_everywhere::InvocationSource cpp_source =
          omnibox_everywhere::InvocationSource::kGlobalHotkey;
      switch (source) {
        case mojom::InvocationSource::kGlobalHotkey:
          cpp_source = omnibox_everywhere::InvocationSource::kGlobalHotkey;
          break;
        case mojom::InvocationSource::kProfilePicker:
          cpp_source = omnibox_everywhere::InvocationSource::kProfilePicker;
          break;
      }
      controller->OnInvoke(cpp_source, profile_);
    }
  }
}

void OmniboxEverywhereDebugPageHandler::ShowLensIph() {
  if (user_education_internals_page_handler_) {
    user_education_internals_page_handler_->ShowFeaturePromo(
        "IPH_OmniboxEverywhereLensPromo", base::DoNothing());
  }
}

void OmniboxEverywhereDebugPageHandler::CreateStartMenuShortcut(
    CreateStartMenuShortcutCallback callback) {
  if (g_browser_process && g_browser_process->GetFeatures()) {
    auto* controller =
        g_browser_process->GetFeatures()->omnibox_everywhere_controller();
    if (controller) {
      controller->CreateStartMenuShortcut(std::move(callback));
      return;
    }
  }
  std::move(callback).Run(false);
}

void OmniboxEverywhereDebugPageHandler::PinToTaskbar(
    PinToTaskbarCallback callback) {
  if (g_browser_process && g_browser_process->GetFeatures()) {
    auto* controller =
        g_browser_process->GetFeatures()->omnibox_everywhere_controller();
    if (controller) {
      controller->OfferPinToTaskbar(std::move(callback));
      return;
    }
  }
  std::move(callback).Run(false);
}

void OmniboxEverywhereDebugPageHandler::OnPrefChanged(
    const std::string& pref_name) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state || !page_.is_bound()) {
    return;
  }
  if (pref_name ==
      omnibox_everywhere::prefs::kOmniboxEverywhereBackgroundMode) {
    page_->OnBackgroundModeChanged(local_state->GetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereBackgroundMode));
  } else if (pref_name == omnibox_everywhere::prefs::kHotkeyEnabled) {
    page_->OnHotkeyChanged(
        local_state->GetBoolean(omnibox_everywhere::prefs::kHotkeyEnabled));
  } else if (pref_name ==
             omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel) {
    page_->OnEphemeralModelChanged(local_state->GetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel));
  } else if (pref_name ==
             omnibox_everywhere::prefs::kOmniboxEverywhereLaunchOnStartup) {
    page_->OnLaunchOnStartupChanged(local_state->GetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereLaunchOnStartup));
  }
}

}  // namespace omnibox_everywhere_debug
