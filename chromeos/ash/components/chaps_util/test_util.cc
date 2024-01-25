// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/test_util.h"

#include <pk11pub.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"

namespace chromeos {

FakeChapsUtil::FakeChapsUtil(OnKeyGenerated on_key_generated)
    : on_key_generated_(on_key_generated) {}
FakeChapsUtil::~FakeChapsUtil() = default;

bool FakeChapsUtil::GenerateSoftwareBackedRSAKey(
    PK11SlotInfo* slot,
    uint16_t num_bits,
    crypto::ScopedSECKEYPublicKey* out_public_key,
    crypto::ScopedSECKEYPrivateKey* out_private_key) {
  if (!crypto::GenerateRSAKeyPairNSS(slot, num_bits, /*permanent=*/true,
                                     out_public_key, out_private_key)) {
    return false;
  }
  crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(out_public_key->get()));
  on_key_generated_.Run(std::string(
      reinterpret_cast<const char*>(spki_der->data), spki_der->len));
  return true;
}

ScopedChapsUtilOverride::ScopedChapsUtilOverride() {
  ChapsUtil::SetFactoryForTesting(base::BindRepeating(
      &ScopedChapsUtilOverride::CreateChapsUtil, base::Unretained(this)));
}

ScopedChapsUtilOverride::~ScopedChapsUtilOverride() {
  ChapsUtil::SetFactoryForTesting(ChapsUtil::FactoryCallback());
}

std::unique_ptr<ChapsUtil> ScopedChapsUtilOverride::CreateChapsUtil() {
  return std::make_unique<FakeChapsUtil>(
      base::BindRepeating(&ScopedChapsUtilOverride::OnKeyGenerated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ScopedChapsUtilOverride::OnKeyGenerated(const std::string& spki) {
  generated_key_spkis_.push_back(spki);
}

}  // namespace chromeos
