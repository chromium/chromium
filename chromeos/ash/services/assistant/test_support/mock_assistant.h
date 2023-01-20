// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_

#include <string>
#include <vector>

#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash::assistant {

class MockAssistant : public Assistant {
 public:
  MockAssistant();

  MockAssistant(const MockAssistant&) = delete;
  MockAssistant& operator=(const MockAssistant&) = delete;

  ~MockAssistant() override;

  MOCK_METHOD1(StartEditReminderInteraction, void(const std::string&));

  MOCK_METHOD(void,
              StartTextInteraction,
              (const std::string&, AssistantQuerySource, bool));

  MOCK_METHOD0(StartVoiceInteraction, void());

  MOCK_METHOD1(StopActiveInteraction, void(bool));

  MOCK_METHOD1(AddAssistantInteractionSubscriber,
               void(AssistantInteractionSubscriber*));

  MOCK_METHOD1(RemoveAssistantInteractionSubscriber,
               void(AssistantInteractionSubscriber*));

  MOCK_METHOD2(RetrieveNotification, void(const AssistantNotification&, int));

  MOCK_METHOD1(DismissNotification, void(const AssistantNotification&));

  MOCK_METHOD1(OnAccessibilityStatusChanged, void(bool));

  MOCK_METHOD1(SendAssistantFeedback, void(const AssistantFeedback&));

  MOCK_METHOD0(StopAlarmTimerRinging, void());
  MOCK_METHOD1(CreateTimer, void(base::TimeDelta));
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
