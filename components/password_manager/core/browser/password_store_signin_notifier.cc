// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_signin_notifier.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

PasswordStoreSigninNotifier::PasswordStoreSigninNotifier() {}

PasswordStoreSigninNotifier::~PasswordStoreSigninNotifier() {}

void PasswordStoreSigninNotifier::NotifySignedOut(const std::string& username,
                                                  bool primary_account) {
  if (!store_)
    return;

  if (primary_account) {
    metrics_util::LogGaiaPasswordHashChange(
        metrics_util::GaiaPasswordHashChange::CLEARED_ON_CHROME_SIGNOUT,
        /*is_sync_password=*/true);
    store_->ClearAllGaiaPasswordHash();
  } else {
    store_->ClearGaiaPasswordHash(username);
  }
}

}  // namespace password_manager
