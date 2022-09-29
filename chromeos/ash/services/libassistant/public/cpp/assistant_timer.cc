// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"

namespace ash::assistant {

AssistantTimer::AssistantTimer() = default;
AssistantTimer::AssistantTimer(const AssistantTimer&) = default;
AssistantTimer& AssistantTimer::operator=(const AssistantTimer&) = default;
AssistantTimer::~AssistantTimer() = default;

bool AssistantTimer::IsEqualInLibAssistantTo(
    const AssistantTimer& other) const {
  return id == other.id && label == other.label &&
         fire_time == other.fire_time &&
         original_duration == other.original_duration && state == other.state;
}

}  // namespace ash::assistant
