// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_ecies_encryptor.h"

namespace ash {

namespace device_sync {

FakeCryptAuthEciesEncryptor::FakeCryptAuthEciesEncryptor() = default;

FakeCryptAuthEciesEncryptor::~FakeCryptAuthEciesEncryptor() = default;

void FakeCryptAuthEciesEncryptor::OnBatchEncryptionStarted() {
  action_ = Action::kEncryption;
}

void FakeCryptAuthEciesEncryptor::OnBatchDecryptionStarted() {
  action_ = Action::kDecryption;
}

void FakeCryptAuthEciesEncryptor::FinishAttempt(
    Action expected_action,
    const IdToOutputMap& id_to_output_map) {
  DCHECK_NE(Action::kUndefined, action_);
  DCHECK_EQ(expected_action, action_);
  OnAttemptFinished(id_to_output_map);
}

FakeCryptAuthEciesEncryptorFactory::FakeCryptAuthEciesEncryptorFactory() =
    default;

FakeCryptAuthEciesEncryptorFactory::~FakeCryptAuthEciesEncryptorFactory() =
    default;

std::unique_ptr<CryptAuthEciesEncryptor>
FakeCryptAuthEciesEncryptorFactory::CreateInstance() {
  auto instance = std::make_unique<FakeCryptAuthEciesEncryptor>();
  instance_ = instance.get();

  return instance;
}

}  // namespace device_sync

}  // namespace ash
