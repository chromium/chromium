// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/assistant_interaction_logger.h"

#include <utility>

#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash::assistant {

namespace {

std::string ResolutionToString(AssistantInteractionResolution resolution) {
  std::stringstream result;
  result << static_cast<int>(resolution);
  return result.str();
}

bool IsPIILoggingAllowed() {
  return features::IsAssistantDebuggingEnabled();
}

std::string HidePiiMaybe(const std::string& value) {
  if (IsPIILoggingAllowed())
    return "[PII](" + value + ")";
  else
    return "[Redacted PII]";
}

#define LOG_INTERACTION() \
  LOG_INTERACTION_AT_LEVEL(AssistantInteractionLogger::kVLogLevel)

#define LOG_INTERACTION_AT_LEVEL(_level) \
  VLOG(_level) << "Assistant: " << __func__ << ": "

}  // namespace

bool AssistantInteractionLogger::IsLoggingEnabled() {
  return VLOG_IS_ON(kVLogLevel);
}

AssistantInteractionLogger::AssistantInteractionLogger() = default;

AssistantInteractionLogger::~AssistantInteractionLogger() = default;

void AssistantInteractionLogger::OnInteractionStarted(
    const AssistantInteractionMetadata& metadata) {
  switch (metadata.type) {
    case AssistantInteractionType::kText:
      LOG_INTERACTION() << "Text interaction with query "
                        << HidePiiMaybe(metadata.query);
      break;
    case AssistantInteractionType::kVoice:
      LOG_INTERACTION() << "Voice interaction";
      break;
  }
}

void AssistantInteractionLogger::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  LOG_INTERACTION() << "with resolution " << ResolutionToString(resolution);
}

void AssistantInteractionLogger::OnHtmlResponse(const std::string& response,
                                                const std::string& fallback) {
  // Displaying fallback instead of the response as the response is filled with
  // HTML tags and rather large.
  LOG_INTERACTION() << "with fallback '" << fallback << "'";
  // Display HTML at highest verbosity.
  LOG_INTERACTION_AT_LEVEL(3) << "with HTML: " << HidePiiMaybe(response);
}

void AssistantInteractionLogger::OnSuggestionsResponse(
    const std::vector<assistant::AssistantSuggestion>& response) {
  std::stringstream suggestions;
  for (const auto& suggestion : response)
    suggestions << "'" << suggestion.text << "', ";
  LOG_INTERACTION() << "{ " << suggestions.str() << " }";
}

void AssistantInteractionLogger::OnTextResponse(const std::string& response) {
  LOG_INTERACTION() << HidePiiMaybe(response);
}

void AssistantInteractionLogger::OnOpenUrlResponse(const GURL& url,
                                                   bool in_background) {
  LOG_INTERACTION() << "with url '" << url.possibly_invalid_spec() << "'";
}

void AssistantInteractionLogger::OnOpenAppResponse(
    const AndroidAppInfo& app_info) {
  LOG_INTERACTION() << "with app '" << app_info.package_name << "'";
}

void AssistantInteractionLogger::OnSpeechRecognitionStarted() {
  LOG_INTERACTION();
}

void AssistantInteractionLogger::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  // Not logged until we have a use for this (and this might spam the log).
}

void AssistantInteractionLogger::OnSpeechRecognitionEndOfUtterance() {
  LOG_INTERACTION();
}

void AssistantInteractionLogger::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  LOG_INTERACTION() << "with final result '" << final_result << "'";
}

void AssistantInteractionLogger::OnSpeechLevelUpdated(float speech_level) {
  // Not logged until we have a use for this and this might spam the log.
}

void AssistantInteractionLogger::OnTtsStarted(bool due_to_error) {
  LOG_INTERACTION() << (due_to_error ? "not" : "") << "due to error";
}

void AssistantInteractionLogger::OnWaitStarted() {
  LOG_INTERACTION();
}

}  // namespace ash::assistant
