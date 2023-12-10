// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer.h"

namespace ash {

namespace device_sync {

class CryptAuthKey;

class CryptAuthKeyProofComputerImpl : public CryptAuthKeyProofComputer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthKeyProofComputer> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyProofComputer> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthKeyProofComputerImpl(const CryptAuthKeyProofComputerImpl&) = delete;
  CryptAuthKeyProofComputerImpl& operator=(
      const CryptAuthKeyProofComputerImpl&) = delete;

  ~CryptAuthKeyProofComputerImpl() override;

  // CryptAuthKeyProofComputer:
  std::optional<std::string> ComputeKeyProof(
      const CryptAuthKey& key,
      const std::string& payload,
      const std::string& salt,
      const std::optional<std::string>& info) override;

 private:
  CryptAuthKeyProofComputerImpl();

  std::optional<std::string> ComputeSymmetricKeyProof(
      const CryptAuthKey& symmetric_key,
      const std::string& payload,
      const std::string& salt,
      const std::string& info);
  std::optional<std::string> ComputeAsymmetricKeyProof(
      const CryptAuthKey& asymmetric_key,
      const std::string& payload,
      const std::string& salt);
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_PROOF_COMPUTER_IMPL_H_
