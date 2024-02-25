// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_

#include "ash/public/cpp/assistant/assistant_state.h"

namespace ash::assistant {

// Instance of |AssistantState| where every std::optional value has a non-null
// value. All values will be set to their equivalent of enabled.
class FullyInitializedAssistantState : public AssistantState {
 public:
  FullyInitializedAssistantState();

  FullyInitializedAssistantState(const FullyInitializedAssistantState&) =
      delete;
  FullyInitializedAssistantState& operator=(
      const FullyInitializedAssistantState&) = delete;

  ~FullyInitializedAssistantState() override = default;

  void SetAssistantEnabled(bool enabled);

  void SetContextEnabled(bool enabled);

 private:
  void InitializeAllValues();
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_
