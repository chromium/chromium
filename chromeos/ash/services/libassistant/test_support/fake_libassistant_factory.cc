// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/test_support/fake_libassistant_factory.h"

#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"

namespace ash::libassistant {

FakeLibassistantFactory::FakeLibassistantFactory() {
  // We start by creating a pending assistant manager, as our unittests
  // might need to access the assistant manager before it is created through
  // CreateAssistantManager() (for example to set expectations).
  pending_assistant_manager_ =
      std::make_unique<chromeos::assistant::FakeAssistantManager>();
}

FakeLibassistantFactory::~FakeLibassistantFactory() = default;

std::unique_ptr<assistant_client::AssistantManager>
FakeLibassistantFactory::CreateAssistantManager(
    const std::string& libassistant_config) {
  auto result = std::move(pending_assistant_manager_);
  if (!result) {
    // We come here if this is not the first call to CreateAssistantManager().
    result = std::make_unique<chromeos::assistant::FakeAssistantManager>();
  }

  // Keep a pointer around so our unittests can still retrieve it.
  current_assistant_manager_ = result.get();
  libassistant_config_ = libassistant_config;

  return result;
}

chromeos::assistant::FakeAssistantManager&
FakeLibassistantFactory::assistant_manager() {
  if (current_assistant_manager_)
    return *current_assistant_manager_;

  // We should only come here if CreateAssistantManager() has not been called
  // yet. In that case we return a pointer to the pending assistant manager
  // instead.
  DCHECK(pending_assistant_manager_);
  return *pending_assistant_manager_;
}

}  // namespace ash::libassistant
