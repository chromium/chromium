// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include <string>
#include <vector>

#include "components/strings/grit/components_strings.h"

namespace password_manager {

PasskeyCredential::PasskeyCredential(Source source,
                                     std::string rp_id,
                                     std::vector<uint8_t> credential_id,
                                     std::vector<uint8_t> user_id,
                                     std::string username,
                                     std::string display_name)
    : source_(source),
      rp_id_(std::move(rp_id)),
      credential_id_(std::move(credential_id)),
      user_id_(std::move(user_id)),
      username_(std::move(username)),
      display_name_(std::move(display_name)) {}

PasskeyCredential::~PasskeyCredential() = default;

PasskeyCredential::PasskeyCredential(const PasskeyCredential&) = default;
PasskeyCredential& PasskeyCredential::operator=(const PasskeyCredential&) =
    default;

PasskeyCredential::PasskeyCredential(PasskeyCredential&&) = default;
PasskeyCredential& PasskeyCredential::operator=(PasskeyCredential&&) = default;

int PasskeyCredential::GetAuthenticatorLabel() const {
  switch (source_) {
    case Source::kWindowsHello:
      return IDS_PASSWORD_MANAGER_USE_WINDOWS_HELLO;
    case Source::kTouchId:
      return IDS_PASSWORD_MANAGER_USE_TOUCH_ID;
    case Source::kAndroidPhone:
      return IDS_PASSWORD_MANAGER_USE_SCREEN_LOCK;
    case Source::kOther:
      return IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE;
  }
}

bool operator==(const PasskeyCredential& lhs,
                const PasskeyCredential& rhs) = default;

}  // namespace password_manager
