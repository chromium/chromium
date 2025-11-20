// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_crypto_test_helpers.h"

#include <stddef.h>

#include <sstream>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/keypair.h"
#include "crypto/random.h"

namespace gcm {

bool CreateEncryptedPayloadForTesting(std::string_view payload,
                                      std::string_view peer_public_key,
                                      std::string_view auth_secret,
                                      IncomingMessage* message) {
  DCHECK(message);

  // Create an ephemeral key for the sender.
  auto key = crypto::keypair::PrivateKey::GenerateEcP256();

  std::string shared_secret;
  // Calculate the shared secret between the sender and its peer.
  if (!ComputeSharedP256Secret(key, peer_public_key, &shared_secret)) {
    return false;
  }

  // Generate a cryptographically secure random salt for the message.
  std::string salt(GCMMessageCryptographer::kSaltSize, '\0');
  crypto::RandBytes(base::as_writable_byte_span(salt));

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_03);

  size_t record_size;
  std::string ciphertext;

  std::vector<uint8_t> public_key = key.ToUncompressedX962Point();
  if (!cryptographer.Encrypt(peer_public_key, base::as_string_view(public_key),
                             shared_secret, auth_secret, salt, payload,
                             &record_size, &ciphertext)) {
    return false;
  }

  std::string encoded_salt, encoded_public_key;

  // Create base64url encoded representations of the salt and local public key.
  base::Base64UrlEncode(salt, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_salt);
  base::Base64UrlEncode(public_key, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  // Write the Encryption header value to |*message|.
  std::stringstream encryption_header;
  encryption_header << "salt=" << encoded_salt << ";rs=" << record_size;

  message->data["encryption"] = encryption_header.str();

  // Write the Crypto-Key value to |*message|.
  std::stringstream crypto_key_header;
  crypto_key_header << "dh=" << encoded_public_key;

  message->data["crypto-key"] = crypto_key_header.str();

  message->raw_data.swap(ciphertext);
  return true;
}

}  // namespace gcm
