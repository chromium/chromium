// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_

namespace privacy_sandbox {

class PrivacySandboxSurveyService{
 public:
  // Records the survey's status when attempting to surface a
  // sentiment survey.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  // LINT.IfChange(PrivacySandboxSentimentSurveyStatus)
  enum class PrivacySandboxSentimentSurveyStatus {
    kSurveyShown = 0,          // Survey was successfully shown.
    kFeatureDisabled = 1,      // Sentiment Survey feature disabled.
    kHatsServiceFailed = 2,    // Could not initialize HaTS service.
    kSurveyLaunchFailed = 3,   // Survey invite failed to launch.
    kInvalidSurveyConfig = 4,  // Failed to initialize survey config.
    kMaxValue = kInvalidSurveyConfig,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/privacy/enums.xml)
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_
