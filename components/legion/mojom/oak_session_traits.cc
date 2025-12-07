// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/mojom/oak_session_traits.h"

namespace mojo {

// static
bool StructTraits<legion::mojom::HandshakeMessageDataView,
                  legion::HandshakeMessage>::
    Read(legion::mojom::HandshakeMessageDataView data,
         legion::HandshakeMessage* out) {
  return data.ReadEphemeralPublicKey(&out->ephemeral_public_key) &&
         data.ReadCiphertext(&out->ciphertext);
}

}  // namespace mojo
