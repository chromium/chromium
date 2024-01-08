// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor.h"
#include "chromeos/ash/services/device_sync/cryptauth_ecies_encryptor_impl.h"

namespace ash {

namespace device_sync {

class FakeCryptAuthEciesEncryptor : public CryptAuthEciesEncryptor {
 public:
  enum class Action { kUndefined, kEncryption, kDecryption };

  FakeCryptAuthEciesEncryptor();

  FakeCryptAuthEciesEncryptor(const FakeCryptAuthEciesEncryptor&) = delete;
  FakeCryptAuthEciesEncryptor& operator=(const FakeCryptAuthEciesEncryptor&) =
      delete;

  ~FakeCryptAuthEciesEncryptor() override;

  void FinishAttempt(Action expected_action,
                     const IdToOutputMap& id_to_output_map);

  const IdToInputMap& id_to_input_map() const { return id_to_input_map_; }

 private:
  // CryptAuthEciesEncryptor:
  void OnBatchEncryptionStarted() override;
  void OnBatchDecryptionStarted() override;

  Action action_ = Action::kUndefined;
};

class FakeCryptAuthEciesEncryptorFactory
    : public CryptAuthEciesEncryptorImpl::Factory {
 public:
  FakeCryptAuthEciesEncryptorFactory();

  FakeCryptAuthEciesEncryptorFactory(
      const FakeCryptAuthEciesEncryptorFactory&) = delete;
  FakeCryptAuthEciesEncryptorFactory& operator=(
      const FakeCryptAuthEciesEncryptorFactory&) = delete;

  ~FakeCryptAuthEciesEncryptorFactory() override;

  FakeCryptAuthEciesEncryptor* instance() { return instance_; }

 private:
  // CryptAuthEciesEncryptorImpl::Factory:
  std::unique_ptr<CryptAuthEciesEncryptor> CreateInstance() override;

  raw_ptr<FakeCryptAuthEciesEncryptor, DanglingUntriaged> instance_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ECIES_ENCRYPTOR_H_
