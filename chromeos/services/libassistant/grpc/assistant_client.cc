// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client.h"

#include <memory>

#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace chromeos {
namespace libassistant {

AssistantClient::AssistantClient(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal)
    : assistant_manager_(std::move(assistant_manager)),
      assistant_manager_internal_(assistant_manager_internal) {}

AssistantClient::~AssistantClient() = default;

void AssistantClient::ResetAssistantManager() {
  assistant_manager_ = nullptr;
}

}  // namespace libassistant
}  // namespace chromeos
