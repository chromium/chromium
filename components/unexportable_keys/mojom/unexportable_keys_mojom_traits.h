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

  static bool FromMojom(unexportable_keys::mojom::SignatureAlgorithm mojo_algo,
                        crypto::SignatureVerifier::SignatureAlgorithm* out);
};

template <>
struct EnumTraits<unexportable_keys::mojom::BackgroundTaskPriority,
                  unexportable_keys::BackgroundTaskPriority> {
  static unexportable_keys::mojom::BackgroundTaskPriority ToMojom(
      unexportable_keys::BackgroundTaskPriority priority);

  static bool FromMojom(
      unexportable_keys::mojom::BackgroundTaskPriority mojo_priority,
      unexportable_keys::BackgroundTaskPriority* out);
};

template <>
struct EnumTraits<unexportable_keys::mojom::ServiceError,
                  unexportable_keys::ServiceError> {
  static unexportable_keys::mojom::ServiceError ToMojom(
      unexportable_keys::ServiceError error);

  static bool FromMojom(unexportable_keys::mojom::ServiceError mojo_error,
                        unexportable_keys::ServiceError* out);
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
}  // namespace mojo

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEYS_MOJOM_TRAITS_H_
