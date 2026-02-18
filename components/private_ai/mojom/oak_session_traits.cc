// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/mojom/oak_session_traits.h"

namespace mojo {

// static
bool StructTraits<private_ai::mojom::HandshakeMessageDataView,
                  private_ai::HandshakeMessage>::
    Read(private_ai::mojom::HandshakeMessageDataView data,
         private_ai::HandshakeMessage* out) {
  return data.ReadEphemeralPublicKey(&out->ephemeral_public_key) &&
         data.ReadCiphertext(&out->ciphertext);
}

}  // namespace mojo
