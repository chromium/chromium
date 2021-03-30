// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"

namespace chromeos {
namespace assistant {

AssistantNotification::AssistantNotification() = default;

AssistantNotification::~AssistantNotification() = default;

AssistantNotification::AssistantNotification(const AssistantNotification&) =
    default;

AssistantNotification& AssistantNotification::operator=(
    const AssistantNotification&) = default;

AssistantNotification::AssistantNotification(AssistantNotification&&) = default;

AssistantNotification& AssistantNotification::operator=(
    AssistantNotification&&) = default;
}  // namespace assistant
}  // namespace chromeos
