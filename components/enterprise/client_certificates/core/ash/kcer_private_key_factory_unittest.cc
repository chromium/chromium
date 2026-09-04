// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ash/kcer_private_key_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "chromeos/ash/components/kcer/kcer_nss/test_utils.h"
#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"
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

// Integration-style (per Google reviewers): a real kcer::Kcer wired to a
// software-only NSS slot (crypto::ScopedTestNSSDB), modeling a no-TPM device
// where hardware_backed=true "succeeds" but yields kChromeOsSwKey.
class KcerPrivateKeyFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  crypto::ScopedTestNSSDB nss_db_;
  kcer::TestKcerHolder kcer_holder_{/*user_slot=*/nss_db_.slot(),
                                    /*device_slot=*/nullptr};

  std::unique_ptr<KcerPrivateKeyFactory> MakeFactory() {
    return std::make_unique<KcerPrivateKeyFactory>(kcer_holder_.GetKcer());
  }
};

TEST_F(KcerPrivateKeyFactoryTest, CreatePrivateKey_GeneratesRealEcKey) {
  auto factory = MakeFactory();

  base::test::TestFuture<scoped_refptr<PrivateKey>> future;
  factory->CreatePrivateKey(future.GetCallback());

  scoped_refptr<PrivateKey> key = future.Take();
  ASSERT_TRUE(key);
  // GenerateEcKey(hardware_backed=true) succeeds on the software-only NSS
  // test slot, but the factory resolves the source from KeyInfo, not from
  // that success - and a non-Chaps slot holds nothing hardware backed.
  EXPECT_EQ(key->GetSource(), PrivateKeySource::kChromeOsSwKey);
  EXPECT_EQ(key->GetAlgorithm(), crypto::sign::ECDSA_SHA256);

  // EC P-256 SubjectPublicKeyInfo is 91 bytes; a non-empty SPKI implies the
  // key really materialized in NSS (not a stub from a mock).
  EXPECT_FALSE(key->GetSubjectPublicKeyInfo().empty());

  // No cert has been imported yet, so the TLS surface must stay disabled.
  EXPECT_EQ(key->GetSSLPrivateKey(), nullptr);
}

TEST_F(KcerPrivateKeyFactoryTest, CreatePrivateKey_TagsKeyAsBrowserEnterprise) {
  auto factory = MakeFactory();

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> key = create_future.Take();
  ASSERT_TRUE(key);

  // The factory tags every freshly generated key as owned by the browser
  // enterprise client certificate (CA Connector) provisioning flow. Read the
  // tag back through Kcer to confirm it was persisted on the underlying NSS
  // key.
  base::test::TestFuture<base::expected<bool, kcer::Error>> tag_future;
  kcer_holder_.GetKcer()->GetBrowserEnterpriseClientCertTag(
      kcer::PrivateKeyHandle(
          kcer::Token::kUser,
          kcer::PublicKeySpki(key->GetSubjectPublicKeyInfo())),
      tag_future.GetCallback());
  ASSERT_TRUE(tag_future.Get().has_value());
  EXPECT_TRUE(tag_future.Get().value());
}

