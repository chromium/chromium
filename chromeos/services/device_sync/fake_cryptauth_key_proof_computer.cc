// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_key_proof_computer.h"

#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_key.h"

namespace {

const char kFakeKeyProofPrefix[] = "fake_key_proof";

}  // namespace

namespace chromeos {

namespace device_sync {

FakeCryptAuthKeyProofComputer::FakeCryptAuthKeyProofComputer() = default;

FakeCryptAuthKeyProofComputer::~FakeCryptAuthKeyProofComputer() = default;

base::Optional<std::string> FakeCryptAuthKeyProofComputer::ComputeKeyProof(
    const CryptAuthKey& key,
    const std::string& payload,
    const std::string& salt,
    const base::Optional<std::string>& info) {
  if (should_return_null_)
    return base::nullopt;

  return kFakeKeyProofPrefix + std::string("_") + std::string("_") + payload +
         std::string("_") + salt + (info ? "_" + *info : "");
}

}  // namespace device_sync

}  // namespace chromeos
