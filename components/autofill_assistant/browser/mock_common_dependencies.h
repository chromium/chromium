// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_

#include "components/autofill_assistant/browser/common_dependencies.h"

#include <memory>
#include <string>

#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/security_state/core/security_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace autofill_assistant {

class MockCommonDependencies : public CommonDependencies {
 public:
  MockCommonDependencies();
  ~MockCommonDependencies() override;

  MOCK_METHOD(std::unique_ptr<AssistantFieldTrialUtil>,
              CreateFieldTrialUtil,
              (),
              (const override));
  MOCK_METHOD(std::string, GetLocale, (), (const override));
  MOCK_METHOD(std::string, GetLatestCountryCode, (), (const override));
  MOCK_METHOD(std::string, GetStoredPermanentCountryCode, (), (const override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (),
              (const override));
  MOCK_METHOD(password_manager::PasswordManagerClient*,
              GetPasswordManagerClient,
              (content::WebContents*),
              (const override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const override));
  MOCK_METHOD(std::string, GetSignedInEmail, (), (const override));
  MOCK_METHOD(security_state::SecurityLevel,
              GetSecurityLevel,
              (content::WebContents*),
              (const override));
  MOCK_METHOD(bool, IsSupervisedUser, (), (const override));
  MOCK_METHOD(bool, IsAllowedForMachineLearning, (), (const override));
  MOCK_METHOD(AnnotateDomModelService*,
              GetOrCreateAnnotateDomModelService,
              (),
              (const override));
  MOCK_METHOD(bool, IsWebLayer, (), (const override));
  MOCK_METHOD(signin::IdentityManager*,
              GetIdentityManager,
              (),
              (const override));
  MOCK_METHOD(consent_auditor::ConsentAuditor*,
              GetConsentAuditor,
              (),
              (const override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
  MOCK_METHOD(bool,
              GetMakeSearchesAndBrowsingBetterEnabled,
              (),
              (const override));
  MOCK_METHOD(bool, GetMetricsReportingEnabled, (), (const override));
};

}  // namespace autofill_assistant

#endif  // #COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_
