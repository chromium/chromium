// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace password_manager {

namespace {

std::vector<uint8_t> ProtobufBytesToVector(const std::string& bytes) {
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

}  // namespace

// static
std::vector<PasskeyCredential> PasskeyCredential::FromCredentialSpecifics(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys) {
  base::flat_set<std::string> shadowed_credential_ids;
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    for (const std::string& id : passkey.newly_shadowed_credential_ids()) {
      shadowed_credential_ids.emplace(id);
    }
  }
  std::vector<password_manager::PasskeyCredential> credentials;
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    if (shadowed_credential_ids.contains(passkey.credential_id())) {
      continue;
    }
    credentials.emplace_back(
        password_manager::PasskeyCredential::Source::kAndroidPhone,
        passkey.rp_id(), ProtobufBytesToVector(passkey.credential_id()),
        ProtobufBytesToVector(passkey.user_id()),
        passkey.has_user_name() ? passkey.user_name() : "",
        passkey.has_user_display_name() ? passkey.user_display_name() : "");
  }
  return credentials;
}

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
