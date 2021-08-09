// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_

#include <memory>

#include "base/callback.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

class PasswordManagerClient;

// Used for moving a form from the profile store to the account store.
class MovePasswordToAccountStoreHelper : public FormFetcher::Consumer {
 public:
  // Starts moving |form|. |done_callback| is run when done.
  MovePasswordToAccountStoreHelper(const PasswordForm& form,
                                   PasswordManagerClient* client,
                                   base::OnceClosure done_callback);
  ~MovePasswordToAccountStoreHelper() override;

 private:
  // FormFetcher::Consumer.
  void OnFetchCompleted() override;

  PasswordForm form_;
  PasswordManagerClient* const client_;
  base::OnceClosure done_callback_;
  std::unique_ptr<FormFetcher> form_fetcher_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_
