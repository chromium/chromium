// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_proof_computer.h"

namespace chromeos {

namespace device_sync {

class CryptAuthKey;

class FakeCryptAuthKeyProofComputer : public CryptAuthKeyProofComputer {
 public:
  FakeCryptAuthKeyProofComputer();
  ~FakeCryptAuthKeyProofComputer() override;

  // CryptAuthKeyProofComputer:
  // Returns "fake_key_proof_|payload|>_<|salt|_|info (if not null)|".
  base::Optional<std::string> ComputeKeyProof(
      const CryptAuthKey& key,
      const std::string& payload,
      const std::string& salt,
      const base::Optional<std::string>& info) override;

  void set_should_return_null(bool should_return_null) {
    should_return_null_ = should_return_null;
  }

 private:
  // If true, ComputeKeyProof() returns base::nullopt.
  bool should_return_null_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthKeyProofComputer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_KEY_PROOF_COMPUTER_H_
