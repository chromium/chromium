// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"

namespace password_manager {

MockPasswordStoreBackend::MockPasswordStoreBackend() = default;

MockPasswordStoreBackend::~MockPasswordStoreBackend() = default;

base::WeakPtr<PasswordStoreBackend> MockPasswordStoreBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
