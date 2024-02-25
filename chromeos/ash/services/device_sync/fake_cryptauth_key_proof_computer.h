// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_

#include <optional>
#include <string>

#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer.h"

namespace ash {

namespace device_sync {

class CryptAuthKey;

class FakeCryptAuthKeyProofComputer : public CryptAuthKeyProofComputer {
 public:
  FakeCryptAuthKeyProofComputer();

  FakeCryptAuthKeyProofComputer(const FakeCryptAuthKeyProofComputer&) = delete;
  FakeCryptAuthKeyProofComputer& operator=(
      const FakeCryptAuthKeyProofComputer&) = delete;

  ~FakeCryptAuthKeyProofComputer() override;

  // CryptAuthKeyProofComputer:
  // Returns "fake_key_proof_|payload|>_<|salt|_|info (if not null)|".
  std::optional<std::string> ComputeKeyProof(
      const CryptAuthKey& key,
      const std::string& payload,
      const std::string& salt,
      const std::optional<std::string>& info) override;

  void set_should_return_null(bool should_return_null) {
    should_return_null_ = should_return_null;
  }

 private:
  // If true, ComputeKeyProof() returns std::nullopt.
  bool should_return_null_ = false;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
