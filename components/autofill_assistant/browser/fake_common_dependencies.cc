// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_common_dependencies.h"

namespace autofill_assistant {

FakeCommonDependencies::FakeCommonDependencies() = default;
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

autofill::PersonalDataManager* FakeCommonDependencies::GetPersonalDataManager(
    content::BrowserContext* browser_context) const {
  return nullptr;
}

password_manager::PasswordManagerClient*
FakeCommonDependencies::GetPasswordManagerClient(
    content::WebContents* web_contents) const {
  return nullptr;
}

std::string FakeCommonDependencies::GetSignedInEmail(
    content::BrowserContext* browser_context) const {
  return signed_in_email_;
}

bool FakeCommonDependencies::IsSupervisedUser(
    content::BrowserContext* browser_context) const {
  return is_supervised_user_;
}

bool FakeCommonDependencies::IsAllowedForMachineLearning(
    content::BrowserContext* browser_context) const {
  return is_allowed_for_machine_learning_;
}

AnnotateDomModelService*
FakeCommonDependencies::GetOrCreateAnnotateDomModelService(
    content::BrowserContext* browser_context) const {
  return nullptr;
}

bool FakeCommonDependencies::IsWebLayer() const {
  return is_weblayer_;
}

signin::IdentityManager* FakeCommonDependencies::GetIdentityManager(
    content::BrowserContext* browser_context) const {
  return nullptr;
}

version_info::Channel FakeCommonDependencies::GetChannel() const {
  return channel_;
}

bool FakeCommonDependencies::GetMakeSearchesAndBrowsingBetterEnabled(
    content::BrowserContext* browser_context) const {
  return msbb_enabled_;
}

bool FakeCommonDependencies::GetMetricsReportingEnabled(
    content::BrowserContext* browser_context) const {
  return uma_enabled_;
}

}  // namespace autofill_assistant
