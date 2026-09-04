// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "chromeos/ash/components/kcer/kcer_nss/test_utils.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/sign.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

// Same fixture style as kcer_private_key_factory_unittest.cc: a real
// kcer::Kcer wired to a real (in-memory) NSS slot via crypto::ScopedTestNSSDB.
// No TPM/Kcer simulation. The slot is software-only, so KcerPrivateKey is
// exercised against keys/certs that genuinely live in NSS. KcerPrivateKey is
// constructed directly here (the factory is intentionally not involved), so
// this test stands on its own.
class KcerPrivateKeyTest : public testing::Test {
 protected:
  // Generates a real EC P-256 key in NSS and returns its kcer::PublicKey.
  kcer::PublicKey GenerateEcKey(bool hardware_backed) {
    base::test::TestFuture<base::expected<kcer::PublicKey, kcer::Error>> future;
    kcer_holder_.GetKcer()->GenerateEcKey(
        kcer::Token::kUser, kcer::EllipticCurve::kP256, hardware_backed,
        future.GetCallback());
    auto result = future.Take();
    CHECK(result.has_value());
    return std::move(result.value());
  }

  // Builds a KcerPrivateKey directly around `public_key`. `source` selects the
  // hardware-backed (kChromeOsHwKey) or software (kChromeOsSwKey) variant.
  // KcerPrivateKey only needs the SubjectPublicKeyInfo, taken straight from the
  // PublicKey's public GetSpki() accessor.
  scoped_refptr<KcerPrivateKey> MakeKey(kcer::PublicKey public_key,
                                        PrivateKeySource source) {
    return base::MakeRefCounted<KcerPrivateKey>(kcer_holder_.GetKcer(),
                                                public_key.GetSpki(), source);
  }

  // Fetches the KeyInfo for the key behind `spki` (needed for BindCert).
  kcer::KeyInfo GetKeyInfo(const kcer::PublicKeySpki& spki) {
    base::test::TestFuture<base::expected<kcer::KeyInfo, kcer::Error>> future;
    kcer_holder_.GetKcer()->GetKeyInfo(
        kcer::PrivateKeyHandle(kcer::Token::kUser, spki), future.GetCallback());
    auto result = future.Take();
    CHECK(result.has_value());
    return std::move(result.value());
  }

  // Imports `x509` into the same NSS slot and returns the matching kcer::Cert.
  scoped_refptr<const kcer::Cert> ImportCert(
      scoped_refptr<net::X509Certificate> x509) {
    base::test::TestFuture<base::expected<void, kcer::Error>> import_future;
    kcer_holder_.GetKcer()->ImportX509Cert(kcer::Token::kUser, x509,
                                           import_future.GetCallback());
    CHECK(import_future.Take().has_value());

    base::test::TestFuture<std::vector<scoped_refptr<const kcer::Cert>>,
                           base::flat_map<kcer::Token, kcer::Error>>
        list_future;
    kcer_holder_.GetKcer()->ListCerts(
        base::flat_set<kcer::Token>({kcer::Token::kUser}),
        list_future.GetCallback());
    std::vector<scoped_refptr<const kcer::Cert>> certs =
        std::get<0>(list_future.Take());
    CHECK_EQ(certs.size(), 1u);
    return certs.front();
  }

  // TestKcerHolder  needs the browser IO thread, so a plain
  // base::test::TaskEnvironment is insufficient; use BrowserTaskEnvironment
  // with a real IO thread, mirroring ssl_private_key_kcer_unittest.cc.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  crypto::ScopedTestNSSDB nss_db_;
  kcer::TestKcerHolder kcer_holder_{/*user_slot=*/nss_db_.slot(),
                                    /*device_slot=*/nullptr};
};

TEST_F(KcerPrivateKeyTest, ReportsKcerSourceAndEcdsaAlgorithm) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  EXPECT_EQ(key->GetSource(), PrivateKeySource::kChromeOsHwKey);
  EXPECT_EQ(key->GetAlgorithm(), crypto::sign::ECDSA_SHA256);
}

TEST_F(KcerPrivateKeyTest, SourceReflectsHardwareBacking) {
  // Hardware-backed state is encoded in the PrivateKeySource: kChromeOsHwKey is
  // the hardware-backed variant, kChromeOsSwKey the software one. The source is
  // passed to KcerPrivateKey independently of the underlying NSS key, and the
  // NSS Kcer backend only supports generating hardware-backed EC keys, so both
  // keys are generated with hardware_backed=true.
  kcer::PublicKey hw_pub = GenerateEcKey(/*hardware_backed=*/true);
  EXPECT_EQ(MakeKey(std::move(hw_pub), PrivateKeySource::kChromeOsHwKey)
                ->GetSource(),
            PrivateKeySource::kChromeOsHwKey);

  kcer::PublicKey sw_pub = GenerateEcKey(/*hardware_backed=*/true);
  EXPECT_EQ(MakeKey(std::move(sw_pub), PrivateKeySource::kChromeOsSwKey)
                ->GetSource(),
            PrivateKeySource::kChromeOsSwKey);
}

