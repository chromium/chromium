// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SEARCH_OVERRIDE_STACK_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SEARCH_OVERRIDE_STACK_H_

#include <optional>

#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

// Describes what sits below the extension controlling the default search
// engine. The settings overridden dialog offers whatever would take over if
// that extension were disabled, which may be another extension the user never
// acknowledged.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SearchOverrideStackState)
enum class SearchOverrideStackState {
  // Disabling the controlling extension returns the non-extension default.
  kSingleOverride = 0,
  // The next extension in precedence was acknowledged by the user.
  kAcknowledgedExtensionNext = 1,
  // Unacknowledged extensions sit above an acknowledged one.
  kUnacknowledgedChainOverAcknowledgedExtension = 2,
  // Unacknowledged extensions bury the non-extension default.
  kUnacknowledgedChainOverNonExtensionDefault = 3,
  // The next extension in precedence can't be disabled, e.g. by policy.
  kMustRemainEnabledExtensionNext = 4,
  // Unacknowledged extensions sit above one that can't be disabled.
  kUnacknowledgedChainOverMustRemainEnabledExtension = 5,
  kMaxValue = kUnacknowledgedChainOverMustRemainEnabledExtension,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/extensions/enums.xml:SearchOverrideStackState)

struct SearchOverrideStackInfo {
  SearchOverrideStackState state = SearchOverrideStackState::kSingleOverride;
  // Unacknowledged extensions between the controlling one (not counted) and
  // the first that is acknowledged or can't be disabled.
  int unacknowledged_chain_length = 0;
};

// Classifies the stack below `controlling_extension_id`. An extension counts
// as acknowledged only if the user accepted it in the dialog
// (ControlledHomeDialogController::kAcknowledgedPreference); one that was
// never prompted does not. `profile` may be off the record, since extension
// preferences are shared with the original profile.
//
// Returns nullopt if the preference stack disagrees that
// `controlling_extension_id` is on top, which happens only in tests where
// extension-controlled preferences aren't populated.
std::optional<SearchOverrideStackInfo> GetSearchOverrideStackInfo(
    Profile& profile,
    const ExtensionId& controlling_extension_id);

// Records the stack histograms, once per `controlling_extension_id` per
// session. The dialog re-shows on every search until the user decides, so
// per-show samples would over-weight the users who keep deferring.
void RecordSearchOverrideStackMetricsOnce(
    Profile& profile,
    const ExtensionId& controlling_extension_id,
    const SearchOverrideStackInfo& info);

// Records `result` split by stack state, once per dialog result.
void RecordSearchOverrideStackDialogResult(
    const SearchOverrideStackInfo& info,
    SettingsOverriddenDialogController::DialogResult result);

// Records the stack state once per profile per session. Unlike the histograms
// above this doesn't need the dialog, which is suppressed when the controlling
// extension sets the same engine as the one beneath it. No-op off the record.
void RecordDseExtensionStackStateOnce(Profile& profile);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SEARCH_OVERRIDE_STACK_H_
