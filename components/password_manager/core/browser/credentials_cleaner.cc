// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credentials_cleaner.h"

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"

namespace password_manager {

// static
std::vector<std::unique_ptr<PasswordForm>>
CredentialsCleaner::RemoveNonHTTPOrHTTPSForms(
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  std::erase_if(forms, [](const auto& form) {
    return !GURL(form->signon_realm).SchemeIsHTTPOrHTTPS();
  });

  return forms;
}

}  // namespace password_manager