TEST_F(KcerPrivateKeyTest, GetSubjectPublicKeyInfoReturnsSpki) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  const std::vector<uint8_t> spki = public_key.GetSpki().value();
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  // A non-empty SPKI implies the key really materialized in NSS.
  EXPECT_FALSE(key->GetSubjectPublicKeyInfo().empty());
  EXPECT_EQ(key->GetSubjectPublicKeyInfo(), spki);
}

TEST_F(KcerPrivateKeyTest, ToProtoSerializesSpkiAndSource) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  const std::vector<uint8_t> spki = public_key.GetSpki().value();
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  client_certificates_pb::PrivateKey proto = key->ToProto();
  EXPECT_EQ(proto.source(),
            client_certificates_pb::PrivateKey::PRIVATE_CHROME_OS_HW_KEY);
  EXPECT_EQ(proto.wrapped_key(), std::string(spki.begin(), spki.end()));
}

TEST_F(KcerPrivateKeyTest, ToDictSerializesSpkiAndSource) {
  // The NSS Kcer backend only supports generating hardware-backed EC keys; the
  // kChromeOsSwKey source is passed to KcerPrivateKey independently of the
  // underlying key, so generating with hardware_backed=true is fine here.
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  const std::vector<uint8_t> spki = public_key.GetSpki().value();
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsSwKey);

  base::DictValue dict = key->ToDict();
  ASSERT_TRUE(dict.FindString(kKey));
  EXPECT_EQ(*dict.FindString(kKey), base::Base64Encode(spki));
  ASSERT_TRUE(dict.FindInt(kKeySource));
  EXPECT_EQ(*dict.FindInt(kKeySource),
            static_cast<int>(PrivateKeySource::kChromeOsSwKey));
}

TEST_F(KcerPrivateKeyTest, SignSlowlyProducesSignature) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  const std::vector<uint8_t> data = key->GetSubjectPublicKeyInfo();

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> sign_future;
  key->Sign(data, sign_future.GetCallback());

  std::optional<std::vector<uint8_t>> signature = sign_future.Take();
  ASSERT_TRUE(signature.has_value());
  EXPECT_FALSE(signature->empty());
}

TEST_F(KcerPrivateKeyTest, GetSSLPrivateKeyNullUntilCertBound) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  const std::vector<uint8_t> spki = public_key.GetSpki().value();
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  // Before BindCert(): TLS surface is disabled.
  EXPECT_EQ(key->GetSSLPrivateKey(), nullptr);

  // Build a cert whose SPKI matches the generated key and import it into NSS.
  std::unique_ptr<net::CertBuilder> issuer = kcer::MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      kcer::MakeCertBuilder(issuer.get(), spki);
  scoped_refptr<const kcer::Cert> cert =
      ImportCert(cert_builder->GetX509Certificate());

  // BindCert() wires the TLS surface to kcer::SSLPrivateKeyKcer.
  key->BindCert(cert, GetKeyInfo(kcer::PublicKeySpki(spki)));
  EXPECT_NE(key->GetSSLPrivateKey(), nullptr);
}

TEST_F(KcerPrivateKeyTest, GetBoundCertNullUntilCertBound) {
  kcer::PublicKey public_key = GenerateEcKey(/*hardware_backed=*/true);
  const std::vector<uint8_t> spki = public_key.GetSpki().value();
  scoped_refptr<KcerPrivateKey> key =
      MakeKey(std::move(public_key), PrivateKeySource::kChromeOsHwKey);

  // Before BindCert(): no cert is bound.
  EXPECT_EQ(key->GetBoundCert(), nullptr);

  // Build a cert whose SPKI matches the generated key and import it into NSS.
  std::unique_ptr<net::CertBuilder> issuer = kcer::MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      kcer::MakeCertBuilder(issuer.get(), spki);
  scoped_refptr<net::X509Certificate> x509 =
      cert_builder->GetX509Certificate();
  scoped_refptr<const kcer::Cert> cert = ImportCert(x509);

  // After BindCert(): GetBoundCert() surfaces the bound cert, which the
  // certificate store uses to build a ClientIdentity without re-listing certs.
  key->BindCert(cert, GetKeyInfo(kcer::PublicKeySpki(spki)));
  scoped_refptr<net::X509Certificate> bound = key->GetBoundCert();
  ASSERT_TRUE(bound);
  EXPECT_TRUE(bound->EqualsExcludingChain(x509.get()));
}

}  // namespace

}  // namespace client_certificates
