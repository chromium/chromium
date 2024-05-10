// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_CRYPTO_TEST_HELPERS_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_CRYPTO_TEST_HELPERS_H_

#include <string_view>

namespace gcm {

struct IncomingMessage;

// Creates an encrypted representation of |payload| using the |peer_public_key|
// (as an octet string in uncompressed form per SEC1 2.3.3) and the
// |auth_secret|. Returns whether the payload could be created and has been
// written to the |*message|.
bool CreateEncryptedPayloadForTesting(std::string_view payload,
                                      std::string_view peer_public_key,
                                      std::string_view auth_secret,
                                      IncomingMessage* message);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_CRYPTO_TEST_HELPERS_H_
