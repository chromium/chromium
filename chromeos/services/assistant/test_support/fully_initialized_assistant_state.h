// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_

#include "ash/public/cpp/assistant/assistant_state.h"

namespace chromeos {
namespace assistant {

// Instance of |AssistantState| where every base::Optional value has a non-null
// value. All values will be set to their equivalent of enabled.
class FullyInitializedAssistantState : public ash::AssistantState {
 public:
  FullyInitializedAssistantState();
  ~FullyInitializedAssistantState() override = default;

 private:
  void InitializeAllValues();

  DISALLOW_COPY_AND_ASSIGN(FullyInitializedAssistantState);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FULLY_INITIALIZED_ASSISTANT_STATE_H_
