// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_

#include <memory>
#include <string>

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

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

  virtual std::string GetCountryCode() const = 0;

  virtual autofill::PersonalDataManager* GetPersonalDataManager(
      content::BrowserContext* browser_context) const = 0;

  virtual password_manager::PasswordManagerClient* GetPasswordManagerClient(
      content::WebContents* web_contents) const = 0;

  virtual std::string GetSignedInEmail(
      content::BrowserContext* browser_context) const = 0;

  virtual bool IsSupervisedUser(
      content::BrowserContext* browser_context) const = 0;

  virtual bool IsAllowedForMachineLearning(
      content::BrowserContext* browser_context) const;

  virtual AnnotateDomModelService* GetOrCreateAnnotateDomModelService(
      content::BrowserContext* browser_context) const = 0;

  virtual bool IsWebLayer() const = 0;

  virtual signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* browser_context) const = 0;

  virtual version_info::Channel GetChannel() const = 0;

  virtual bool GetMakeSearchesAndBrowsingBetterEnabled(
      content::BrowserContext* browser_context) const;

  virtual bool GetMetricsReportingEnabled(
      content::BrowserContext* browser_context) const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_COMMON_DEPENDENCIES_H_
