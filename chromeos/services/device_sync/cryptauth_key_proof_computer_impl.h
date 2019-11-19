// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_key_proof_computer.h"

namespace chromeos {

namespace device_sync {

class CryptAuthKey;

class CryptAuthKeyProofComputerImpl : public CryptAuthKeyProofComputer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyProofComputer> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthKeyProofComputerImpl() override;

  // CryptAuthKeyProofComputer:
  base::Optional<std::string> ComputeKeyProof(
      const CryptAuthKey& key,
      const std::string& payload,
      const std::string& salt,
      const base::Optional<std::string>& info) override;

 private:
  CryptAuthKeyProofComputerImpl();

  base::Optional<std::string> ComputeSymmetricKeyProof(
      const CryptAuthKey& symmetric_key,
      const std::string& payload,
      const std::string& salt,
      const std::string& info);
  base::Optional<std::string> ComputeAsymmetricKeyProof(
      const CryptAuthKey& asymmetric_key,
      const std::string& payload,
      const std::string& salt);

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyProofComputerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
