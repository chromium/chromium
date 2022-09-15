// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_

#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/version_info/channel.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace autofill_assistant {

class FakeCommonDependencies : public CommonDependencies {
 public:
  explicit FakeCommonDependencies(signin::IdentityManager* identity_manager);
  ~FakeCommonDependencies() override;

  // From CommonDependencies:
  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;
  std::string GetLocale() const override;
  std::string GetCountryCode() const override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;
  PrefService* GetPrefs() const override;
  std::string GetSignedInEmail() const override;
  bool IsSupervisedUser() const override;
  bool IsAllowedForMachineLearning() const override;
  AnnotateDomModelService* GetOrCreateAnnotateDomModelService() const override;
  bool IsWebLayer() const override;
  signin::IdentityManager* GetIdentityManager() const override;
  consent_auditor::ConsentAuditor* GetConsentAuditor() const override;
  version_info::Channel GetChannel() const override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetMetricsReportingEnabled() const override;

  // Intentionally public to allow tests direct access.
  std::string locale_;
  std::string country_code_;
  std::string signed_in_email_;
  bool is_supervised_user_ = false;
  bool is_allowed_for_machine_learning_ = true;
  bool is_weblayer_ = false;
  version_info::Channel channel_ = version_info::Channel::UNKNOWN;
  bool msbb_enabled_ = true;
  bool uma_enabled_ = true;
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  std::unique_ptr<consent_auditor::FakeConsentAuditor> consent_auditor_ =
      std::make_unique<consent_auditor::FakeConsentAuditor>();
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_
