// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Security/SecItem.h>
#import <Security/SecKey.h>

#import <memory>
#import <optional>
#import <utility>

#import "base/apple/scoped_cftyperef.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/enterprise/client_certificates/core/client_identity.h"
#import "components/enterprise/client_certificates/core/mock_certificate_provisioning_service.h"
#import "components/enterprise/client_certificates/core/mock_private_key.h"
#import "components/enterprise/client_certificates/ios/client_identity_ios.h"
#import "crypto/evp.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/boringssl/src/include/openssl/base.h"
#import "third_party/boringssl/src/include/openssl/bytestring.h"
#import "third_party/boringssl/src/include/openssl/ec_key.h"
#import "third_party/boringssl/src/include/openssl/evp.h"
#import "third_party/boringssl/src/include/openssl/rsa.h"
#import "third_party/boringssl/src/include/openssl/ssl.h"

using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;
using testing::Return;
using testing::StrictMock;

namespace client_certificates {

namespace {

scoped_refptr<net::X509Certificate> LoadTestCert() {
  static constexpr char kTestCertFileName[] = "client_1.pem";
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 kTestCertFileName);
}

base::apple::ScopedCFTypeRef<SecKeyRef> SecKeyFromPKCS8(
    base::span<const uint8_t> pkcs8) {
  bssl::UniquePtr<EVP_PKEY> openssl_key =
      crypto::evp::PrivateKeyFromBytes(pkcs8);
  if (!openssl_key) {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }

  // `SecKeyCreateWithData` expects PKCS#1 for RSA keys, and a concatenated
  // format for EC keys. See `SecKeyCopyExternalRepresentation` for details.
  CFStringRef key_type;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0)) {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }
  if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_RSA) {
    key_type = kSecAttrKeyTypeRSA;
    if (!RSA_marshal_private_key(cbb.get(),
                                 EVP_PKEY_get0_RSA(openssl_key.get()))) {
      return base::apple::ScopedCFTypeRef<SecKeyRef>();
    }
  } else if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_EC) {
    key_type = kSecAttrKeyTypeECSECPrimeRandom;
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(openssl_key.get());
    size_t priv_len = EC_KEY_priv2oct(ec_key, nullptr, 0);
    uint8_t* out;
    if (priv_len == 0 ||
        !EC_POINT_point2cbb(cbb.get(), EC_KEY_get0_group(ec_key),
                            EC_KEY_get0_public_key(ec_key),
                            POINT_CONVERSION_UNCOMPRESSED, nullptr) ||
        !CBB_add_space(cbb.get(), &out, priv_len) ||
        EC_KEY_priv2oct(ec_key, out, priv_len) != priv_len) {
      return base::apple::ScopedCFTypeRef<SecKeyRef>();
    }
  } else {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> attrs(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attrs.get(), kSecAttrKeyClass, kSecAttrKeyClassPrivate);
  CFDictionarySetValue(attrs.get(), kSecAttrKeyType, key_type);

  base::apple::ScopedCFTypeRef<CFDataRef> data(
      CFDataCreate(kCFAllocatorDefault, CBB_data(cbb.get()),
                   base::checked_cast<CFIndex>(CBB_len(cbb.get()))));

  return base::apple::ScopedCFTypeRef<SecKeyRef>(
      SecKeyCreateWithData(data.get(), attrs.get(), nullptr));
}

base::apple::ScopedCFTypeRef<SecKeyRef> LoadTestKey() {
  static constexpr char kTestKeyFileName[] = "client_1.pk8";
  base::FilePath pkcs8_path =
      net::GetTestCertsDirectory().AppendASCII(kTestKeyFileName);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  return SecKeyFromPKCS8(*pkcs8);
}

}  // namespace

class CertificateProvisioningServiceIOSTest : public PlatformTest {
 protected:
  using GetIOSCallback =
      base::OnceCallback<void(std::optional<ClientIdentityIOS>)>;

