// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <array>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_view_util.h"
#include "crypto/kex.h"
#include "crypto/keypair.h"

namespace gcm {

bool ComputeSharedP256Secret(crypto::keypair::PrivateKey our_key,
                             std::string_view their_point,
                             std::string* out_shared_secret) {
  if (!our_key.IsEcP256()) {
    DLOG(ERROR) << "The private key is invalid.";
    return false;
  }

  std::optional<crypto::keypair::PublicKey> their_key =
      crypto::keypair::PublicKey::FromEcP256Point(
          base::as_byte_span(their_point));
  if (!their_key.has_value()) {
    DLOG(ERROR) << "Can't convert peer public value to curve point.";
    return false;
  }

  std::array<uint8_t, 32> result;
  crypto::kex::EcdhP256(*their_key, our_key, result);

  out_shared_secret->assign(base::as_string_view(result));
  return true;
}

}  // namespace gcm
