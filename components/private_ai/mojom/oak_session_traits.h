// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_MOJOM_OAK_SESSION_TRAITS_H_
#define COMPONENTS_PRIVATE_AI_MOJOM_OAK_SESSION_TRAITS_H_

#include <array>

#include "components/private_ai/crypto/constants.h"
#include "components/private_ai/crypto/handshake_message.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<private_ai::mojom::HandshakeMessageDataView,
                    private_ai::HandshakeMessage> {
  static const std::array<uint8_t, private_ai::kP256X962Length>&
  ephemeral_public_key(const private_ai::HandshakeMessage& r) {
    return r.ephemeral_public_key;
  }

  static const std::vector<uint8_t>& ciphertext(
      const private_ai::HandshakeMessage& r) {
    return r.ciphertext;
  }

  static bool Read(private_ai::mojom::HandshakeMessageDataView data,
                   private_ai::HandshakeMessage* out);
};

}  // namespace mojo

#endif  // COMPONENTS_PRIVATE_AI_MOJOM_OAK_SESSION_TRAITS_H_
