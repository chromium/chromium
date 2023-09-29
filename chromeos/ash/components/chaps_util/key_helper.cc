// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/key_helper.h"

namespace chromeos {

crypto::ScopedSECItem MakeIdFromPubKeyNss(
    std::vector<CK_BYTE>& public_key_bytes) {
  SECItem secitem_modulus;
  secitem_modulus.data = public_key_bytes.data();
  secitem_modulus.len = public_key_bytes.size();
  return crypto::ScopedSECItem(PK11_MakeIDFromPubKey(&secitem_modulus));
}

std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id) {
  return std::vector<uint8_t>(id->data, id->data + id->len);
}

}  // namespace chromeos
