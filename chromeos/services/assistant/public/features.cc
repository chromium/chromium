// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/features.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

const base::Feature kAssistantAudioEraser{"AssistantAudioEraser",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantFeedbackUi{"AssistantFeedbackUi",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantWarmerWelcomeFeature{
    "AssistantWarmerWelcome", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantAppSupport{"AssistantAppSupport",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantProactiveSuggestions{
    "AssistantProactiveSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// The maximum width (in dip) for the proactive suggestions chip.
const base::FeatureParam<int> kAssistantProactiveSuggestionsMaxWidth{
    &kAssistantProactiveSuggestions, "max-width", 280};

const base::FeatureParam<std::string>
    kAssistantProactiveSuggestionsServerExperimentIds{
        &kAssistantProactiveSuggestions, "server-experiment-ids", ""};

// When enabled, the proactive suggestions view will show only after the user
// scrolls up in the source web contents. When disabled, the view will be shown
// immediately once the set of proactive suggestions are available.
const base::FeatureParam<bool> kAssistantProactiveSuggestionsShowOnScroll{
    &kAssistantProactiveSuggestions, "show-on-scroll", true};

const base::FeatureParam<bool> kAssistantProactiveSuggestionsSuppressDuplicates{
    &kAssistantProactiveSuggestions, "suppress-duplicates", false};

const base::FeatureParam<int>
    kAssistantProactiveSuggestionsTimeoutThresholdMillis{
        &kAssistantProactiveSuggestions, "timeout-threshold-millis", 15 * 1000};

const base::Feature kAssistantRoutines{"AssistantRoutines",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInAssistantNotifications{
    "InAssistantNotifications", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableClearCutLog{"EnableClearCutLog",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStereoAudioInput{"AssistantEnableStereoAudioInput",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableTextQueriesWithClientDiscourseContext{
    "AssistantEnableTextQueriesWithClientDiscourseContext",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePowerManager{"ChromeOSAssistantEnablePowerManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables sending a screen context request ("What's on my screen?" and
// metalayer selection) as a text query. This is as opposed to sending
// the request as a contextual cards request.
const base::Feature kScreenContextQuery{"ChromeOSAssistantScreenContextQuery",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableMediaSessionIntegration{
    "AssistantEnableMediaSessionIntegration",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disable voice match for test purpose.
const base::Feature kDisableVoiceMatch{"DisableVoiceMatch",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

int GetProactiveSuggestionsMaxWidth() {
  return kAssistantProactiveSuggestionsMaxWidth.Get();
}

std::string GetProactiveSuggestionsServerExperimentIds() {
  return kAssistantProactiveSuggestionsServerExperimentIds.Get();
}

base::TimeDelta GetProactiveSuggestionsTimeoutThreshold() {
  return base::TimeDelta::FromMilliseconds(
      kAssistantProactiveSuggestionsTimeoutThresholdMillis.Get());
}

bool IsAppSupportEnabled() {
  return base::FeatureList::IsEnabled(
      assistant::features::kAssistantAppSupport);
}

bool IsAudioEraserEnabled() {
  return base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsClearCutLogEnabled() {
  return base::FeatureList::IsEnabled(kEnableClearCutLog);
}

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

bool IsFeedbackUiEnabled() {
  return base::FeatureList::IsEnabled(kAssistantFeedbackUi);
}

bool IsInAssistantNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kInAssistantNotifications);
}

bool IsMediaSessionIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kEnableMediaSessionIntegration);
}

bool IsPowerManagerEnabled() {
  return base::FeatureList::IsEnabled(kEnablePowerManager);
}

bool IsProactiveSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantProactiveSuggestions);
}

bool IsProactiveSuggestionsShowOnScrollEnabled() {
  return kAssistantProactiveSuggestionsShowOnScroll.Get();
}

bool IsProactiveSuggestionsSuppressDuplicatesEnabled() {
  return kAssistantProactiveSuggestionsSuppressDuplicates.Get();
}

bool IsRoutinesEnabled() {
  return base::FeatureList::IsEnabled(kAssistantRoutines);
}

bool IsScreenContextQueryEnabled() {
  return base::FeatureList::IsEnabled(kScreenContextQuery);
}

bool IsStereoAudioInputEnabled() {
  return base::FeatureList::IsEnabled(kEnableStereoAudioInput) ||
         // Audio eraser requires 2 channel input.
         base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsWarmerWelcomeEnabled() {
  return base::FeatureList::IsEnabled(kAssistantWarmerWelcomeFeature);
}

bool IsVoiceMatchDisabled() {
  return base::FeatureList::IsEnabled(kDisableVoiceMatch);
}

bool IsAssistantWebContainerEnabled() {
  return app_list_features::IsAssistantLauncherUIEnabled();
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
