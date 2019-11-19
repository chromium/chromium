// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_form_saver.h"

namespace password_manager {

autofill::PasswordForm StubFormSaver::PermanentlyBlacklist(
    PasswordStore::FormDigest digest) {
  return autofill::PasswordForm();
}

void StubFormSaver::Unblacklist(const PasswordStore::FormDigest& digest) {}

std::unique_ptr<FormSaver> StubFormSaver::Clone() {
  return nullptr;
}

}  // namespace password_manager
