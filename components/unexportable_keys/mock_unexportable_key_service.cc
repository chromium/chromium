// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mock_unexportable_key_service.h"

namespace unexportable_keys {

namespace {
using ::testing::Invoke;
}  // namespace

MockUnexportableKeyService::MockUnexportableKeyService() = default;
MockUnexportableKeyService::~MockUnexportableKeyService() = default;

void MockUnexportableKeyService::DelegateToService(
    UnexportableKeyService& service) {
  ON_CALL(*this, GenerateSigningKeySlowlyAsync)
      .WillByDefault(Invoke(
          &service, &UnexportableKeyService::GenerateSigningKeySlowlyAsync));
  ON_CALL(*this, FromWrappedSigningKeySlowlyAsync)
      .WillByDefault(Invoke(
          &service, &UnexportableKeyService::FromWrappedSigningKeySlowlyAsync));
  ON_CALL(*this, GenerateAttestationKeySlowlyAsync)
      .WillByDefault(
          Invoke(&service,
                 &UnexportableKeyService::GenerateAttestationKeySlowlyAsync));
  ON_CALL(*this, FromWrappedAttestationKeySlowlyAsync)
      .WillByDefault(Invoke(
          &service,
          &UnexportableKeyService::FromWrappedAttestationKeySlowlyAsync));
  ON_CALL(*this, GetAllKeysForGarbageCollectionSlowlyAsync)
      .WillByDefault(Invoke(
          &service,
          &UnexportableKeyService::GetAllKeysForGarbageCollectionSlowlyAsync));
  ON_CALL(*this, SignSlowlyAsync)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::SignSlowlyAsync));
  ON_CALL(*this, CertifySlowlyAsync)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::CertifySlowlyAsync));
  ON_CALL(*this, DeleteKeysSlowlyAsync)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::DeleteKeysSlowlyAsync));
  ON_CALL(*this, DeleteAllKeysSlowlyAsync)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::DeleteAllKeysSlowlyAsync));
  ON_CALL(*this, GetSubjectPublicKeyInfo)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::GetSubjectPublicKeyInfo));
  ON_CALL(*this, GetWrappedKey)
      .WillByDefault(Invoke(&service, &UnexportableKeyService::GetWrappedKey));
  ON_CALL(*this, GetAlgorithm)
      .WillByDefault(Invoke(&service, &UnexportableKeyService::GetAlgorithm));
  ON_CALL(*this, GetKeyTag)
      .WillByDefault(Invoke(&service, &UnexportableKeyService::GetKeyTag));
  ON_CALL(*this, GetCreationTime)
      .WillByDefault(
          Invoke(&service, &UnexportableKeyService::GetCreationTime));
}

}  // namespace unexportable_keys
