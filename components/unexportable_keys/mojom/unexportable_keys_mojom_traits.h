// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEYS_MOJOM_TRAITS_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEYS_MOJOM_TRAITS_H_

#include "base/types/token_type.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {
template <>
struct EnumTraits<unexportable_keys::mojom::SignatureAlgorithm,
                  crypto::SignatureVerifier::SignatureAlgorithm> {
  static unexportable_keys::mojom::SignatureAlgorithm ToMojom(
      crypto::SignatureVerifier::SignatureAlgorithm algo);

  static crypto::SignatureVerifier::SignatureAlgorithm FromMojom(
      unexportable_keys::mojom::SignatureAlgorithm mojo_algo);
};

template <>
struct EnumTraits<unexportable_keys::mojom::BackgroundTaskPriority,
                  unexportable_keys::BackgroundTaskPriority> {
  static unexportable_keys::mojom::BackgroundTaskPriority ToMojom(
      unexportable_keys::BackgroundTaskPriority priority);

  static unexportable_keys::BackgroundTaskPriority FromMojom(
      unexportable_keys::mojom::BackgroundTaskPriority mojo_priority);
};

template <>
struct EnumTraits<unexportable_keys::mojom::ServiceError,
                  unexportable_keys::ServiceError> {
  static unexportable_keys::mojom::ServiceError ToMojom(
      unexportable_keys::ServiceError error);

  static unexportable_keys::ServiceError FromMojom(
      unexportable_keys::mojom::ServiceError mojo_error);
};

template <>
struct StructTraits<unexportable_keys::mojom::UnexportableKeyIdDataView,
                    unexportable_keys::UnexportableKeyId> {
  static const base::UnguessableToken& key_id(
      const unexportable_keys::UnexportableKeyId& input) {
    return input.value();
  }

  static bool Read(unexportable_keys::mojom::UnexportableKeyIdDataView data,
                   unexportable_keys::UnexportableKeyId* output);
};

template <>
struct StructTraits<unexportable_keys::mojom::UnexportableSigningKeyIdDataView,
                    unexportable_keys::UnexportableSigningKeyId> {
  static const base::UnguessableToken& key_id(
      const unexportable_keys::UnexportableSigningKeyId& input) {
    return input.value();
  }

  static bool Read(
      unexportable_keys::mojom::UnexportableSigningKeyIdDataView data,
      unexportable_keys::UnexportableSigningKeyId* output);
};

template <>
struct StructTraits<
    unexportable_keys::mojom::UnexportableAttestationKeyIdDataView,
    unexportable_keys::UnexportableAttestationKeyId> {
  static const base::UnguessableToken& key_id(
      const unexportable_keys::UnexportableAttestationKeyId& input) {
    return input.value();
  }

  static bool Read(
      unexportable_keys::mojom::UnexportableAttestationKeyIdDataView data,
      unexportable_keys::UnexportableAttestationKeyId* output);
};

template <>
struct EnumTraits<unexportable_keys::mojom::AttestationFormat,
                  crypto::AttestationStatement::Format> {
  static unexportable_keys::mojom::AttestationFormat ToMojom(
      crypto::AttestationStatement::Format format);

  static crypto::AttestationStatement::Format FromMojom(
      unexportable_keys::mojom::AttestationFormat mojo_format);
};

template <>
struct StructTraits<unexportable_keys::mojom::AttestationStatementDataView,
                    crypto::AttestationStatement> {
  static crypto::AttestationStatement::Format format(
      const crypto::AttestationStatement& input) {
    return input.format;
  }

  static const std::vector<uint8_t>& statement(
      const crypto::AttestationStatement& input) {
    return input.statement;
  }

  static const std::vector<uint8_t>& signature(
      const crypto::AttestationStatement& input) {
    return input.signature;
  }

  static bool Read(unexportable_keys::mojom::AttestationStatementDataView data,
                   crypto::AttestationStatement* output);
};
}  // namespace mojo

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEYS_MOJOM_TRAITS_H_
