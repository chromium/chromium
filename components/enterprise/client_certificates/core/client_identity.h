// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_IDENTITY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_IDENTITY_H_

#include "base/memory/scoped_refptr.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

class PrivateKey;

struct ClientIdentity {
  ClientIdentity(scoped_refptr<PrivateKey> private_key,
                 scoped_refptr<net::X509Certificate> client_certificate);
  ~ClientIdentity();

  scoped_refptr<PrivateKey> private_key{};
  scoped_refptr<net::X509Certificate> client_certificate{};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_IDENTITY_H_
