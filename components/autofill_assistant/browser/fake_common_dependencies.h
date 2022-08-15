// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_

#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/version_info/channel.h"

namespace autofill_assistant {

class FakeCommonDependencies : public CommonDependencies {
 public:
  FakeCommonDependencies();
  ~FakeCommonDependencies() override;

  // From CommonDependencies:
  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const override;
  std::string GetLocale() const override;
  std::string GetCountryCode() const override;
  autofill::PersonalDataManager* GetPersonalDataManager(
      content::BrowserContext* browser_context) const override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const override;
  std::string GetSignedInEmail(
      content::BrowserContext* browser_context) const override;
  bool IsSupervisedUser(
      content::BrowserContext* browser_context) const override;
  bool IsAllowedForMachineLearning(
      content::BrowserContext* browser_context) const override;
  AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const override;
  bool IsWebLayer() const override;
  signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* browser_context) const override;
  version_info::Channel GetChannel() const override;
  bool GetMakeSearchesAndBrowsingBetterEnabled(
      content::BrowserContext* browser_context) const override;
  bool GetMetricsReportingEnabled(
      content::BrowserContext* browser_context) const override;

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
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_COMMON_DEPENDENCIES_H_
