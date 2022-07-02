// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_

#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "testing/gmock/include/gmock/gmock.h"

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
  MOCK_METHOD(std::string, GetCountryCode, (), (const override));
  MOCK_METHOD(autofill::PersonalDataManager*,
              GetPersonalDataManager,
              (content::BrowserContext*),
              (const override));
  MOCK_METHOD(password_manager::PasswordManagerClient*,
              GetPasswordManagerClient,
              (content::WebContents*),
              (const override));
  MOCK_METHOD(std::string,
              GetSignedInEmail,
              (content::BrowserContext*),
              (const override));
  MOCK_METHOD(bool,
              IsSupervisedUser,
              (content::BrowserContext*),
              (const override));
  MOCK_METHOD(AnnotateDomModelService*,
              GetOrCreateAnnotateDomModelService,
              (content::BrowserContext*),
              (const override));
  MOCK_METHOD(bool, IsWebLayer, (), (const override));
  MOCK_METHOD(signin::IdentityManager*,
              GetIdentityManager,
              (content::BrowserContext*),
              (const override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
};

}  // namespace autofill_assistant

#endif  // #COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_COMMON_DEPENDENCIES_H_
