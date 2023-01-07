// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_

#include <string>
#include <vector>

#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash::assistant {

// A subscriber that will log all Assistant interactions.
// The interactions will be logged using
//     VLOG(AssistantInteractionLogger::kVLogLevel)
class AssistantInteractionLogger : public AssistantInteractionSubscriber {
 public:
  // VLog level used for logging interactions.
  constexpr static const int kVLogLevel = 1;

  // Returns if the current logging level is high enough so that the traces
  // will be printed. If not, there is no point in creating this class.
  static bool IsLoggingEnabled();

  AssistantInteractionLogger();
  AssistantInteractionLogger(AssistantInteractionLogger&) = delete;
  AssistantInteractionLogger& operator=(AssistantInteractionLogger&) = delete;
  ~AssistantInteractionLogger() override;

  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override;

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;

  void OnSuggestionsResponse(
      const std::vector<AssistantSuggestion>& response) override;

  void OnTextResponse(const std::string& response) override;

  void OnOpenUrlResponse(const GURL& url, bool in_background) override;

  void OnOpenAppResponse(const AndroidAppInfo& app_info) override;

  void OnSpeechRecognitionStarted() override;

  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override;

  void OnSpeechRecognitionEndOfUtterance() override;

  void OnSpeechRecognitionFinalResult(const std::string& final_result) override;

  void OnSpeechLevelUpdated(float speech_level) override;

  void OnTtsStarted(bool due_to_error) override;

  void OnWaitStarted() override;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_INTERACTION_LOGGER_H_
