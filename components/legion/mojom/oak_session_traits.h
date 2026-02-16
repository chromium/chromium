// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_MOJOM_OAK_SESSION_TRAITS_H_
#define COMPONENTS_LEGION_MOJOM_OAK_SESSION_TRAITS_H_

#include <array>

#include "components/legion/crypto/constants.h"
#include "components/legion/crypto/handshake_message.h"
#include "components/legion/mojom/oak_session.mojom.h"
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

#endif  // COMPONENTS_LEGION_MOJOM_OAK_SESSION_TRAITS_H_
