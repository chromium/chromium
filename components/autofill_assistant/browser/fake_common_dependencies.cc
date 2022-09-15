// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_common_dependencies.h"

namespace autofill_assistant {

FakeCommonDependencies::FakeCommonDependencies(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}
FakeCommonDependencies::~FakeCommonDependencies() = default;

std::unique_ptr<AssistantFieldTrialUtil>
FakeCommonDependencies::CreateFieldTrialUtil() const {
  return nullptr;
}

std::string FakeCommonDependencies::GetLocale() const {
  return locale_;
}

std::string FakeCommonDependencies::GetCountryCode() const {
  return country_code_;
}

autofill::PersonalDataManager* FakeCommonDependencies::GetPersonalDataManager()
    const {
  return nullptr;
}

password_manager::PasswordManagerClient*
FakeCommonDependencies::GetPasswordManagerClient(
    content::WebContents* web_contents) const {
  return nullptr;
}

PrefService* FakeCommonDependencies::GetPrefs() const {
  return nullptr;
}

std::string FakeCommonDependencies::GetSignedInEmail() const {
  return signed_in_email_;
}

bool FakeCommonDependencies::IsSupervisedUser() const {
  return is_supervised_user_;
}

bool FakeCommonDependencies::IsAllowedForMachineLearning() const {
  return is_allowed_for_machine_learning_;
}

AnnotateDomModelService*
FakeCommonDependencies::GetOrCreateAnnotateDomModelService() const {
  return nullptr;
}

bool FakeCommonDependencies::IsWebLayer() const {
  return is_weblayer_;
}

signin::IdentityManager* FakeCommonDependencies::GetIdentityManager() const {
  return identity_manager_;
}

consent_auditor::ConsentAuditor* FakeCommonDependencies::GetConsentAuditor()
    const {
  return consent_auditor_.get();
}

version_info::Channel FakeCommonDependencies::GetChannel() const {
  return channel_;
}

bool FakeCommonDependencies::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return msbb_enabled_;
}

bool FakeCommonDependencies::GetMetricsReportingEnabled() const {
  return uma_enabled_;
}

}  // namespace autofill_assistant
