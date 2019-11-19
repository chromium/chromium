// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFormManagerForUI : public PasswordFormManagerForUI {
 public:
  MockPasswordFormManagerForUI();
  ~MockPasswordFormManagerForUI() override;

  MOCK_CONST_METHOD0(GetOrigin, const GURL&());
  MOCK_CONST_METHOD0(GetBestMatches,
                     const std::vector<const autofill::PasswordForm*>&());
  MOCK_CONST_METHOD0(GetFederatedMatches,
                     std::vector<const autofill::PasswordForm*>());
  MOCK_CONST_METHOD0(GetPendingCredentials, const autofill::PasswordForm&());
  MOCK_CONST_METHOD0(GetCredentialSource, metrics_util::CredentialSourceType());
  MOCK_METHOD0(GetMetricsRecorder, PasswordFormMetricsRecorder*());
  MOCK_CONST_METHOD0(GetInteractionsStats,
                     base::span<const InteractionsStats>());
  MOCK_CONST_METHOD0(IsBlacklisted, bool());
  MOCK_METHOD0(Save, void());
  MOCK_METHOD1(Update,
               void(const autofill::PasswordForm& credentials_to_update));
  MOCK_METHOD1(OnUpdateUsernameFromPrompt,
               void(const base::string16& new_username));
  MOCK_METHOD1(OnUpdatePasswordFromPrompt,
               void(const base::string16& new_password));
  MOCK_METHOD0(OnNopeUpdateClicked, void());
  MOCK_METHOD0(OnNeverClicked, void());
  MOCK_METHOD1(OnNoInteraction, void(bool));
  MOCK_METHOD0(PermanentlyBlacklist, void());
  MOCK_METHOD0(OnPasswordsRevealed, void());

  DISALLOW_COPY_AND_ASSIGN(MockPasswordFormManagerForUI);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FORM_MANAGER_FOR_UI_H_
