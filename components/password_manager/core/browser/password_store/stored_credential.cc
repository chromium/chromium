// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

StoredCredential::StoredCredential() = default;

StoredCredential::StoredCredential(StoredCredential&&) = default;

StoredCredential& StoredCredential::operator=(StoredCredential&&) = default;

StoredCredential::~StoredCredential() = default;

}  // namespace password_manager
