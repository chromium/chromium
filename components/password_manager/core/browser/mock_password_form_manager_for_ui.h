// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFormManagerForUI : public PasswordFormManagerForUI {
 public:
  MockPasswordFormManagerForUI();
  ~MockPasswordFormManagerForUI() override;

  MOCK_METHOD(const GURL&, GetURL, (), (const override));
  MOCK_METHOD(const std::vector<const PasswordForm*>&,
              GetBestMatches,
              (),
              (const override));
  MOCK_METHOD(std::vector<const PasswordForm*>,
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
  MOCK_METHOD(base::span<const CompromisedCredentials>,
              GetCompromisedCredentials,
              (),
              (const override));
  MOCK_METHOD(bool, IsBlacklisted, (), (const override));
  MOCK_METHOD(bool, WasUnblacklisted, (), (const override));
  MOCK_METHOD(bool, IsMovableToAccountStore, (), (const override));
  MOCK_METHOD(void, Save, (), (override));
  MOCK_METHOD(void, Update, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              OnUpdateUsernameFromPrompt,
              (const base::string16&),
              (override));
  MOCK_METHOD(void,
              OnUpdatePasswordFromPrompt,
              (const base::string16&),
              (override));
  MOCK_METHOD(void, OnNopeUpdateClicked, (), (override));
  MOCK_METHOD(void, OnNeverClicked, (), (override));
  MOCK_METHOD(void, OnNoInteraction, (bool), (override));
  MOCK_METHOD(void, PermanentlyBlacklist, (), (override));
  MOCK_METHOD(void, OnPasswordsRevealed, (), (override));
  MOCK_METHOD(void, MoveCredentialsToAccountStore, (), (override));
  MOCK_METHOD(void, BlockMovingCredentialsToAccountStore, (), (override));

  DISALLOW_COPY_AND_ASSIGN(MockPasswordFormManagerForUI);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
