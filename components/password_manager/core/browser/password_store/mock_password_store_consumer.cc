// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/mock_password_store_consumer.h"

#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

MockPasswordStoreConsumer::MockPasswordStoreConsumer() = default;

MockPasswordStoreConsumer::~MockPasswordStoreConsumer() = default;

base::WeakPtr<PasswordStoreConsumer> MockPasswordStoreConsumer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void MockPasswordStoreConsumer::CancelAllRequests() {
  cancelable_task_tracker()->TryCancelAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace password_manager
