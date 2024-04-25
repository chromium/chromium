// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
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
SignatureTestData ReadSignatureTestData(std::string_view file_name);

// Converts uint64_t unix timestamp in seconds to base::Time.
base::Time ConvertUnixTimestampSeconds(uint64_t time);

// Helper method that loads a certificate from the test certificates folder and
// places it in an heap allocated trust store.
std::unique_ptr<bssl::TrustStoreInMemory> LoadTestCert(
    std::string_view cert_file_name);

// This allows for modifying the CastTrustStore in order to run a test. When
// the instance is destroyed, the CastTrustStore will be reverted to its
// default state. Used for testing.
class ScopedCastTrustStoreConfig {
 public:
  // Note: There is no need for a default config. Calling VerifyDeviceCert()
  // without a ScopedCastTrustStoreConfig instance present will use the
  // default configuration.
  static std::unique_ptr<ScopedCastTrustStoreConfig> BuiltInCertificates();
  static std::unique_ptr<ScopedCastTrustStoreConfig> TestCertificates(
      std::string_view cert_file_name);
  static std::unique_ptr<ScopedCastTrustStoreConfig> BuiltInAndTestCertificates(
      std::string_view cert_file_name);
  ScopedCastTrustStoreConfig(const ScopedCastTrustStoreConfig&) = delete;
  ScopedCastTrustStoreConfig& operator=(const ScopedCastTrustStoreConfig&) =
      delete;
  ~ScopedCastTrustStoreConfig();

 private:
  friend std::unique_ptr<ScopedCastTrustStoreConfig>
  std::make_unique<ScopedCastTrustStoreConfig>();
  ScopedCastTrustStoreConfig() = default;

  // Sets the `in_use_` singleton and checks that there are no other
  // ScopedCastTrustStoreConfig instances.
  static void SetInUse();

  // Tracks whether or not there is an instance globally in the process. This is
  // used to ensure that there is not more than one instance at any given time.
  static std::atomic_flag in_use_;
};

}  // namespace testing
}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_CERT_TEST_HELPERS_H_
