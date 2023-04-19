// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace password_manager {

class PasswordManagerClient;

// Used for moving a form from the profile store to the account store.
class MovePasswordToAccountStoreHelper : public FormFetcher::Consumer {
 public:
  // Starts moving |form|. |done_callback| is run when done.
  MovePasswordToAccountStoreHelper(
      const PasswordForm& form,
      PasswordManagerClient* client,
      metrics_util::MoveToAccountStoreTrigger trigger,
      base::OnceClosure done_callback);
  ~MovePasswordToAccountStoreHelper() override;

 private:
  // FormFetcher::Consumer.
  void OnFetchCompleted() override;

  PasswordForm form_;
  const raw_ptr<PasswordManagerClient> client_;
  const metrics_util::MoveToAccountStoreTrigger trigger_;
  base::OnceClosure done_callback_;
  std::unique_ptr<FormFetcher> form_fetcher_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOVE_PASSWORD_TO_ACCOUNT_STORE_HELPER_H_
