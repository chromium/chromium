// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"

#include <string>
#include <vector>

#include "components/sync/engine/nigori/nigori.h"

namespace syncer {

crosapi::mojom::NigoriKeyPtr NigoriToMojo(const Nigori& nigori) {
  std::string deprecated_user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&deprecated_user_key, &encryption_key, &mac_key);

  crosapi::mojom::NigoriKeyPtr mojo_result = crosapi::mojom::NigoriKey::New();
  mojo_result->encryption_key =
      std::vector<uint8_t>(encryption_key.begin(), encryption_key.end());
  mojo_result->mac_key = std::vector<uint8_t>(mac_key.begin(), mac_key.end());
  return mojo_result;
}

std::unique_ptr<Nigori> NigoriFromMojo(
    const crosapi::mojom::NigoriKey& mojo_nigori_key) {
  const std::string encryption_key(mojo_nigori_key.encryption_key.begin(),
                                   mojo_nigori_key.encryption_key.end());
  const std::string mac_key(mojo_nigori_key.mac_key.begin(),
                            mojo_nigori_key.mac_key.end());
  // |user_key| is deprecated, it's safe to pass empty string.
  return Nigori::CreateByImport(
      /*user_key=*/std::string(), encryption_key, mac_key);
}

}  // namespace syncer
