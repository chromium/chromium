// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FRE_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FRE_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace omnibox_everywhere {

// Stages of the Omnibox Everywhere First Run Experience (FRE).
// Note: Enumerator values must match `searchbox::mojom::FreStage` as
// `OmniboxEverywhereUI` serializes `static_cast<int>(FreStage)` directly to
// WebUI (`initialFreStage`).

// LINT.IfChange(FreStage)
enum class FreStage {
  // FRE is complete or inactive.
  kNone = 0,
  // Stage 1: Two-row modal card (Value prop + Where to find).
  kIntroModal = 1,
  // Stage 2: One-row chin with shortcut selector dropdown & "Set as shortcut"
  // CTA.
  kShortcutSetupChin = 2,
  // Stage 3: One-row educational chin showing active shortcut reminder.
  kShortcutReminderChin = 3,
};
// LINT.ThenChange(//components/omnibox/browser/searchbox.mojom:FreStage)

// Encapsulates First Run Experience (FRE) state machine transitions and
// preference observation for Omnibox Everywhere.
class OmniboxEverywhereFreController {
 public:
  explicit OmniboxEverywhereFreController(PrefService* profile_prefs,
                                          PrefService* local_state = nullptr);
  OmniboxEverywhereFreController(const OmniboxEverywhereFreController&) =
      delete;
  OmniboxEverywhereFreController& operator=(
      const OmniboxEverywhereFreController&) = delete;
  ~OmniboxEverywhereFreController();

  // Returns the active FRE stage for the profile. While an open-window session
  // has latched a stage via `IncrementImpression()`, returns the latched stage
  // so the displayed FRE stage does not transition mid-session.
  FreStage GetCurrentStage() const;

  // Returns true if the overall FRE has been dismissed/completed or if the FRE
  // feature is disabled.
  bool IsFreDismissed() const;

  // Records an impression for `stage` if it matches the currently active FRE
  // stage (at most once per open-window session) and latches the active stage
  // until `EndSession()` is called.
  void IncrementImpression(FreStage stage);

  // Ends the current open-window session (called when the Omnibox Everywhere
  // window is closed or destroyed; in persistent mode, blur/demote keeps the
  // session open), clearing the session's impression guard and stage latch so
  // the next invocation reflects any stage advancement.
  void EndSession();

  // Records that the specified FRE stage was dismissed (or marks overall FRE
  // completed).
  void OnStageDismissed(FreStage stage);

  // Marks the specified FRE stage as completed (as opposed to dismissed via
  // 'X'). For example, configuring a shortcut outside the chin (e.g. via
  // Settings) completes Stage 2 and advances past it without skipping
  // downstream stages.
  void MarkStageCompleted(FreStage stage);

  // Registers a callback to be notified whenever FRE-related profile or local
  // state preferences change.
  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingClosure callback);

 private:
  FreStage ComputeCurrentStage() const;
  void RecordShortcutSetupImpression();
  void OnProfilePrefChanged();
  void OnLocalStatePrefChanged();
  void NotifyStateChanged();

  raw_ptr<PrefService> profile_prefs_ = nullptr;
  raw_ptr<PrefService> local_state_ = nullptr;
  std::optional<FreStage> latched_stage_;
  bool impression_recorded_for_session_ = false;
  PrefChangeRegistrar profile_pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  base::RepeatingClosureList state_changed_callbacks_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FRE_CONTROLLER_H_
