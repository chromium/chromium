// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_form_saver.h"

namespace password_manager {

PasswordForm StubFormSaver::Blocklist(PasswordFormDigest digest) {
  return PasswordForm();
}

void StubFormSaver::Unblocklist(const PasswordFormDigest& digest) {}

std::unique_ptr<FormSaver> StubFormSaver::Clone() {
  return nullptr;
}

}  // namespace password_manager
