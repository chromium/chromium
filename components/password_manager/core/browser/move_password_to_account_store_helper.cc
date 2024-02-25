// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/move_password_to_account_store_helper.h"

#include <utility>

#include "base/barrier_closure.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

MovePasswordToAccountStoreHelper::MovePasswordToAccountStoreHelper(
    const PasswordForm& form,
    PasswordManagerClient* client,
    metrics_util::MoveToAccountStoreTrigger trigger,
    base::OnceClosure done_callback)
    : form_(form),
      client_(client),
      trigger_(trigger),
      done_callback_(std::move(done_callback)),
      form_fetcher_(std::make_unique<FormFetcherImpl>(
          PasswordFormDigest(form),
          client,
          /*should_migrate_http_passwords=*/true)) {
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

MovePasswordToAccountStoreHelper::~MovePasswordToAccountStoreHelper() {
  form_fetcher_->RemoveConsumer(this);
}

void MovePasswordToAccountStoreHelper::OnFetchCompleted() {
  auto save_manager = std::make_unique<PasswordSaveManagerImpl>(client_);
  save_manager->Init(client_, form_fetcher_.get(), /*metrics_recorder=*/nullptr,
                     /*votes_uploader=*/nullptr);
  save_manager->CreatePendingCredentials(form_, {}, {}, /*is_http_auth=*/false,
                                         /*is_credential_api_save=*/false);
  save_manager->MoveCredentialsToAccountStore(trigger_);
  // `form_fetcher_` will be destroyed when `done_callback_` is run below but
  // must outlive `save_manager`. So destroy `save_manager` now.
  save_manager.reset();
  std::move(done_callback_).Run();
  // |this| might be deleted now!
}

}  // namespace password_manager
