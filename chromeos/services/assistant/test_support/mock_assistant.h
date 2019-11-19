// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_

#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {
namespace assistant {

class MockAssistant : public mojom::Assistant {
 public:
  MockAssistant();
  ~MockAssistant() override;

  MOCK_METHOD0(StartCachedScreenContextInteraction, void());

  MOCK_METHOD1(StartEditReminderInteraction, void(const std::string&));

  MOCK_METHOD1(StartMetalayerInteraction, void(const gfx::Rect&));

  MOCK_METHOD3(StartTextInteraction,
               void(const std::string&, mojom::AssistantQuerySource, bool));

  MOCK_METHOD0(StartVoiceInteraction, void());

  MOCK_METHOD2(StartWarmerWelcomeInteraction, void(int, bool));

  MOCK_METHOD1(StopActiveInteraction, void(bool));

  MOCK_METHOD1(
      AddAssistantInteractionSubscriber,
      void(mojo::PendingRemote<
           chromeos::assistant::mojom::AssistantInteractionSubscriber>));

  MOCK_METHOD2(RetrieveNotification,
               void(chromeos::assistant::mojom::AssistantNotificationPtr, int));

  MOCK_METHOD1(DismissNotification,
               void(chromeos::assistant::mojom::AssistantNotificationPtr));

  // Mock DoCacheScreenContext in lieu of CacheScreenContext.
  MOCK_METHOD1(DoCacheScreenContext, void(base::OnceClosure*));

  // Note: We can't mock CacheScreenContext directly because of the move
  // semantics required around base::OnceClosure. Instead, we route calls to a
  // mockable delegate method, DoCacheScreenContext.
  void CacheScreenContext(base::OnceClosure callback) override {
    DoCacheScreenContext(&callback);
  }

  MOCK_METHOD0(ClearScreenContextCache, void());

  MOCK_METHOD1(OnAccessibilityStatusChanged, void(bool));

  MOCK_METHOD1(SendAssistantFeedback,
               void(chromeos::assistant::mojom::AssistantFeedbackPtr));

  MOCK_METHOD0(StopAlarmTimerRinging, void());
  MOCK_METHOD1(CreateTimer, void(base::TimeDelta));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAssistant);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