TEST_F(KcerPrivateKeyFactoryTest, CreatePrivateKey_NullKcer) {
  KcerPrivateKeyFactory factory{/*kcer=*/base::WeakPtr<kcer::Kcer>()};

  base::test::TestFuture<scoped_refptr<PrivateKey>> future;
  factory.CreatePrivateKey(future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(KcerPrivateKeyFactoryTest, LoadPrivateKey_ProtoRoundTrip) {
  auto factory = MakeFactory();

  // Generate a real key, serialize it, then reload via the proto path.
  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> created = create_future.Take();
  ASSERT_TRUE(created);
  const std::vector<uint8_t> original_spki = created->GetSubjectPublicKeyInfo();

  // The load path goes GetKeyInfo -> ListCerts -> (no cert match) -> return
  // unbound KcerPrivateKey. Both Kcer calls run for real against NSS.
  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  factory->LoadPrivateKey(created->ToProto(), load_future.GetCallback());
  scoped_refptr<PrivateKey> loaded = load_future.Take();

  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->GetSource(), PrivateKeySource::kChromeOsSwKey);
  EXPECT_EQ(loaded->GetSubjectPublicKeyInfo(), original_spki);
  EXPECT_EQ(loaded->GetSource(), created->GetSource());
  // No matching cert in Kcer yet -> TLS surface stays disabled.
  EXPECT_EQ(loaded->GetSSLPrivateKey(), nullptr);
}

TEST_F(KcerPrivateKeyFactoryTest, LoadPrivateKey_KeyDoesNotExist) {
  auto factory = MakeFactory();

  // Proto referencing an SPKI that was never generated.
  const std::vector<uint8_t> bogus_spki(91, 0xAB);
  client_certificates_pb::PrivateKey proto;
  proto.set_source(
      client_certificates_pb::PrivateKey::PRIVATE_CHROME_OS_HW_KEY);
  proto.set_wrapped_key(std::string(bogus_spki.begin(), bogus_spki.end()));

  base::test::TestFuture<scoped_refptr<PrivateKey>> future;
  factory->LoadPrivateKey(proto, future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(KcerPrivateKeyFactoryTest, LoadPrivateKeyFromDict_RoundTrip) {
  auto factory = MakeFactory();

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> created = create_future.Take();
  ASSERT_TRUE(created);
  const std::vector<uint8_t> original_spki = created->GetSubjectPublicKeyInfo();

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  factory->LoadPrivateKeyFromDict(created->ToDict(), load_future.GetCallback());
  scoped_refptr<PrivateKey> loaded = load_future.Take();

  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->GetSubjectPublicKeyInfo(), original_spki);
  EXPECT_EQ(loaded->GetSource(), created->GetSource());
}

TEST_F(KcerPrivateKeyFactoryTest,
       LoadPrivateKeyFromDict_SoftwareSourceLoadsAsSoftwareBacked) {
  auto factory = MakeFactory();

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> created = create_future.Take();
  ASSERT_TRUE(created);

  // Build a dict whose source is the software ChromeOS key. The loaded key
  // must honor that source (kChromeOsSwKey), independent of how the underlying
  // key was generated.
  base::DictValue dict;
  dict.Set(kKey, base::Base64Encode(created->GetSubjectPublicKeyInfo()));
  dict.Set(kKeySource, static_cast<int>(PrivateKeySource::kChromeOsSwKey));

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  factory->LoadPrivateKeyFromDict(dict, load_future.GetCallback());
  scoped_refptr<PrivateKey> loaded = load_future.Take();

  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->GetSource(), PrivateKeySource::kChromeOsSwKey);
}

TEST_F(KcerPrivateKeyFactoryTest,
       LoadPrivateKeyFromDict_StaleHardwareSourceIsDowngraded) {
  auto factory = MakeFactory();

  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> created = create_future.Take();
  ASSERT_TRUE(created);

  // Simulate an identity from before the factory stopped inferring hardware
  // backing from generation success: prefs claim kChromeOsHwKey for a
  // software-backed key. The load must trust live KeyInfo over the stale pref.
  base::DictValue dict;
  dict.Set(kKey, base::Base64Encode(created->GetSubjectPublicKeyInfo()));
  dict.Set(kKeySource, static_cast<int>(PrivateKeySource::kChromeOsHwKey));

  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  factory->LoadPrivateKeyFromDict(dict, load_future.GetCallback());
  scoped_refptr<PrivateKey> loaded = load_future.Take();

  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded->GetSource(), PrivateKeySource::kChromeOsSwKey);
}

TEST_F(KcerPrivateKeyFactoryTest, GetSSLPrivateKey_BindsAfterCertImport) {
  auto factory = MakeFactory();

  // 1. Generate a real EC key in NSS via the factory.
  base::test::TestFuture<scoped_refptr<PrivateKey>> create_future;
  factory->CreatePrivateKey(create_future.GetCallback());
  scoped_refptr<PrivateKey> created = create_future.Take();
  ASSERT_TRUE(created);

  // 2. Build an X.509 cert whose subject SPKI matches the generated key, so
  //    Kcer's ListCerts will return a match on SPKI when the factory reloads.
  std::unique_ptr<net::CertBuilder> issuer = kcer::MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      kcer::MakeCertBuilder(issuer.get(), created->GetSubjectPublicKeyInfo());
  scoped_refptr<net::X509Certificate> x509 = cert_builder->GetX509Certificate();
  ASSERT_TRUE(x509);

  // 3. Import the cert into the same NSS slot the key lives in.
  base::test::TestFuture<base::expected<void, kcer::Error>> import_future;
  kcer_holder_.GetKcer()->ImportX509Cert(kcer::Token::kUser, x509,
                                         import_future.GetCallback());
  ASSERT_TRUE(import_future.Take().has_value());

  // 4. Reload the private key. The factory's full chain runs:
  //    GetKeyInfo -> ListCerts -> BindCert(matched cert)
  base::test::TestFuture<scoped_refptr<PrivateKey>> load_future;
  factory->LoadPrivateKey(created->ToProto(), load_future.GetCallback());
  scoped_refptr<PrivateKey> loaded = load_future.Take();
  ASSERT_TRUE(loaded);

  // 5. BindCert should have wired the TLS surface to kcer::SSLPrivateKeyKcer.
  EXPECT_NE(loaded->GetSSLPrivateKey(), nullptr);
}

}  // namespace

}  // namespace client_certificates
