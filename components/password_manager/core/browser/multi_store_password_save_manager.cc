// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"

namespace password_manager {

MultiStorePasswordSaveManager::MultiStorePasswordSaveManager(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : PasswordSaveManagerImpl(std::move(profile_form_saver),
                              std::move(account_form_saver)) {}

MultiStorePasswordSaveManager::~MultiStorePasswordSaveManager() = default;

std::unique_ptr<PasswordSaveManager> MultiStorePasswordSaveManager::Clone() {
  auto result = std::make_unique<MultiStorePasswordSaveManager>(
      profile_store_form_saver_->Clone(),
      account_store_form_saver_ ? account_store_form_saver_->Clone() : nullptr);
  CloneInto(result.get());
  return result;
}

}  // namespace password_manager
