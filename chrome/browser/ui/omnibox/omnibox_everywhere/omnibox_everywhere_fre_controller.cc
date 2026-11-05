// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_fre_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "components/prefs/pref_service.h"

namespace omnibox_everywhere {

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

}  // namespace

OmniboxEverywhereFreController::OmniboxEverywhereFreController(
    PrefService* profile_prefs,
    PrefService* local_state)
    : profile_prefs_(profile_prefs),
      local_state_(ResolveLocalState(local_state)) {
  if (profile_prefs_) {
    profile_pref_change_registrar_.Init(profile_prefs_);
    auto callback = base::BindRepeating(
        &OmniboxEverywhereFreController::OnProfilePrefChanged,
        base::Unretained(this));
    profile_pref_change_registrar_.Add(prefs::kFreIntroDismissed, callback);
    profile_pref_change_registrar_.Add(prefs::kFreShortcutSetupDismissed,
                                       callback);
    profile_pref_change_registrar_.Add(prefs::kFreShortcutReminderDismissed,
                                       callback);
    profile_pref_change_registrar_.Add(prefs::kFreIntroImpressionCount,
                                       callback);
    profile_pref_change_registrar_.Add(prefs::kFreShortcutSetupImpressionCount,
                                       callback);
    profile_pref_change_registrar_.Add(
        prefs::kFreShortcutReminderImpressionCount, callback);
    profile_pref_change_registrar_.Add(prefs::kFreDismissed, callback);
  }

  if (local_state_) {
    local_state_pref_change_registrar_.Init(local_state_);
    auto callback = base::BindRepeating(
        &OmniboxEverywhereFreController::OnLocalStatePrefChanged,
        base::Unretained(this));
    local_state_pref_change_registrar_.Add(prefs::kOmniboxEverywhereHotkey,
                                           callback);
    local_state_pref_change_registrar_.Add(prefs::kHotkeyEnabled, callback);
  }
}

OmniboxEverywhereFreController::~OmniboxEverywhereFreController() = default;

FreStage OmniboxEverywhereFreController::GetCurrentStage() const {
  if (!profile_prefs_ ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return FreStage::kNone;
  }
  if (latched_stage_.has_value()) {
    return latched_stage_.value();
  }
  return ComputeCurrentStage();
}

FreStage OmniboxEverywhereFreController::ComputeCurrentStage() const {
  if (profile_prefs_->GetBoolean(prefs::kFreDismissed)) {
    return FreStage::kNone;
  }

  // Stage 1: Intro Modal (max 2 impressions or until dismissed)
  if (!profile_prefs_->GetBoolean(prefs::kFreIntroDismissed) &&
      profile_prefs_->GetInteger(prefs::kFreIntroImpressionCount) <
          omnibox::kOmniboxEverywhereFreMaxIntroImpressionsParam.Get()) {
    return FreStage::kIntroModal;
  }

  // Stage 2: Shortcut Setup Chin.
  // Stays active until dismissed via 'X' or until a hotkey is selected and
  // the impression threshold is reached (on the next open).
  if (!profile_prefs_->GetBoolean(prefs::kFreShortcutSetupDismissed)) {
    const bool has_hotkey = prefs::HasOmniboxEverywhereHotkey(local_state_);
    if (!has_hotkey ||
        profile_prefs_->GetInteger(prefs::kFreShortcutSetupImpressionCount) <
            omnibox::kOmniboxEverywhereFreMaxShortcutSetupImpressionsParam
                .Get()) {
      return FreStage::kShortcutSetupChin;
    }
  }

  // Stage 3: Shortcut Reminder Chin (max 3 impressions or until dismissed)
  if (!profile_prefs_->GetBoolean(prefs::kFreShortcutReminderDismissed) &&
      profile_prefs_->GetInteger(prefs::kFreShortcutReminderImpressionCount) <
          omnibox::kOmniboxEverywhereFreMaxShortcutReminderImpressionsParam
              .Get()) {
    if (prefs::HasOmniboxEverywhereHotkey(local_state_)) {
      return FreStage::kShortcutReminderChin;
    }
  }

  return FreStage::kNone;
}

bool OmniboxEverywhereFreController::IsFreDismissed() const {
  if (!profile_prefs_ ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return true;
  }
  if (latched_stage_.has_value()) {
    return false;
  }
  return profile_prefs_->GetBoolean(prefs::kFreDismissed);
}

