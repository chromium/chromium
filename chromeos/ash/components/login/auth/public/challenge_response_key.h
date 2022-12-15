// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CHALLENGE_RESPONSE_KEY_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CHALLENGE_RESPONSE_KEY_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace ash {

// This class contains information about a challenge-response key for user
// authentication. This includes information about the public key of the
// cryptographic key to be challenged, the signature algorithms supported for
// the challenge, and the id of the extension that handles the
// challenge-response.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    ChallengeResponseKey {
 public:
  // Cryptographic signature algorithm type for challenge requests.
  enum class SignatureAlgorithm {
    kRsassaPkcs1V15Sha1,
    kRsassaPkcs1V15Sha256,
    kRsassaPkcs1V15Sha384,
    kRsassaPkcs1V15Sha512,
  };

  ChallengeResponseKey();
  ChallengeResponseKey(const ChallengeResponseKey& other);
  ~ChallengeResponseKey();

  bool operator==(const ChallengeResponseKey& other) const;
  bool operator!=(const ChallengeResponseKey& other) const;

  // Getter and setter for the DER-encoded blob of the X.509 Subject Public Key
  // Info.
  const std::string& public_key_spki_der() const {
    return public_key_spki_der_;
  }
  void set_public_key_spki_der(const std::string& public_key_spki_der) {
    public_key_spki_der_ = public_key_spki_der;
  }

  // Getter and setter for the list of supported signature algorithms, in the
  // order of preference (starting from the most preferred). Absence of this
  // field denotes that the key cannot be used for signing.
  const std::vector<SignatureAlgorithm>& signature_algorithms() const {
    return signature_algorithms_;
  }
  void set_signature_algorithms(
      const std::vector<SignatureAlgorithm>& signature_algorithms) {
    signature_algorithms_ = signature_algorithms;
  }

  // Getter and setter for the id of the extension that is used to sign the key.
  const std::string& extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

 private:
  std::string public_key_spki_der_;
  std::vector<SignatureAlgorithm> signature_algorithms_;
  std::string extension_id_;
};

// Contains the parts of a ChallengeResponseKey that can be persisted to disk.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    DeserializedChallengeResponseKey {
  std::string public_key_spki_der;
  std::string extension_id;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_CHALLENGE_RESPONSE_KEY_H_
