// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/strings/grit/ui_strings.h"

namespace omnibox_everywhere {
namespace prefs {

bool IsEphemeralModelEnabled() {
  if (g_browser_process && g_browser_process->local_state()) {
    return g_browser_process->local_state()->GetBoolean(
        kOmniboxEverywhereEphemeralModel);
  }
#if BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHotkeyEnabled, true);
  registry->RegisterStringPref(kOmniboxEverywhereHotkey, "");
  registry->RegisterBooleanPref(kOmniboxEverywhereEnabled, true);
  registry->RegisterBooleanPref(kOmniboxEverywhereBackgroundMode, false);
  registry->RegisterBooleanPref(kOmniboxEverywhereLaunchOnStartup, false);
#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, true);
#else
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, false);
#endif
  registry->RegisterFilePathPref(kLastTargetProfileDir, base::FilePath());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  registry->RegisterBooleanPref(kOmniboxEverywhereShowAiMode, true);
  registry->RegisterBooleanPref(kFreDismissed, false);
  registry->RegisterIntegerPref(kFreImpressionCount, 0);
  registry->RegisterBooleanPref(kFreIntroDismissed, false);
  registry->RegisterIntegerPref(kFreIntroImpressionCount, 0);
  registry->RegisterBooleanPref(kFreShortcutSetupDismissed, false);
  registry->RegisterIntegerPref(kFreShortcutSetupImpressionCount, 0);
  registry->RegisterBooleanPref(kFreShortcutReminderDismissed, false);
  registry->RegisterIntegerPref(kFreShortcutReminderImpressionCount, 0);
  registry->RegisterBooleanPref(kScreenshotDisclosureAccepted, false);
}

namespace {

PrefService* ResolveLocalState(PrefService* local_state) {
  if (local_state) {
    return local_state;
  }
  if (g_browser_process) {
    return g_browser_process->local_state();
  }
  return nullptr;
}

#if BUILDFLAG(IS_MAC)
void MapMacHotkeyToken(std::u16string& token) {
  // Accelerator code returns hotkeys on Mac represented by their respective
  // symbols (e.g. ⌘) rather than their spelled forms (e.g. Command). Map the
  // former to the latter using standard localized strings.
  if (token == u"⌃") {
    token = l10n_util::GetStringUTF16(IDS_APP_CTRL_KEY);
  } else if (token == u"⌥") {
    token = u"Option";
  } else if (token == u"⇧") {
    token = l10n_util::GetStringUTF16(IDS_APP_SHIFT_KEY);
  } else if (token == u"⌘") {
    token = u"Cmd";
  }
}
#endif

}  // namespace

bool HasOmniboxEverywhereHotkey(PrefService* local_state) {
  PrefService* resolved_local_state = ResolveLocalState(local_state);
  if (!resolved_local_state) {
    return false;
  }
  if (!resolved_local_state->GetBoolean(kHotkeyEnabled)) {
    return false;
  }
  return !GetOmniboxEverywhereHotkey(resolved_local_state).IsEmpty();
}

FreStage GetCurrentFreStage(Profile* profile, PrefService* local_state) {
  if (!profile || !profile->GetPrefs()) {
    return FreStage::kNone;
  }
  PrefService* prefs = profile->GetPrefs();
  PrefService* resolved_local_state = ResolveLocalState(local_state);

  if (prefs->GetBoolean(kFreDismissed)) {
    return FreStage::kNone;
  }

  // Stage 1: Intro Modal (max 2 impressions or until dismissed)
  if (!prefs->GetBoolean(kFreIntroDismissed) &&
      prefs->GetInteger(kFreIntroImpressionCount) < kMaxFreIntroImpressions) {
    return FreStage::kIntroModal;
  }

  // Stage 2: Shortcut Setup Chin (max 3 impressions or until dismissed/set).
  // If a hotkey is already set, skip Stage 2 setup chin and go directly to
  // Stage 3 reminder chin.
  if (!HasOmniboxEverywhereHotkey(resolved_local_state) &&
      !prefs->GetBoolean(kFreShortcutSetupDismissed) &&
      prefs->GetInteger(kFreShortcutSetupImpressionCount) <
          kMaxFreShortcutSetupImpressions) {
    return FreStage::kShortcutSetupChin;
  }

  // Stage 3: Shortcut Reminder Chin (max 3 impressions or until dismissed)
  if (!prefs->GetBoolean(kFreShortcutReminderDismissed) &&
      prefs->GetInteger(kFreShortcutReminderImpressionCount) <
          kMaxFreShortcutReminderImpressions) {
    if (HasOmniboxEverywhereHotkey(resolved_local_state)) {
      return FreStage::kShortcutReminderChin;
    }
  }

  return FreStage::kNone;
}

void IncrementFreImpression(Profile* profile, PrefService* local_state) {
  if (!profile || !profile->GetPrefs()) {
    return;
  }
  PrefService* prefs = profile->GetPrefs();
  PrefService* resolved_local_state = ResolveLocalState(local_state);
  FreStage current_stage = GetCurrentFreStage(profile, resolved_local_state);
  switch (current_stage) {
    case FreStage::kIntroModal:
      prefs->SetInteger(kFreIntroImpressionCount,
                        prefs->GetInteger(kFreIntroImpressionCount) + 1);
      break;
    case FreStage::kShortcutSetupChin: {
      // If a hotkey isn't set, impressions are not capped so the setup chin
      // remains visible until the user selects a shortcut or explicitly
      // dismisses it.
      const bool has_hotkey = HasOmniboxEverywhereHotkey(resolved_local_state);
      if (has_hotkey) {
        prefs->SetInteger(
            kFreShortcutSetupImpressionCount,
            prefs->GetInteger(kFreShortcutSetupImpressionCount) + 1);
      }
      break;
    }
    case FreStage::kShortcutReminderChin:
      prefs->SetInteger(
          kFreShortcutReminderImpressionCount,
          prefs->GetInteger(kFreShortcutReminderImpressionCount) + 1);
      break;
    case FreStage::kNone:
      break;
  }

  // If impressions have exhausted all stages, mark the overall FRE as dismissed
  // so downstream features (such as IPH) are unblocked.
  if (GetCurrentFreStage(profile, resolved_local_state) == FreStage::kNone) {
    prefs->SetBoolean(kFreDismissed, true);
  }
}

void OnFreStageDismissed(Profile* profile,
                         FreStage stage,
                         PrefService* local_state) {
  if (!profile || !profile->GetPrefs()) {
    return;
  }
  PrefService* prefs = profile->GetPrefs();
  PrefService* resolved_local_state = ResolveLocalState(local_state);
  switch (stage) {
    case FreStage::kIntroModal:
      prefs->SetBoolean(kFreIntroDismissed, true);
      break;
    case FreStage::kShortcutSetupChin: {
      prefs->SetBoolean(kFreShortcutSetupDismissed, true);
      const bool has_hotkey = HasOmniboxEverywhereHotkey(resolved_local_state);
      if (!has_hotkey) {
        prefs->SetBoolean(kFreShortcutReminderDismissed, true);
        prefs->SetBoolean(kFreDismissed, true);
      }
      break;
    }
    case FreStage::kShortcutReminderChin:
      prefs->SetBoolean(kFreShortcutReminderDismissed, true);
      prefs->SetBoolean(kFreDismissed, true);
      break;
    case FreStage::kNone:
      prefs->SetBoolean(kFreDismissed, true);
      break;
  }
}

ui::Accelerator GetDefaultOmniboxEverywhereHotkey() {
  // TODO(crbug.com/556870238): Remove the default hotkey because
  // shortcuts should be explicitly chosen by the user.
  return ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
}

ui::Accelerator GetOmniboxEverywhereHotkey(PrefService* local_state) {
  if (!local_state) {
    return GetDefaultOmniboxEverywhereHotkey();
  }

  const std::string hotkey_str =
      local_state->GetString(kOmniboxEverywhereHotkey);
  if (hotkey_str.empty()) {
    return GetDefaultOmniboxEverywhereHotkey();
  }

  const ui::Accelerator hotkey = ui::Command::StringToAccelerator(hotkey_str);
  if (!hotkey.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(hotkey.modifiers()) != 0) {
    return hotkey;
  }

  return GetDefaultOmniboxEverywhereHotkey();
}

void SetOmniboxEverywhereHotkey(PrefService* local_state,
                                std::string_view hotkey_str) {
  if (!local_state) {
    return;
  }
  local_state->SetString(kOmniboxEverywhereHotkey, hotkey_str);
}

std::vector<std::string> GetOmniboxEverywhereHotkeyTokens(
    const ui::Accelerator& accelerator) {
  std::vector<std::string> tokens;
  if (accelerator.IsEmpty()) {
    return tokens;
  }

  std::vector<std::u16string> shortcut_vector =
      accelerator.GetShortcutVectorRepresentation();

#if BUILDFLAG(IS_MAC)
  // On macOS, Accelerator::GetShortcutVectorRepresentation() represents
  // modifiers as symbols (e.g. ⌃, ⌥, ⇧, ⌘) in canonical Apple order:
  // [control -> option -> shift -> command].
  // Following the approach in chrome/browser/glic/glic_hotkey.cc, map
  // these symbols to their localized text labels.
  for (std::u16string& token : shortcut_vector) {
    MapMacHotkeyToken(token);
  }

  // Non-letter keys (e.g. Return, Backspace, Escape, Tab) are mapped to
  // glyphs by KeyCodeToMacSymbol() in GetShortcutVectorRepresentation().
  // Replace the key token with its localized name from KeyCodeToName()
  // for consistency with the text modifier badges.
  if (!shortcut_vector.empty()) {
    const std::u16string key_name = accelerator.KeyCodeToName();
    if (!key_name.empty()) {
      shortcut_vector.back() = key_name;
    }
  }
#endif

  tokens.reserve(shortcut_vector.size());
  for (const auto& token : shortcut_vector) {
    tokens.push_back(base::UTF16ToUTF8(token));
  }

  return tokens;
}

std::vector<std::string> GetAvailableHotkeyPresets() {
  const std::vector<ui::Accelerator> accelerators = {
#if BUILDFLAG(IS_MAC)
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN),
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
#else
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN),
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      ui::Accelerator(ui::VKEY_O, ui::EF_ALT_DOWN),
#endif
  };
  std::vector<std::string> presets;
  presets.reserve(accelerators.size());
  for (const auto& accelerator : accelerators) {
    // ui::Command::AcceleratorToString produces the canonical string
    // representation parseable by ui::Command::StringToAccelerator (unlike
    // ui::Accelerator::GetShortcutText() which produces localized UI text).
    presets.push_back(ui::Command::AcceleratorToString(accelerator));
  }
  return presets;
}
bool AreShortcutsAvailableForProfile(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return true;
  }
  PrefService* prefs = profile->GetPrefs();
  const bool has_enterprise_shortcuts =
      !prefs->GetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList).empty();
  if (!has_enterprise_shortcuts) {
    return true;
  }
  const bool enterprise_visible =
      prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible);
  const bool personal_visible =
      prefs->GetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible);
  return enterprise_visible || personal_visible;
}

