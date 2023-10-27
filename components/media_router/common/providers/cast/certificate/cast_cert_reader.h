// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_READER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_READER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace cast_certificate {

// Creates a trust store using the test roots encoded in the PEM file at |path|.
bool PopulateStoreWithCertsFromPath(bssl::TrustStoreInMemory* store,
                                    const base::FilePath& path);

// Reads a PEM file located at |path|, containing certificates to a vector of
// their DER data.
std::vector<std::string> ReadCertificateChainFromFile(
    const base::FilePath& path);

// Reads a PEM certificate list loaded into a C-string |str| into a
// vector of their DER data.
std::vector<std::string> ReadCertificateChainFromString(const char* str);

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_READER_H_
