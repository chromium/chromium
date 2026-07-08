// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/certificate_provider/thread_safe_certificate_map.h"

#include <string>
#include <string_view>

#include "chromeos/components/certificate_provider/certificate_info.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {
namespace certificate_provider {
namespace {

constexpr char kExtension1[] = "extension1";
constexpr char kExtension2[] = "extension2";

CertificateInfo CreateCertInfo(const std::string& cert_filename) {
  CertificateInfo cert_info;
  cert_info.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), cert_filename);
  EXPECT_TRUE(cert_info.certificate) << "Could not load " << cert_filename;
  cert_info.supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);
  return cert_info;
}

std::string GetSpki(const net::X509Certificate& certificate) {
  std::string_view spki;
  EXPECT_TRUE(net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
      &spki));
  return std::string(spki);
}

class ThreadSafeCertificateMapTest : public testing::Test {
 protected:
  ThreadSafeCertificateMapTest()
      : cert_info1_(CreateCertInfo("client_1.pem")),
        cert_info2_(CreateCertInfo("client_2.pem")) {}

  void CheckLookUpCertificate(const CertificateInfo& cert_info,
                              const std::string& expected_extension_id) {
    bool is_currently_provided = false;
    CertificateInfo info;
    std::string extension_id;
    EXPECT_TRUE(map_.LookUpCertificate(
        *cert_info.certificate, &is_currently_provided, &info, &extension_id));
    EXPECT_TRUE(is_currently_provided);
    EXPECT_EQ(expected_extension_id, extension_id);

    is_currently_provided = false;
    extension_id.clear();
    EXPECT_TRUE(map_.LookUpCertificateBySpki(GetSpki(*cert_info.certificate),
                                             &is_currently_provided, &info,
                                             &extension_id));
    EXPECT_TRUE(is_currently_provided);
    EXPECT_EQ(expected_extension_id, extension_id);
  }

  ThreadSafeCertificateMap map_;
  const CertificateInfo cert_info1_;
  const CertificateInfo cert_info2_;
};

TEST_F(ThreadSafeCertificateMapTest, LookUpCertificate) {
  map_.UpdateCertificatesForExtension(kExtension1, {cert_info1_});
  CheckLookUpCertificate(cert_info1_, kExtension1);

  bool is_currently_provided = true;
  CertificateInfo info;
  std::string extension_id;
  EXPECT_FALSE(map_.LookUpCertificate(
      *cert_info2_.certificate, &is_currently_provided, &info, &extension_id));
  EXPECT_FALSE(is_currently_provided);

  // Removing the certificate retains the fingerprint as known but not
  // currently provided.
  map_.UpdateCertificatesForExtension(kExtension1, {});
  is_currently_provided = true;
  EXPECT_FALSE(map_.LookUpCertificate(
      *cert_info1_.certificate, &is_currently_provided, &info, &extension_id));
}

// If two extensions provide the same certificate, the extension that started
// providing it first must be preferred regardless of how the extension ids
// compare.
TEST_F(ThreadSafeCertificateMapTest, LookUpPrefersFirstProviderOnCollision) {
  static_assert(std::string_view(kExtension1) < std::string_view(kExtension2));

  // |kExtension2| is the first to provide |cert_info1_|.
  map_.UpdateCertificatesForExtension(kExtension2, {cert_info1_});
  // |kExtension1| later provides the same certificate.
  map_.UpdateCertificatesForExtension(kExtension1, {cert_info1_});
  EXPECT_EQ(1u, map_.GetCertificates().size());

  CheckLookUpCertificate(cert_info1_, kExtension2);

  // Re-registering the same certificate must keep |kExtension2| as the
  // preferred provider.
  map_.UpdateCertificatesForExtension(kExtension2, {cert_info1_});
  CheckLookUpCertificate(cert_info1_, kExtension2);

  // Once |kExtension2| stops providing the certificate, |kExtension1| takes
  // over.
  map_.RemoveExtension(kExtension2);
  CheckLookUpCertificate(cert_info1_, kExtension1);

  // Now |kExtension1| is the first provider; re-adding |kExtension2| does not
  // change that.
  map_.UpdateCertificatesForExtension(kExtension2, {cert_info1_});
  CheckLookUpCertificate(cert_info1_, kExtension1);
}

}  // namespace
}  // namespace certificate_provider
}  // namespace chromeos
