// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_H_

#include "components/password_manager/core/browser/password_manager_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {
class MockPasswordManager : public password_manager::PasswordManagerInterface {
 public:
  MockPasswordManager();
  ~MockPasswordManager() override;

  MockPasswordManager(const MockPasswordManager&) = delete;
  MockPasswordManager& operator=(const MockPasswordManager&) = delete;

  // FormSubmissionObserver:
  MOCK_METHOD(void,
              DidNavigateMainFrame,
              (bool form_may_be_submitted),
              (override));

  // PasswordManagerInterface:
  MOCK_METHOD(void,
              OnPasswordFormsParsed,
              (PasswordManagerDriver*, const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormsRendered,
              (PasswordManagerDriver*, const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmitted,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormCleared,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              SetGenerationElementAndTypeForForm,
              (PasswordManagerDriver*,
               autofill::FormRendererId,
               autofill::FieldRendererId,
               autofill::password_generation::PasswordGenerationType),
              (override));
  MOCK_METHOD(void,
              OnPresaveGeneratedPassword,
              (PasswordManagerDriver*,
               const autofill::FormData&,
               const std::u16string&),
              (override));
  MOCK_METHOD(
      void,
      ProcessAutofillPredictions,
      (PasswordManagerDriver*,
       const autofill::FormData&,
       (const base::flat_map<autofill::FieldGlobalId,
                             autofill::AutofillType::ServerPrediction>&)),
      (override));
  MOCK_METHOD(PasswordManagerClient*, GetClient, (), (override));
#if BUILDFLAG(IS_IOS)
  MOCK_METHOD(void,
              OnSubframeFormSubmission,
              (PasswordManagerDriver*, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              UpdateStateOnUserInput,
              (password_manager::PasswordManagerDriver*,
               std::optional<autofill::FormRendererId>,
               autofill::FieldRendererId,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, OnPasswordNoLongerGenerated, (), (override));
  MOCK_METHOD(void,
              OnPasswordFormsRemoved,
              (PasswordManagerDriver*,
               const autofill::FieldDataManager&,
               const std::set<autofill::FormRendererId>&,
               const std::set<autofill::FieldRendererId>&),
              (override));
  MOCK_METHOD(void,
              OnIframeDetach,
              (const std::string&,
               PasswordManagerDriver*,
               const autofill::FieldDataManager&),
              (override));
  MOCK_METHOD(void,
              PropagateFieldDataManagerInfo,
              (const autofill::FieldDataManager&, const PasswordManagerDriver*),
              (override));
#endif
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_H_
