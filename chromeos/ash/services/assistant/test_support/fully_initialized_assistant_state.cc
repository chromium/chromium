// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/fully_initialized_assistant_state.h"

namespace ash::assistant {

FullyInitializedAssistantState::FullyInitializedAssistantState() {
  InitializeAllValues();
}

void FullyInitializedAssistantState::SetAssistantEnabled(bool enabled) {
  settings_enabled_ = enabled;

  for (auto& observer : observers_)
    observer.OnAssistantSettingsEnabled(settings_enabled_.value());
}

void FullyInitializedAssistantState::SetContextEnabled(bool enabled) {
  context_enabled_ = enabled;
}

void FullyInitializedAssistantState::InitializeAllValues() {
  settings_enabled_ = true;
  consent_status_ = prefs::ConsentStatus::kActivityControlAccepted;
  context_enabled_ = true;
  hotword_enabled_ = true;
  hotword_always_on_ = true;
  launch_with_mic_open_ = true;
  notification_enabled_ = true;
  allowed_state_ = AssistantAllowedState::ALLOWED;
  locale_ = "en_US";
  arc_play_store_enabled_ = true;
  locked_full_screen_enabled_ = true;
}

}  // namespace ash::assistant
