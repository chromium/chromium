// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"

#include <memory>

#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace ash::libassistant {

AssistantClient::AssistantClient(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager)
    : assistant_manager_(std::move(assistant_manager)) {}

AssistantClient::~AssistantClient() = default;

void AssistantClient::ResetAssistantManager() {
  assistant_manager_ = nullptr;
}

}  // namespace ash::libassistant
