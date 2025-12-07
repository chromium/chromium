// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "crypto/unexportable_key.h"

namespace net {
class SSLPrivateKey;
}  // namespace net

namespace client_certificates {

// Convert a provided UnexportableSigningKey into an SSLPrivateKey. This can
// involve a hardware operation so it is marked as being slow.
scoped_refptr<net::SSLPrivateKey> SSLPrivateKeyFromUnexportableSigningKeySlowly(
    const crypto::UnexportableSigningKey& key);

namespace internal {

using ConvertKeyCallback =
    base::RepeatingCallback<scoped_refptr<net::SSLPrivateKey>(
        const crypto::UnexportableSigningKey&)>;

void SetConverterForTesting(ConvertKeyCallback callback);

}  // namespace internal

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_SSL_KEY_CONVERTER_H_
