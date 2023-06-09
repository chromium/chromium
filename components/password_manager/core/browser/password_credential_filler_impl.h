// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_

#include <string>
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

class PasswordCredentialFillerImpl : public PasswordCredentialFiller {
 public:
  PasswordCredentialFillerImpl(base::WeakPtr<PasswordManagerDriver> driver,
                               SubmissionReadinessState submission_readiness);
  PasswordCredentialFillerImpl(const PasswordCredentialFillerImpl&) = delete;
  PasswordCredentialFillerImpl& operator=(const PasswordCredentialFillerImpl&) =
      delete;
  ~PasswordCredentialFillerImpl() override;

  bool IsReadyToFill() override;

  void FillUsernameAndPassword(const std::u16string& username,
                               const std::u16string& password) override;

  void UpdateTriggerSubmission(bool new_value) override;

  bool ShouldTriggerSubmission() const override;

  SubmissionReadinessState GetSubmissionReadinessState() const override;

  const GURL& GetFrameUrl() const override;

  void CleanUp(ToShowVirtualKeyboard should_show) override;

 private:
  // Driver supplied by the client. Gets cleared when
  // FillUsernameAndPassword() or CleanUp() gets called.
  base::WeakPtr<PasswordManagerDriver> driver_;

  // Readiness state supplied by the client, used to compute
  // trigger_submission_.
  SubmissionReadinessState submission_readiness_;

  // Whether the controller should trigger submission when a credential is
  // filled in.
  bool trigger_submission_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_IMPL_H_
