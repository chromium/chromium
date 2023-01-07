// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_

#include <string>
#include <vector>

#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::assistant {

class MockAssistantInteractionSubscriber
    : public AssistantInteractionSubscriber {
 public:
  MockAssistantInteractionSubscriber();
  MockAssistantInteractionSubscriber(
      const MockAssistantInteractionSubscriber&) = delete;
  MockAssistantInteractionSubscriber& operator=(
      const MockAssistantInteractionSubscriber&) = delete;
  ~MockAssistantInteractionSubscriber() override;

  // AssistantInteractionSubscriber:
  MOCK_METHOD(void,
              OnInteractionStarted,
              (const AssistantInteractionMetadata&),
              (override));
  MOCK_METHOD(void,
              OnInteractionFinished,
              (AssistantInteractionResolution),
              (override));
  MOCK_METHOD(void,
              OnHtmlResponse,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsResponse,
              (const std::vector<AssistantSuggestion>&),
              (override));
  MOCK_METHOD(void, OnTextResponse, (const std::string&), (override));
  MOCK_METHOD(void, OnOpenUrlResponse, (const ::GURL&, bool), (override));
  MOCK_METHOD(void, OnOpenAppResponse, (const AndroidAppInfo&), (override));
  MOCK_METHOD(void, OnSpeechRecognitionStarted, (), (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionIntermediateResult,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, OnSpeechRecognitionEndOfUtterance, (), (override));
  MOCK_METHOD(void,
              OnSpeechRecognitionFinalResult,
              (const std::string&),
              (override));
  MOCK_METHOD(void, OnSpeechLevelUpdated, (float), (override));
  MOCK_METHOD(void, OnTtsStarted, (bool), (override));
  MOCK_METHOD(void, OnWaitStarted, (), (override));
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_INTERACTION_SUBSCRIBER_H_
