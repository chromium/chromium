// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CHROMEOS_EXPLICIT_PASSPHRASE_MOJO_UTILS_H_
#define COMPONENTS_SYNC_CHROMEOS_EXPLICIT_PASSPHRASE_MOJO_UTILS_H_

#include <memory>

#include "chromeos/crosapi/mojom/sync.mojom.h"

namespace syncer {

class Nigori;

// Converts |nigori| into its mojo representation.
crosapi::mojom::NigoriKeyPtr NigoriToMojo(const Nigori& nigori);

// Creates Nigori from its mojo representation. Returns nullptr if
// |mojo_nigori_key| doesn't represent a valid Nigori.
std::unique_ptr<Nigori> NigoriFromMojo(
    const crosapi::mojom::NigoriKey& mojo_nigori_key);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CHROMEOS_EXPLICIT_PASSPHRASE_MOJO_UTILS_H_
