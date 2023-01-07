// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"

namespace ash::assistant {

AssistantNotification::AssistantNotification() = default;

AssistantNotification::~AssistantNotification() = default;

AssistantNotification::AssistantNotification(const AssistantNotification&) =
    default;

AssistantNotification& AssistantNotification::operator=(
    const AssistantNotification&) = default;

AssistantNotification::AssistantNotification(AssistantNotification&&) = default;

AssistantNotification& AssistantNotification::operator=(
    AssistantNotification&&) = default;

}  // namespace ash::assistant
