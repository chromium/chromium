// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "url/gurl.h"

namespace password_manager {

// PasswordCredentialFiller class is responsible for filling (username,
// passwords) using a PasswordManagerDriver. It can also submit the form.
class PasswordCredentialFiller {
 public:
  using SubmissionReadinessState = autofill::mojom::SubmissionReadinessState;
  using ToShowVirtualKeyboard = PasswordManagerDriver::ToShowVirtualKeyboard;

  // The destructor of the implementation should make sure the class is cleaned
  // (i.e. by checking !IsReadyToFill())
  virtual ~PasswordCredentialFiller() = default;

  // Fills the given username and password to the form. It can also submit the
  // form if signaled.
  virtual void FillUsernameAndPassword(const std::u16string& username,
                                       const std::u16string& password) = 0;

  // Instructs the filler to submit the form or not.
  virtual void UpdateTriggerSubmission(bool new_value) = 0;

  // Returns whether the filler should submit the form or not.
  virtual bool ShouldTriggerSubmission() const = 0;

  // Returns the SubmissionReadinessState this filler created with.
  virtual SubmissionReadinessState GetSubmissionReadinessState() const = 0;

  // Returns the frame URL this filler is interacting with.
  virtual GURL GetFrameUrl() const = 0;

  // Cleans up the filler and shows the virtual keyboard depending on
  // `should_show`.
  virtual void Dismiss(ToShowVirtualKeyboard should_show) = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<PasswordCredentialFiller> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CREDENTIAL_FILLER_H_
