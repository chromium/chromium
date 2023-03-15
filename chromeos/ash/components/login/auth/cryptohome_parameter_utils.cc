// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"

#include "base/check_op.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {
namespace cryptohome_parameter_utils {

using ::cryptohome::KeyDefinition;
using ::cryptohome::KeyLabel;

KeyDefinition CreateKeyDefFromUserContext(const UserContext& user_context) {
  if (!user_context.GetChallengeResponseKeys().empty()) {
    // The case of challenge-response keys. No secret is passed, only public-key
    // information.
    return KeyDefinition::CreateForChallengeResponse(
        user_context.GetChallengeResponseKeys(),
        KeyLabel(GenerateChallengeResponseKeyLabel(
            user_context.GetChallengeResponseKeys())),
        cryptohome::PRIV_DEFAULT);
  }

  // The case of a password or a PIN.
  const Key* key = user_context.GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  return KeyDefinition::CreateForPassword(
      key->GetSecret(), KeyLabel(key->GetLabel()), cryptohome::PRIV_DEFAULT);
}

KeyDefinition CreateAuthorizationKeyDefFromUserContext(
    const UserContext& user_context) {
  KeyDefinition key_def = CreateKeyDefFromUserContext(user_context);

  // Don't set the authorization's key label, implicitly setting it to an empty
  // string, which is a wildcard allowing any key to match. This is necessary
  // because cryptohomes created by Chrome OS M38 and older will have a legacy
  // key with no label while those created by Chrome OS M39 and newer will have
  // a key with the label kCryptohomeGaiaKeyLabel.
  //
  // This logic does not apply to challenge-response, PIN and weak keys in
  // general, as those do not authenticate against a wildcard label.
  switch (key_def.type) {
    case KeyDefinition::TYPE_PASSWORD:
      if (!user_context.IsUsingPin())
        key_def.label.value().clear();
      break;
    case KeyDefinition::TYPE_CHALLENGE_RESPONSE:
      break;
    case KeyDefinition::TYPE_PUBLIC_MOUNT:
      break;
  }

  return key_def;
}

}  // namespace cryptohome_parameter_utils
}  // namespace ash