  void SetUp() override {
    // Create a mock core service and transfer ownership to the iOS service.
    auto mock_core_service =
        std::make_unique<StrictMock<MockCertificateProvisioningService>>();
    mock_core_service_ = mock_core_service.get();
    service_ =
        CertificateProvisioningServiceIOS::Create(std::move(mock_core_service));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // The iOS service under test.
  std::unique_ptr<CertificateProvisioningServiceIOS> service_;
  // A raw pointer to the mocked core service owned by `service_`.
  raw_ptr<StrictMock<MockCertificateProvisioningService>> mock_core_service_;
};

// Tests that a valid ClientIdentity from the core service is properly
// converted into a ClientIdentityIOS with a valid SecIdentityRef.
TEST_F(CertificateProvisioningServiceIOSTest, GetIdentityIOS_Success) {
  auto private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key = LoadTestKey();
  EXPECT_CALL(*private_key, GetSecKeyRef()).WillOnce(Return(fake_key.get()));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity("test", private_key, certificate);

  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(core_identity));

  base::test::TestFuture<std::optional<ClientIdentityIOS>> test_future;
  service_->GetManagedIdentityIOS(test_future.GetCallback());

  auto ios_identity = test_future.Get();

  ASSERT_TRUE(ios_identity.has_value());
  EXPECT_TRUE(ios_identity->is_valid());
  EXPECT_EQ(ios_identity->name(), "test");
  EXPECT_EQ(ios_identity->private_key(), private_key);
  EXPECT_EQ(ios_identity->certificate(), certificate);
  EXPECT_NE(ios_identity->identity_ref.get(), nullptr);
}

// Tests that if the core service returns nullopt, the iOS service also returns
// nullopt.
TEST_F(CertificateProvisioningServiceIOSTest, GetIdentityIOS_CoreFailure) {
  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(std::nullopt));

  base::test::TestFuture<std::optional<ClientIdentityIOS>> test_future;
  service_->GetManagedIdentityIOS(test_future.GetCallback());

  auto ios_identity = test_future.Get();

  EXPECT_FALSE(ios_identity.has_value());
}

// Tests the caching behavior: a second call should return the same iOS identity
// (without regenerating the SecIdentityRef) even though it calls the core
// service again.
TEST_F(CertificateProvisioningServiceIOSTest, GetIdentityIOS_CachesResult) {
  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key = LoadTestKey();
  EXPECT_CALL(*mocked_private_key, GetSecKeyRef())
      .WillOnce(Return(fake_key.get()));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity("test", mocked_private_key, certificate);

  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillRepeatedly(RunOnceCallbackRepeatedly<0>(core_identity));

  // First call, populates the cache.
  base::test::TestFuture<std::optional<ClientIdentityIOS>> first_future;
  service_->GetManagedIdentityIOS(first_future.GetCallback());
  auto first_identity = first_future.Get();
  ASSERT_TRUE(first_identity.has_value());
  EXPECT_NE(first_identity->identity_ref.get(), nullptr);

  // Second call, should be a cache hit for the SecIdentityRef.
  base::test::TestFuture<std::optional<ClientIdentityIOS>> second_future;
  service_->GetManagedIdentityIOS(second_future.GetCallback());
  auto second_identity = second_future.Get();

  ASSERT_TRUE(second_identity.has_value());
  EXPECT_EQ(first_identity->identity_ref.get(),
            second_identity->identity_ref.get());
}

// Tests that if the cached identity has an expired certificate, the service
// will refetch the identity from the core service.
TEST_F(CertificateProvisioningServiceIOSTest,
       GetIdentityIOS_RefetchesIfExpired) {
  auto mocked_private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key = LoadTestKey();
  EXPECT_CALL(*mocked_private_key, GetSecKeyRef())
      .WillOnce(Return(fake_key.get()));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity("test", mocked_private_key, certificate);

  auto mocked_private_key_2 =
      base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key_2 = LoadTestKey();
  EXPECT_CALL(*mocked_private_key_2, GetSecKeyRef())
      .WillOnce(Return(fake_key_2.get()));
  ClientIdentity core_identity_2("test", mocked_private_key_2, certificate);

  // Expect two calls, returning different identities.
  // Note: WillOnce + WillOnce sequences the returns.
  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(core_identity))
      .WillOnce(RunOnceCallback<0>(core_identity_2));

  // 1. Populate the cache.
  base::test::TestFuture<std::optional<ClientIdentityIOS>> first_future;
  service_->GetManagedIdentityIOS(first_future.GetCallback());
  auto first_identity = first_future.Get();
  ASSERT_TRUE(first_identity.has_value());

  // 2. Advance the clock to the distant future so the cert is expired.
  task_environment_.AdvanceClock(base::Days(500 * 365));

  // 3. Second call.
  base::test::TestFuture<std::optional<ClientIdentityIOS>> second_future;
  service_->GetManagedIdentityIOS(second_future.GetCallback());
  auto second_identity = second_future.Get();
  ASSERT_TRUE(second_identity.has_value());
  EXPECT_NE(first_identity->private_key(), second_identity->private_key());
  EXPECT_EQ(second_identity->private_key(), mocked_private_key_2);
}

