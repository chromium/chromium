// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
#define COMPONENTS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/cert/pki/trust_store_in_memory.h"

namespace cast_certificate {
namespace testing {

// Returns components/test/data/cast_certificate
const base::FilePath& GetCastCertificateDirectory();

// Returns components/test/data/cast_certificate/certificates
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
// |file_name| should be relative to //components/test/data/cast_certificate
SignatureTestData ReadSignatureTestData(const base::StringPiece& file_name);

// Converts uint64_t unix timestamp in seconds to base::Time.
base::Time ConvertUnixTimestampSeconds(uint64_t time);

// Helper method that loads a certificate from the test certificates folder and
// places it in an heap allocated trust store.
std::unique_ptr<net::TrustStoreInMemory> LoadTestCert(
    const base::StringPiece& cert_file_name);

}  // namespace testing
}  // namespace cast_certificate

#endif  // COMPONENTS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
