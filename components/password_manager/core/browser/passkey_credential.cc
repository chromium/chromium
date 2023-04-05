// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

std::u16string ToUsernameString(const PasskeyCredential::Username& username) {
  if (username.value() && !username.value()->empty()) {
    return base::UTF8ToUTF16(*username.value());
  }
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

}  // namespace

PasskeyCredential::PasskeyCredential(const Username& username,
                                     const DeviceName& device_name,
                                     const BackendId& backend_id)
    : username_(ToUsernameString(username)),
      device_name_(device_name),
      backend_id_(backend_id) {}

PasskeyCredential::~PasskeyCredential() = default;

PasskeyCredential::PasskeyCredential(const PasskeyCredential&) = default;
PasskeyCredential& PasskeyCredential::operator=(const PasskeyCredential&) =
    default;

PasskeyCredential::PasskeyCredential(PasskeyCredential&&) = default;
PasskeyCredential& PasskeyCredential::operator=(PasskeyCredential&&) = default;

bool operator==(const PasskeyCredential& lhs,
                const PasskeyCredential& rhs) = default;

}  // namespace password_manager
