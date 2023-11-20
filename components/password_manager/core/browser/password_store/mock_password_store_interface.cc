// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"

namespace password_manager {

MockPasswordStoreInterface::MockPasswordStoreInterface() = default;

MockPasswordStoreInterface::~MockPasswordStoreInterface() = default;

void MockPasswordStoreInterface::ShutdownOnUIThread() {}

}  // namespace password_manager
