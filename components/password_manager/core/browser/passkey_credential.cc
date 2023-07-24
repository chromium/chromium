// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/passkey_credential.h"

#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "base/containers/span.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace password_manager {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace {

std::vector<uint8_t> ProtobufBytesToVector(const std::string& bytes) {
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

}  // namespace

// static
std::vector<PasskeyCredential> PasskeyCredential::FromCredentialSpecifics(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys) {
  std::vector<sync_pb::WebauthnCredentialSpecifics> filtered =
      webauthn::passkey_model_utils::FilterShadowedCredentials(passkeys);
  std::vector<password_manager::PasskeyCredential> ret;
  ret.reserve(filtered.size());
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : filtered) {
    ret.emplace_back(
        password_manager::PasskeyCredential::Source::kAndroidPhone,
        RpId(passkey.rp_id()),
        CredentialId(ProtobufBytesToVector(passkey.credential_id())),
        UserId(ProtobufBytesToVector(passkey.user_id())),
        Username(passkey.has_user_name() ? passkey.user_name() : ""),
        DisplayName(passkey.has_user_display_name()
                        ? passkey.user_display_name()
                        : ""));
  }
  return ret;
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

PasskeyCredential::PasskeyCredential(Source source,
                                     RpId rp_id,
                                     CredentialId credential_id,
                                     UserId user_id,
                                     Username username,
                                     DisplayName display_name)
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

std::u16string PasskeyCredential::GetAuthenticatorLabel() const {
  if (authenticator_label_) {
    return *authenticator_label_;
  }
  int id;
  switch (source_) {
    case Source::kWindowsHello:
      id = IDS_PASSWORD_MANAGER_USE_WINDOWS_HELLO;
      break;
    case Source::kTouchId:
      id = IDS_PASSWORD_MANAGER_USE_TOUCH_ID;
      break;
    case Source::kAndroidPhone:
      id = IDS_PASSWORD_MANAGER_USE_SCREEN_LOCK;
      break;
    case Source::kOther:
      id = IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

bool operator==(const PasskeyCredential& lhs,
                const PasskeyCredential& rhs) = default;

}  // namespace password_manager
