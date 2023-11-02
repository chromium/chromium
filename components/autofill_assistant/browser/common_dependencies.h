// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_

#include <memory>
#include <string>

class PrefService;

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace content {
class WebContents;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace autofill_assistant {

class AnnotateDomModelService;
class AssistantFieldTrialUtil;

// Interface for platform delegates that provide dependencies to the starter.
//
// This interface contains all methods with a common implementation across
// platforms (desktop and Android) but a different implementation on WebLayer.
class CommonDependencies {
 public:
  virtual ~CommonDependencies();

  virtual std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil()
      const = 0;

  virtual std::string GetLocale() const = 0;

  virtual std::string GetLatestCountryCode() const = 0;

  virtual std::string GetStoredPermanentCountryCode() const = 0;

  virtual autofill::PersonalDataManager* GetPersonalDataManager() const = 0;

  virtual password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const = 0;

  virtual PrefService* GetPrefs() const = 0;

  virtual std::string GetSignedInEmail() const = 0;

  virtual bool IsSupervisedUser() const = 0;

  virtual bool IsAllowedForMachineLearning() const;

  virtual AnnotateDomModelService* GetOrCreateAnnotateDomModelService()
      const = 0;

  virtual bool IsWebLayer() const = 0;

  virtual signin::IdentityManager* GetIdentityManager() const = 0;

  virtual consent_auditor::ConsentAuditor* GetConsentAuditor() const = 0;

  virtual version_info::Channel GetChannel() const = 0;

  virtual bool GetMakeSearchesAndBrowsingBetterEnabled() const;

  virtual bool GetMetricsReportingEnabled() const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_
