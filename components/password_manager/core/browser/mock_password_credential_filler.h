// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CREDENTIAL_FILLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordCredentialFiller final : public PasswordCredentialFiller {
 public:
  MockPasswordCredentialFiller();
  ~MockPasswordCredentialFiller() override;

  MOCK_METHOD(void,
              FillUsernameAndPassword,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, UpdateTriggerSubmission, (bool), (override));
  MOCK_METHOD(bool, ShouldTriggerSubmission, (), (const override));
  MOCK_METHOD(SubmissionReadinessState,
              GetSubmissionReadinessState,
              (),
              (const override));
  MOCK_METHOD(base::WeakPtr<password_manager::PasswordManagerDriver>,
              GetDriver,
              (),
              (const override));
  MOCK_METHOD(GURL, GetFrameUrl, (), (const override));
  MOCK_METHOD(void, Dismiss, (ToShowVirtualKeyboard), (override));

  base::WeakPtr<PasswordCredentialFiller> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockPasswordCredentialFiller> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_CREDENTIAL_FILLER_H_
