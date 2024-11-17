// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace privacy_sandbox {

// This service manages survey eligibility determination, ensuring surveys are
// surfaced only when specific criteria are met.
class PrivacySandboxSurveyService : public KeyedService {
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
  // LINT.ThenChange(/tools/metrics/histograms/enums.xml)

  explicit PrivacySandboxSurveyService(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);
  ~PrivacySandboxSurveyService() override;
  PrivacySandboxSurveyService(const PrivacySandboxSurveyService&) = delete;
  PrivacySandboxSurveyService& operator=(const PrivacySandboxSurveyService&) =
      delete;

  // Determines if the sentiment survey should be surfaced. Returning `true`
  // does not guarantee that a survey is shown.
  bool ShouldShowSentimentSurvey();

  // Fetch the required product specific bits for the sentiment survey.
  std::map<std::string, bool> GetSentimentSurveyPsb();

  // Emits the given sentiment survey status.
  void RecordSentimentSurveyStatus(PrivacySandboxSentimentSurveyStatus status);

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_SERVICE_H_
