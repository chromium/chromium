// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace cast_certificate {
namespace testing {

// Returns components/test/data/media_router/common/providers/cast/certificate
const base::FilePath& GetCastCertificateDirectory();

// Returns
// components/test/data/media_router/common/providers/cast/certificate/
//   certificates
const base::FilePath& GetCastCertificatesSubDirectory();

// Helper structure that describes a message and its various signatures.
struct SignatureTestData {
  std::string message;

  // RSASSA PKCS#1 v1.5 with SHA1.
  std::string signature_sha1;

  // RSASSA PKCS#1 v1.5 with SHA256.
  std::string signature_sha256;
};

// Reads a PEM file that contains "MESSAGE", "SIGNATURE SHA1" and
// "SIGNATURE SHA256" blocks.
// |file_name| should be relative to
// //components/test/data/media_router/common/providers/cast/certificate
SignatureTestData ReadSignatureTestData(const base::StringPiece& file_name);

// Converts uint64_t unix timestamp in seconds to base::Time.
base::Time ConvertUnixTimestampSeconds(uint64_t time);

// Helper method that loads a certificate from the test certificates folder and
// places it in an heap allocated trust store.
std::unique_ptr<bssl::TrustStoreInMemory> LoadTestCert(
    const base::StringPiece& cert_file_name);

}  // namespace testing
}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