void OmniboxEverywhereFreController::IncrementImpression(FreStage stage) {
  if (!profile_prefs_ ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return;
  }
  if (latched_stage_.has_value()) {
    return;
  }

  FreStage current_stage = ComputeCurrentStage();
  if (current_stage == FreStage::kNone || current_stage != stage) {
    return;
  }

  latched_stage_ = current_stage;
  if (impression_recorded_for_session_) {
    return;
  }

  switch (current_stage) {
    case FreStage::kIntroModal: {
      impression_recorded_for_session_ = true;
      const int next_count =
          profile_prefs_->GetInteger(prefs::kFreIntroImpressionCount) + 1;
      if (next_count >=
              omnibox::kOmniboxEverywhereFreMaxIntroImpressionsParam.Get() &&
          prefs::HasOmniboxEverywhereHotkey(local_state_)) {
        profile_prefs_->SetBoolean(prefs::kFreShortcutSetupDismissed, true);
      }
      profile_prefs_->SetInteger(prefs::kFreIntroImpressionCount, next_count);
      break;
    }
    case FreStage::kShortcutSetupChin: {
      // If a hotkey isn't set, impressions are not capped so the setup chin
      // remains visible until the user selects a shortcut or explicitly
      // dismisses it. If a hotkey is selected later during this same session,
      // `OnLocalStatePrefChanged()` will record the impression then.
      if (prefs::HasOmniboxEverywhereHotkey(local_state_)) {
        RecordShortcutSetupImpression();
      }
      return;
    }
    case FreStage::kShortcutReminderChin:
      impression_recorded_for_session_ = true;
      profile_prefs_->SetInteger(
          prefs::kFreShortcutReminderImpressionCount,
          profile_prefs_->GetInteger(
              prefs::kFreShortcutReminderImpressionCount) +
              1);
      break;
    case FreStage::kNone:
      break;
  }

  // If impressions have exhausted all stages, mark the overall FRE as dismissed
  // so downstream features (such as IPH) are unblocked on the next open.
  if (ComputeCurrentStage() == FreStage::kNone) {
    profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
  }
}

void OmniboxEverywhereFreController::RecordShortcutSetupImpression() {
  impression_recorded_for_session_ = true;
  profile_prefs_->SetInteger(
      prefs::kFreShortcutSetupImpressionCount,
      profile_prefs_->GetInteger(prefs::kFreShortcutSetupImpressionCount) + 1);
  if (ComputeCurrentStage() == FreStage::kNone) {
    profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
  }
}

void OmniboxEverywhereFreController::EndSession() {
  const bool was_latched = latched_stage_.has_value();
  latched_stage_.reset();
  impression_recorded_for_session_ = false;
  if (was_latched) {
    NotifyStateChanged();
  }
}

void OmniboxEverywhereFreController::OnStageDismissed(FreStage stage) {
  if (!profile_prefs_ ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return;
  }
  latched_stage_.reset();
  impression_recorded_for_session_ = false;
  switch (stage) {
    case FreStage::kIntroModal:
      if (prefs::HasOmniboxEverywhereHotkey(local_state_)) {
        profile_prefs_->SetBoolean(prefs::kFreShortcutSetupDismissed, true);
      }
      profile_prefs_->SetBoolean(prefs::kFreIntroDismissed, true);
      break;
    case FreStage::kShortcutSetupChin:
      profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
      profile_prefs_->SetBoolean(prefs::kFreShortcutReminderDismissed, true);
      profile_prefs_->SetBoolean(prefs::kFreShortcutSetupDismissed, true);
      break;
    case FreStage::kShortcutReminderChin:
      profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
      profile_prefs_->SetBoolean(prefs::kFreShortcutReminderDismissed, true);
      break;
    case FreStage::kNone:
      profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
      break;
  }
}

void OmniboxEverywhereFreController::MarkStageCompleted(FreStage stage) {
  if (!profile_prefs_ ||
      !base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    return;
  }
  latched_stage_.reset();
  impression_recorded_for_session_ = false;
  switch (stage) {
    case FreStage::kIntroModal:
      profile_prefs_->SetBoolean(prefs::kFreIntroDismissed, true);
      break;
    case FreStage::kShortcutSetupChin:
      profile_prefs_->SetBoolean(prefs::kFreShortcutSetupDismissed, true);
      break;
    case FreStage::kShortcutReminderChin:
      profile_prefs_->SetBoolean(prefs::kFreDismissed, true);
      profile_prefs_->SetBoolean(prefs::kFreShortcutReminderDismissed, true);
      break;
    case FreStage::kNone:
      break;
  }
}

base::CallbackListSubscription
OmniboxEverywhereFreController::RegisterStateChangedCallback(
    base::RepeatingClosure callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

void OmniboxEverywhereFreController::OnProfilePrefChanged() {
  if (!latched_stage_.has_value()) {
    NotifyStateChanged();
  }
}

void OmniboxEverywhereFreController::OnLocalStatePrefChanged() {
  if (profile_prefs_ &&
      base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre)) {
    if (prefs::HasOmniboxEverywhereHotkey(local_state_)) {
      if (latched_stage_ == FreStage::kShortcutSetupChin &&
          !impression_recorded_for_session_) {
        RecordShortcutSetupImpression();
      }
    } else if (latched_stage_ == FreStage::kShortcutReminderChin) {
      // If the hotkey is cleared or disabled while the reminder chin is
      // latched, unlatch so the reminder chin does not remain visible with
      // empty key tokens.
      latched_stage_.reset();
    }
  }
  NotifyStateChanged();
}

void OmniboxEverywhereFreController::NotifyStateChanged() {
  state_changed_callbacks_.Notify();
}

}  // namespace omnibox_everywhere