// Tests that if creating the SecIdentityRef fails (e.g., bad private key),
// the resulting ClientIdentityIOS is invalid.
TEST_F(CertificateProvisioningServiceIOSTest, GetIdentityIOS_InvalidRefFails) {
  // Use a private key mock that does not return a valid SecKeyRef.
  auto private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  EXPECT_CALL(*private_key, GetSecKeyRef()).WillOnce(Return(nullptr));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity("test", private_key, certificate);

  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(core_identity));

  base::test::TestFuture<std::optional<ClientIdentityIOS>> test_future;
  service_->GetManagedIdentityIOS(test_future.GetCallback());

  auto ios_identity = test_future.Get();

  // The final identity should be nullopt because it's not valid for iOS use.
  EXPECT_FALSE(ios_identity.has_value());
}

// Tests the compatibility wrapper `GetManagedIdentity` returns a valid
// ClientIdentity without the iOS-specific parts.
TEST_F(CertificateProvisioningServiceIOSTest, GetManagedIdentity_StripsData) {
  auto private_key = base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key = LoadTestKey();
  EXPECT_CALL(*private_key, GetSecKeyRef()).WillOnce(Return(fake_key.get()));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity("test", private_key, certificate);

  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(core_identity));

  base::test::TestFuture<std::optional<ClientIdentity>> test_future;
  service_->GetManagedIdentity(test_future.GetCallback());

  auto managed_identity = test_future.Get();

  ASSERT_TRUE(managed_identity.has_value());
  EXPECT_TRUE(managed_identity->is_valid());
  EXPECT_EQ(managed_identity->name, "test");
  EXPECT_EQ(managed_identity->private_key, private_key);
  EXPECT_EQ(managed_identity->certificate, certificate);
}

// Tests that if the core service returns a different identity (e.g. after a
// rotation or re-provisioning), the iOS service correctly regenerates the
// SecIdentityRef.
TEST_F(CertificateProvisioningServiceIOSTest,
       GetIdentityIOS_UpdatesOnNewIdentity) {
  auto mocked_private_key_1 =
      base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key_1 = LoadTestKey();
  EXPECT_CALL(*mocked_private_key_1, GetSecKeyRef())
      .WillOnce(Return(fake_key_1.get()));
  auto certificate = LoadTestCert();
  ClientIdentity core_identity_1("test", mocked_private_key_1, certificate);

  auto mocked_private_key_2 =
      base::MakeRefCounted<StrictMock<MockPrivateKey>>();
  auto fake_key_2 = LoadTestKey();
  EXPECT_CALL(*mocked_private_key_2, GetSecKeyRef())
      .WillOnce(Return(fake_key_2.get()));
  ClientIdentity core_identity_2("test", mocked_private_key_2, certificate);

  EXPECT_CALL(*mock_core_service_, GetManagedIdentity(_))
      .WillOnce(RunOnceCallback<0>(core_identity_1))
      .WillOnce(RunOnceCallback<0>(core_identity_2));

  // 1. Populate the cache.
  base::test::TestFuture<std::optional<ClientIdentityIOS>> first_future;
  service_->GetManagedIdentityIOS(first_future.GetCallback());
  auto first_identity = first_future.Get();
  ASSERT_TRUE(first_identity.has_value());

  // 2. Second call returns a DIFFERENT identity (different private key).
  base::test::TestFuture<std::optional<ClientIdentityIOS>> second_future;
  service_->GetManagedIdentityIOS(second_future.GetCallback());
  auto second_identity = second_future.Get();

  ASSERT_TRUE(second_identity.has_value());
  EXPECT_NE(first_identity->identity_ref.get(),
            second_identity->identity_ref.get());
  EXPECT_EQ(second_identity->private_key(), mocked_private_key_2);
}

}  // namespace client_certificates
