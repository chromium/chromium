// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

struct PasswordFillingParams;

class PasswordCredentialFillerImpl final : public PasswordCredentialFiller {
 public:
  PasswordCredentialFillerImpl(
      base::WeakPtr<PasswordManagerDriver> driver,
      const PasswordFillingParams& password_filling_params);
  PasswordCredentialFillerImpl(const PasswordCredentialFillerImpl&) = delete;
  PasswordCredentialFillerImpl& operator=(const PasswordCredentialFillerImpl&) =
      delete;
  ~PasswordCredentialFillerImpl() override;

  void FillUsernameAndPassword(const std::u16string& username,
                               const std::u16string& password) override;

  void UpdateTriggerSubmission(bool new_value) override;

  bool ShouldTriggerSubmission() const override;

  SubmissionReadinessState GetSubmissionReadinessState() const override;

  GURL GetFrameUrl() const override;

  void Dismiss(ToShowVirtualKeyboard should_show) override;

  base::WeakPtr<PasswordCredentialFiller> AsWeakPtr() override;

 private:
  // Driver supplied by the client.
  base::WeakPtr<PasswordManagerDriver> driver_;

  // Readiness state supplied by the client, used to compute
  // trigger_submission_.
  SubmissionReadinessState submission_readiness_;

  // Whether the controller should trigger submission when a credential is
  // filled in.
  bool trigger_submission_;

  base::WeakPtrFactory<PasswordCredentialFillerImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_
