// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_store/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFormManagerForUI : public PasswordFormManagerForUI {
 public:
  MockPasswordFormManagerForUI();

  MockPasswordFormManagerForUI(const MockPasswordFormManagerForUI&) = delete;
  MockPasswordFormManagerForUI& operator=(const MockPasswordFormManagerForUI&) =
      delete;

  ~MockPasswordFormManagerForUI() override;

  MOCK_METHOD(const GURL&, GetURL, (), (const override));
  MOCK_METHOD(base::span<const PasswordForm>,
              GetBestMatches,
              (),
              (const override));
  MOCK_METHOD((base::span<const PasswordForm>),
              GetFederatedMatches,
              (),
              (const override));
  MOCK_METHOD(const PasswordForm&, GetPendingCredentials, (), (const override));
  MOCK_METHOD(metrics_util::CredentialSourceType,
              GetCredentialSource,
              (),
              (const override));
  MOCK_METHOD(PasswordFormMetricsRecorder*, GetMetricsRecorder, (), (override));
  MOCK_METHOD(base::span<const InteractionsStats>,
              GetInteractionsStats,
              (),
              (const override));
  MOCK_METHOD((base::span<const PasswordForm>),
              GetInsecureCredentials,
              (),
              (const override));
  MOCK_METHOD(bool, IsBlocklisted, (), (const override));
  MOCK_METHOD(bool, IsMovableToAccountStore, (), (const override));
  MOCK_METHOD(void, Save, (), (override));
  MOCK_METHOD(bool,
              IsUpdateAffectingPasswordsStoredInTheGoogleAccount,
              (),
              (const override));
  MOCK_METHOD(void,
              OnUpdateUsernameFromPrompt,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void,
              OnUpdatePasswordFromPrompt,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void, OnNopeUpdateClicked, (), (override));
  MOCK_METHOD(void, OnNeverClicked, (), (override));
  MOCK_METHOD(void, OnNoInteraction, (bool), (override));
  MOCK_METHOD(void, Blocklist, (), (override));
  MOCK_METHOD(void, OnPasswordsRevealed, (), (override));
  MOCK_METHOD(void, MoveCredentialsToAccountStore, (), (override));
  MOCK_METHOD(void, BlockMovingCredentialsToAccountStore, (), (override));
  MOCK_METHOD(PasswordForm::Store,
              GetPasswordStoreForSaving,
              (const PasswordForm& password_form),
              (const override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