bool IsOmniboxEverywhereShortcutsVisible(Profile* profile) {
  if (profile && profile->GetPrefs()) {
    PrefService* prefs = profile->GetPrefs();
    const auto pref_value = static_cast<ShowShortcutsPrefValue>(
        prefs->GetInteger(kOmniboxEverywhereShowShortcuts));
    if (pref_value == ShowShortcutsPrefValue::kDisabled) {
      return false;
    }
    if (pref_value == ShowShortcutsPrefValue::kEnabled) {
      return AreShortcutsAvailableForProfile(profile);
    }
    // Fallback to Customize Chrome / NTP setting.
    if (!prefs->GetBoolean(ntp_prefs::kNtpShortcutsVisible)) {
      return false;
    }
    return AreShortcutsAvailableForProfile(profile);
  }

  return false;
}

bool IsScreenshotDisclosureAccepted(const PrefService* prefs) {
  return prefs && prefs->GetBoolean(kScreenshotDisclosureAccepted);
}

bool IsScreenshotDisclosureAccepted(const Profile* profile) {
  return profile && IsScreenshotDisclosureAccepted(profile->GetPrefs());
}

void SetScreenshotDisclosureAccepted(PrefService* prefs, bool accepted) {
  if (prefs) {
    prefs->SetBoolean(kScreenshotDisclosureAccepted, accepted);
  }
}

void SetScreenshotDisclosureAccepted(Profile* profile, bool accepted) {
  if (profile) {
    SetScreenshotDisclosureAccepted(profile->GetPrefs(), accepted);
  }
}

}  // namespace prefs
}  // namespace omnibox_everywhere
