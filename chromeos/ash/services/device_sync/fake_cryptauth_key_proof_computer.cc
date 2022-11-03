// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_key_proof_computer.h"

#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kFakeKeyProofPrefix[] = "fake_key_proof";

}  // namespace

namespace ash {

namespace device_sync {

FakeCryptAuthKeyProofComputer::FakeCryptAuthKeyProofComputer() = default;

FakeCryptAuthKeyProofComputer::~FakeCryptAuthKeyProofComputer() = default;

absl::optional<std::string> FakeCryptAuthKeyProofComputer::ComputeKeyProof(
    const CryptAuthKey& key,
    const std::string& payload,
    const std::string& salt,
    const absl::optional<std::string>& info) {
  if (should_return_null_)
    return absl::nullopt;

  return kFakeKeyProofPrefix + std::string("_") + std::string("_") + payload +
         std::string("_") + salt + (info ? "_" + *info : "");
}

}  // namespace device_sync

}  // namespace ash
