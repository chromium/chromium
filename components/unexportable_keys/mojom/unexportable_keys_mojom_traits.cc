// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mojom/unexportable_keys_mojom_traits.h"

#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/unguessable_token.mojom.h"

namespace mojo {
unexportable_keys::mojom::SignatureAlgorithm
EnumTraits<unexportable_keys::mojom::SignatureAlgorithm,
           crypto::SignatureVerifier::SignatureAlgorithm>::
    ToMojom(crypto::SignatureVerifier::SignatureAlgorithm algo) {
  switch (algo) {
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
      return unexportable_keys::mojom::SignatureAlgorithm::RSA_PKCS1_SHA1;
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return unexportable_keys::mojom::SignatureAlgorithm::RSA_PKCS1_SHA256;
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return unexportable_keys::mojom::SignatureAlgorithm::ECDSA_SHA256;
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
      return unexportable_keys::mojom::SignatureAlgorithm::RSA_PSS_SHA256;
  }
}

bool mojo::EnumTraits<unexportable_keys::mojom::SignatureAlgorithm,
                      crypto::SignatureVerifier::SignatureAlgorithm>::
    FromMojom(unexportable_keys::mojom::SignatureAlgorithm mojo_algo,
              crypto::SignatureVerifier::SignatureAlgorithm* out) {
  switch (mojo_algo) {
    case unexportable_keys::mojom::SignatureAlgorithm::RSA_PKCS1_SHA1:
      *out = crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1;
      return true;
    case unexportable_keys::mojom::SignatureAlgorithm::RSA_PKCS1_SHA256:
      *out = crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
      return true;
    case unexportable_keys::mojom::SignatureAlgorithm::ECDSA_SHA256:
      *out = crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
      return true;
    case unexportable_keys::mojom::SignatureAlgorithm::RSA_PSS_SHA256:
      *out = crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256;
      return true;
  }
  return false;
}

unexportable_keys::mojom::BackgroundTaskPriority
EnumTraits<unexportable_keys::mojom::BackgroundTaskPriority,
           unexportable_keys::BackgroundTaskPriority>::
    ToMojom(unexportable_keys::BackgroundTaskPriority priority) {
  switch (priority) {
    case unexportable_keys::BackgroundTaskPriority::kBestEffort:
      return unexportable_keys::mojom::BackgroundTaskPriority::kBestEffort;
    case unexportable_keys::BackgroundTaskPriority::kUserBlocking:
      return unexportable_keys::mojom::BackgroundTaskPriority::kUserBlocking;
    case unexportable_keys::BackgroundTaskPriority::kUserVisible:
      return unexportable_keys::mojom::BackgroundTaskPriority::kUserVisible;
  }
}

bool EnumTraits<unexportable_keys::mojom::BackgroundTaskPriority,
                unexportable_keys::BackgroundTaskPriority>::
    FromMojom(unexportable_keys::mojom::BackgroundTaskPriority mojo_priority,
              unexportable_keys::BackgroundTaskPriority* out) {
  switch (mojo_priority) {
    case unexportable_keys::mojom::BackgroundTaskPriority::kBestEffort:
      *out = unexportable_keys::BackgroundTaskPriority::kBestEffort;
      return true;
    case unexportable_keys::mojom::BackgroundTaskPriority::kUserVisible:
      *out = unexportable_keys::BackgroundTaskPriority::kUserVisible;
      return true;
    case unexportable_keys::mojom::BackgroundTaskPriority::kUserBlocking:
      *out = unexportable_keys::BackgroundTaskPriority::kUserBlocking;
      return true;
  }
  return false;
}

unexportable_keys::mojom::ServiceError EnumTraits<
    unexportable_keys::mojom::ServiceError,
    unexportable_keys::ServiceError>::ToMojom(unexportable_keys::ServiceError
                                                  error) {
  switch (error) {
    case unexportable_keys::ServiceError::kAlgorithmNotSupported:
      return unexportable_keys::mojom::ServiceError::kAlgorithmNotSupported;
    case unexportable_keys::ServiceError::kCryptoApiFailed:
      return unexportable_keys::mojom::ServiceError::kCryptoApiFailed;
    case unexportable_keys::ServiceError::kVerifySignatureFailed:
      return unexportable_keys::mojom::ServiceError::kVerifySignatureFailed;
    case unexportable_keys::ServiceError::kKeyCollision:
      return unexportable_keys::mojom::ServiceError::kKeyCollision;
    case unexportable_keys::ServiceError::kKeyNotFound:
      return unexportable_keys::mojom::ServiceError::kKeyNotFound;
    case unexportable_keys::ServiceError::kKeyNotReady:
      return unexportable_keys::mojom::ServiceError::kKeyNotReady;
    case unexportable_keys::ServiceError::kNoKeyProvider:
      return unexportable_keys::mojom::ServiceError::kNoKeyProvider;
    case unexportable_keys::ServiceError::kOperationNotSupported:
      return unexportable_keys::mojom::ServiceError::kOperationNotSupported;
  }
}

bool EnumTraits<unexportable_keys::mojom::ServiceError,
                unexportable_keys::ServiceError>::
    FromMojom(unexportable_keys::mojom::ServiceError input,
              unexportable_keys::ServiceError* output) {
  switch (input) {
    case unexportable_keys::mojom::ServiceError::kAlgorithmNotSupported:
      *output = unexportable_keys::ServiceError::kAlgorithmNotSupported;
      return true;
    case unexportable_keys::mojom::ServiceError::kCryptoApiFailed:
      *output = unexportable_keys::ServiceError::kCryptoApiFailed;
      return true;
    case unexportable_keys::mojom::ServiceError::kVerifySignatureFailed:
      *output = unexportable_keys::ServiceError::kVerifySignatureFailed;
      return true;
    case unexportable_keys::mojom::ServiceError::kKeyCollision:
      *output = unexportable_keys::ServiceError::kKeyCollision;
      return true;
    case unexportable_keys::mojom::ServiceError::kKeyNotFound:
      *output = unexportable_keys::ServiceError::kKeyNotFound;
      return true;
    case unexportable_keys::mojom::ServiceError::kKeyNotReady:
      *output = unexportable_keys::ServiceError::kKeyNotReady;
      return true;
    case unexportable_keys::mojom::ServiceError::kNoKeyProvider:
      *output = unexportable_keys::ServiceError::kNoKeyProvider;
      return true;
    case unexportable_keys::mojom::ServiceError::kOperationNotSupported:
      *output = unexportable_keys::ServiceError::kOperationNotSupported;
      return true;
  }
  return false;
}

bool StructTraits<unexportable_keys::mojom::UnexportableKeyIdDataView,
                  unexportable_keys::UnexportableKeyId>::
    Read(unexportable_keys::mojom::UnexportableKeyIdDataView data,
         unexportable_keys::UnexportableKeyId* output) {
  base::UnguessableToken key_id;
  if (!data.ReadKeyId(&key_id)) {
    // Failed to read the underlying UnguessableToken.
    return false;
  }
  // Construct the base::TokenType from the UnguessableToken.
  *output = unexportable_keys::UnexportableKeyId(key_id);
  return true;
}
}  // namespace mojo
