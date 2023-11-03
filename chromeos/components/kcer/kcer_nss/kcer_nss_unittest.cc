// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_impl.h"
#include "chromeos/components/kcer/kcer_nss/kcer_token_impl_nss.h"
#include "chromeos/components/kcer/kcer_nss/test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/secure_hash.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

// The tests here provide only the minimal coverage for the basic functionality
// of Kcer. More thorough testing, including edge cases, will be done in a
// fuzzer.
// TODO(244408716): Implement the fuzzer.

using testing::UnorderedElementsAre;

namespace kcer {

// Test-only overloads for better errors from EXPECT_EQ, etc.
std::ostream& operator<<(std::ostream& stream, Error val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, Token val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, PublicKey val) {
  stream << "{\n";
  stream << "  token: " << val.GetToken() << "\n";
  stream << "  pkcs11_id: " << base::Base64Encode(val.GetPkcs11Id().value())
         << "\n";
  stream << "  spki: " << base::Base64Encode(val.GetSpki().value()) << "\n";
  stream << "}\n";
  return stream;
}

namespace {

enum class TestKeyType {
  kRsa,
  kEcc,
  kImportedRsa,
  kImportedEcc,
};

constexpr char kPublicKeyBase64[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArURIGgAq8joyzjFdUpzmOeDa5VgTC8"
    "n77sMCQsm01mwk+6NwHhCSyCfXoB9EuMcKynj9SZbCgArnsHcZiqBsKpU/VnBO/"
    "vp5MSY5qFMYxEpjPYSQcASUkOlkVYieQN6NK4FUynPJBIh3Rs6LUHlGU+"
    "w3GifCl3Be4Q0om61Eo+jxQJBlRFTyqETh0AeHI2lEK9hsePsn8AMJn2tv7GoaiS+"
    "RoZsMAcDg8uhtmlQB/"
    "eoy7MtXwSchI0e2Q8QdUneNp529Ee+pUQ5Uki1L2pE4Pnyj+j2i2x4wGFGdJgiBMSvtpvdPdF+"
    "NMfjdbVaDzTF3rcL3lNCxRb4xk3TMFXV7dQIDAQAB";

std::string TestKeyTypeToStr(TestKeyType key_type) {
  switch (key_type) {
    case TestKeyType::kRsa:
      return "kRsa";
    case TestKeyType::kEcc:
      return "kEcc";
    case TestKeyType::kImportedRsa:
      return "kImportedRsa";
    case TestKeyType::kImportedEcc:
      return "kImportedEcc";
  }
}

std::vector<uint8_t> StrToBytes(const std::string& val) {
  return std::vector<uint8_t>(val.begin(), val.end());
}

scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
  return content::GetIOThreadTaskRunner({});
}

std::string ToString(const std::vector<SigningScheme>& vec) {
  std::stringstream res;
  res << "[";
  for (const SigningScheme& s : vec) {
    res << static_cast<int>(s) << ", ";
  }
  res << "]";
  return res.str();
}

std::string ToString(const absl::optional<chaps::KeyPermissions>& val) {
  if (!val.has_value()) {
    return "<empty>";
  }
  // Should be updated if `KeyPermissions` struct is changed.
  return base::StringPrintf("[arc:%d corp:%d]", val->key_usages().arc(),
                            val->key_usages().corporate());
}

std::unique_ptr<kcer::Kcer> CreateKcer(
    scoped_refptr<base::TaskRunner> token_task_runner,
    base::WeakPtr<kcer::internal::KcerToken> user_token,
    base::WeakPtr<kcer::internal::KcerToken> device_token) {
  auto kcer = std::make_unique<kcer::internal::KcerImpl>();
  kcer->Initialize(std::move(token_task_runner), std::move(user_token),
                   std::move(device_token));
  return kcer;
}

bool KeyInfoEquals(const KeyInfo& expected, const KeyInfo& actual) {
  if (expected.is_hardware_backed != actual.is_hardware_backed) {
    LOG(ERROR) << "ERROR: is_hardware_backed: expected: "
               << expected.is_hardware_backed
               << ", actual: " << actual.is_hardware_backed;
    return false;
  }
  if (expected.key_type != actual.key_type) {
    LOG(ERROR) << "ERROR: key_type: expected: " << int(expected.key_type)
               << ", actual: " << int(actual.key_type);
    return false;
  }
  if (expected.supported_signing_schemes != actual.supported_signing_schemes) {
    LOG(ERROR) << "ERROR: supported_signing_schemes: expected: "
               << ToString(expected.supported_signing_schemes)
               << ", actual: " << ToString(actual.supported_signing_schemes);
    return false;
  }
  if (expected.nickname != actual.nickname) {
    LOG(ERROR) << "ERROR: nickname: expected: "
               << expected.nickname.value_or("<empty>")
               << ", actual: " << actual.nickname.value_or("<empty>");
    return false;
  }
  if (!KeyPermissionsEqual(expected.key_permissions, actual.key_permissions)) {
    LOG(ERROR) << "ERROR: key_permissions: expected: "
               << ToString(expected.key_permissions)
               << ", actual: " << ToString(actual.key_permissions);
    return false;
  }
  if (expected.cert_provisioning_profile_id !=
      actual.cert_provisioning_profile_id) {
    LOG(ERROR) << "ERROR: cert_provisioning_profile_id: expected: "
               << expected.cert_provisioning_profile_id.value_or("<empty>")
               << ", actual: "
               << actual.cert_provisioning_profile_id.value_or("<empty>");
    return false;
  }
  return true;
}

// Reads a file in the PEM format, decodes it, returns the content of the first
// PEM block in the DER format. Currently supports CERTIFICATE and PRIVATE KEY
// block types.
absl::optional<std::vector<uint8_t>> ReadPemFileReturnDer(
    const base::FilePath& path) {
  std::string pem_data;
  if (!base::ReadFileToString(path, &pem_data)) {
    return absl::nullopt;
  }

  bssl::PEMTokenizer tokenizer(pem_data, {"CERTIFICATE", "PRIVATE KEY"});
  if (!tokenizer.GetNext()) {
    return absl::nullopt;
  }
  return StrToBytes(tokenizer.data());
}

// A helper class for receiving notifications from Kcer.
class NotificationsObserver {
 public:
  explicit NotificationsObserver(base::test::TaskEnvironment& task_environment)
      : task_environment_(task_environment) {}

  base::RepeatingClosure GetCallback() {
    return base::BindRepeating(&NotificationsObserver::OnCertDbChanged,
                               weak_factory_.GetWeakPtr());
  }

  void OnCertDbChanged() {
    notifications_counter_++;
    if (run_loop_ &&
        notifications_counter_ >= expected_notifications_.value()) {
      run_loop_->Quit();
    }
  }

  // Waits until the required number of notifications is received. Tries to
  // check that no extra notifications is sent.
  bool WaitUntil(size_t notifications) {
    if (notifications_counter_ < notifications) {
      expected_notifications_ = notifications;
      run_loop_.emplace();
      run_loop_->Run();
      run_loop_.reset();
      expected_notifications_.reset();
    }

    // An additional RunUntilIdle to try catching extra unwanted notifications.
    task_environment_->RunUntilIdle();

    if (notifications_counter_ != notifications) {
      LOG(ERROR) << "Actual notifications: " << notifications_counter_;
      return false;
    }
    return true;
  }

  size_t Notifications() const { return notifications_counter_; }

 private:
  const raw_ref<base::test::TaskEnvironment> task_environment_;
  size_t notifications_counter_ = 0;
  absl::optional<base::RunLoop> run_loop_;
  absl::optional<size_t> expected_notifications_;
  base::WeakPtrFactory<NotificationsObserver> weak_factory_{this};
};

std::unique_ptr<net::CertBuilder> MakeCertIssuer() {
  auto issuer = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                   /*issuer=*/nullptr);
  issuer->SetSubjectCommonName("IssuerSubjectCommonName");
  issuer->GenerateRSAKey();
  return issuer;
}

// Creates a certificate builder that can generate a self-signed certificate for
// the `public_key`.
std::unique_ptr<net::CertBuilder> MakeCertBuilder(
    net::CertBuilder* issuer,
    const std::vector<uint8_t>& public_key) {
  std::unique_ptr<net::CertBuilder> cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(public_key, issuer);
  cert_builder->SetSignatureAlgorithm(
      bssl::SignatureAlgorithm::kRsaPkcs1Sha256);
  auto now = base::Time::Now();
  cert_builder->SetValidity(now, now + base::Days(30));
  cert_builder->SetSubjectCommonName("SubjectCommonName");

  return cert_builder;
}

// Test fixture for KcerNss tests. Provides the least amount of pre-configured
// setup to give more control to the tests themself.
class KcerNssTest : public testing::Test {
 public:
  KcerNssTest() : observer_(task_environment_) {}

 protected:
  void InitializeKcer(std::vector<Token> tokens) {
    base::WeakPtr<internal::KcerToken> user_token_ptr;
    base::WeakPtr<internal::KcerToken> device_token_ptr;

    for (Token token_type : tokens) {
      if (token_type == Token::kUser) {
        CHECK(!user_token_ptr.MaybeValid());
        user_token_ =
            std::make_unique<TokenHolder>(token_type, /*initialize=*/true);
        user_token_ptr = user_token_->GetWeakPtr();
      } else if (token_type == Token::kDevice) {
        CHECK(!device_token_ptr.MaybeValid());
        device_token_ =
            std::make_unique<TokenHolder>(token_type, /*initialize=*/true);
        device_token_ptr = device_token_->GetWeakPtr();
      }
    }

    kcer_ = CreateKcer(IOTaskRunner(), user_token_ptr, device_token_ptr);
    observers_subscription_ = kcer_->AddObserver(observer_.GetCallback());
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  NotificationsObserver observer_;
  base::CallbackListSubscription observers_subscription_;
  std::unique_ptr<TokenHolder> user_token_;
  std::unique_ptr<TokenHolder> device_token_;
  std::unique_ptr<Kcer> kcer_;
};

// Test that if a method is called with a token that is not (and won't be)
// available, then an error is returned.
TEST_F(KcerNssTest, UseUnavailableTokenThenGetError) {
  InitializeKcer(/*tokens=*/{});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_waiter.GetCallback());

  ASSERT_FALSE(generate_waiter.Get().has_value());
  EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that all methods can be queued while a Kcer instance and its token are
// initializing and that the entire task queue can be processed when
// initialization completes (in this case - completes with a failure).
TEST_F(KcerNssTest, QueueTasksThenFailInitializationThenGetErrors) {
  // Do not initialize yet to simulate slow initialization.
  TokenHolder user_token(Token::kUser, /*initialize=*/false);

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  // Internal values don't matter, they won't be accessed during this test.
  scoped_refptr<Cert> fake_cert = base::MakeRefCounted<Cert>(
      Token::kUser, Pkcs11Id(), /*nickname=*/std::string(),
      /*x509_cert=*/nullptr);

  // Create a Kcer instance without any tokens. It will queue all the incoming
  // requests itself.
  auto kcer = std::make_unique<kcer::internal::KcerImpl>();
  auto subscription = kcer->AddObserver(observer_.GetCallback());

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_rsa_waiter;
  kcer->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_ec_waiter;
  kcer->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                      /*hardware_backed=*/true,
                      generate_ec_waiter.GetCallback());
  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer({1, 2, 3}),
                  import_key_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>>
      import_cert_from_bytes_waiter;
  kcer->ImportCertFromBytes(Token::kUser, CertDer({1, 2, 3}),
                            import_cert_from_bytes_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> import_x509_cert_waiter;
  kcer->ImportX509Cert(Token::kUser,
                       /*cert=*/cert_builder->GetX509Certificate(),
                       import_x509_cert_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>>
      remove_key_and_certs_waiter;
  kcer->RemoveKeyAndCerts(PrivateKeyHandle(PublicKeySpki()),
                          remove_key_and_certs_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
  kcer->RemoveCert(fake_cert, remove_cert_waiter.GetCallback());
  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_keys_waiter;
  kcer->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      list_certs_waiter;
  kcer->ListCerts({Token::kUser}, list_certs_waiter.GetCallback());
  base::test::TestFuture<base::expected<bool, Error>> does_key_exist_waiter;
  kcer->DoesPrivateKeyExist(PrivateKeyHandle(PublicKeySpki()),
                            does_key_exist_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
  kcer->Sign(PrivateKeyHandle(PublicKeySpki()), SigningScheme::kRsaPkcs1Sha512,
             DataToSign({1, 2, 3}), sign_waiter.GetCallback());
  base::test::TestFuture<base::expected<Signature, Error>> sign_digest_waiter;
  kcer->SignRsaPkcs1Raw(PrivateKeyHandle(PublicKeySpki()),
                        DigestWithPrefix({1, 2, 3}),
                        sign_digest_waiter.GetCallback());
  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<KeyInfo, Error>> get_key_info_waiter;
  kcer->GetKeyInfo(PrivateKeyHandle(PublicKeySpki()),
                   get_key_info_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
  kcer->SetKeyNickname(PrivateKeyHandle(PublicKeySpki()), "new_nickname",
                       set_nickname_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
  kcer->SetKeyPermissions(PrivateKeyHandle(PublicKeySpki()),
                          chaps::KeyPermissions(),
                          set_permissions_waiter.GetCallback());
  base::test::TestFuture<base::expected<void, Error>> set_cert_prov_waiter;
  kcer->SetCertProvisioningProfileId(PrivateKeyHandle(PublicKeySpki()),
                                     "cert_prov_id",
                                     set_cert_prov_waiter.GetCallback());
  // Close the list with one more GenerateRsaKey, so all methods are tested
  // with other methods before and after them.
  base::test::TestFuture<base::expected<PublicKey, Error>>
      generate_rsa_waiter_2;
  kcer->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                       /*hardware_backed=*/true,
                       generate_rsa_waiter_2.GetCallback());
  // TODO(244408716): Add more methods when they are implemented.

  // Check some waiters that the requests are queued.
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(list_keys_waiter.IsReady());
  EXPECT_FALSE(set_permissions_waiter.IsReady());

  // Initialize Kcer with a token. This will empty the queue in `kcer` and move
  // all the requests into the token.
  kcer->Initialize(IOTaskRunner(), user_token.GetWeakPtr(),
                   /*device_token=*/nullptr);
  task_environment_.RunUntilIdle();

  // Check some waiters that the requests are still queued.
  EXPECT_FALSE(generate_rsa_waiter.IsReady());
  EXPECT_FALSE(list_keys_waiter.IsReady());
  EXPECT_FALSE(set_permissions_waiter.IsReady());

  // This should process and fail all the requests.
  user_token.FailInitialization();

  ASSERT_FALSE(generate_rsa_waiter.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_ec_waiter.Get().has_value());
  EXPECT_EQ(generate_ec_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_key_waiter.Get().has_value());
  EXPECT_EQ(import_key_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_cert_from_bytes_waiter.Get().has_value());
  EXPECT_EQ(import_cert_from_bytes_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(import_x509_cert_waiter.Get().has_value());
  EXPECT_EQ(import_x509_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(remove_key_and_certs_waiter.Get().has_value());
  EXPECT_EQ(remove_key_and_certs_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(remove_cert_waiter.Get().has_value());
  EXPECT_EQ(remove_cert_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_keys_waiter.Get<1>().empty());
  EXPECT_EQ(list_keys_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(list_certs_waiter.Get<1>().empty());
  EXPECT_EQ(list_certs_waiter.Get<1>().at(Token::kUser),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(does_key_exist_waiter.Get().has_value());
  EXPECT_EQ(does_key_exist_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_waiter.Get().has_value());
  EXPECT_EQ(sign_waiter.Get().error(), Error::kTokenInitializationFailed);
  ASSERT_FALSE(sign_digest_waiter.Get().has_value());
  EXPECT_EQ(sign_digest_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_token_info_waiter.Get().has_value());
  EXPECT_EQ(get_token_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(get_key_info_waiter.Get().has_value());
  EXPECT_EQ(get_key_info_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_nickname_waiter.Get().has_value());
  EXPECT_EQ(set_nickname_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_permissions_waiter.Get().has_value());
  EXPECT_EQ(set_permissions_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(set_cert_prov_waiter.Get().has_value());
  EXPECT_EQ(set_cert_prov_waiter.Get().error(),
            Error::kTokenInitializationFailed);
  ASSERT_FALSE(generate_rsa_waiter_2.Get().has_value());
  EXPECT_EQ(generate_rsa_waiter_2.Get().error(),
            Error::kTokenInitializationFailed);
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer forwards notifications from external sources. (Notifications
// created by Kcer are tested together with the methods that create them.)
TEST_F(KcerNssTest, ObserveExternalNotification) {
  TokenHolder user_token(Token::kUser, /*initialize=*/true);

  std::unique_ptr<Kcer> kcer =
      CreateKcer(IOTaskRunner(), user_token.GetWeakPtr(),
                 /*device_token=*/nullptr);

  NotificationsObserver observer_1(task_environment_);
  NotificationsObserver observer_2(task_environment_);

  // Add the first observer.
  auto subscription_1 = kcer->AddObserver(observer_1.GetCallback());

  EXPECT_EQ(observer_1.Notifications(), 0u);

  // Check that it receives a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/1));

  // Add one more observer.
  auto subscription_2 = kcer->AddObserver(observer_2.GetCallback());

  // Check that both of them receive a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/2));
  EXPECT_TRUE(observer_2.WaitUntil(/*notifications=*/1));

  // Destroy the first subscription, the first observer should stop receiving
  // notifications now.
  subscription_1 = base::CallbackListSubscription();

  // Check that only the second observer receives a notification.
  net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  EXPECT_TRUE(observer_2.WaitUntil(/*notifications=*/2));
  EXPECT_TRUE(observer_1.WaitUntil(/*notifications=*/2));
}

TEST_F(KcerNssTest, ListKeys) {
  InitializeKcer({Token::kUser, Token::kDevice});

  std::vector<PublicKey> all_expected_keys;
  std::vector<PublicKey> user_expected_keys;
  std::vector<PublicKey> device_expected_keys;

  // Initially there should be no keys.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());

    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                          /*hardware_backed=*/true,
                          generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // The new key should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key on a different token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateRsaKey(Token::kDevice, RsaModulusLength::k2048,
                          /*hardware_backed=*/true,
                          generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Generate a key of a different type on user token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    user_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Generate a key of a different type on device token.
  {
    base::test::TestFuture<base::expected<PublicKey, Error>>
        generate_key_waiter;
    kcer_->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                         /*hardware_backed=*/true,
                         generate_key_waiter.GetCallback());
    ASSERT_TRUE(generate_key_waiter.Get().has_value());
    device_expected_keys.push_back(generate_key_waiter.Get().value());
    all_expected_keys.push_back(generate_key_waiter.Take().value());
  }

  // Keys of both types from both tokens should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser, Token::kDevice},
                    list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(all_expected_keys));
  }

  // Keys of both types only from the user token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(user_expected_keys));
  }

  // Keys of both types only from the device token should be found.
  {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kDevice}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_THAT(list_keys_waiter.Get<std::vector<PublicKey>>(),
                testing::UnorderedElementsAreArray(device_expected_keys));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::Sign() works correctly for RSA keys with different signing
// schemes.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignRsa) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kRsaPkcs1Sha1 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha1;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  // Test kRsaPkcs1Sha256 signature. Save signature to compare with it later.
  Signature rsa256_signature;
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPkcs1Sha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    rsa256_signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, rsa256_signature));
  }

  // Test kRsaPssRsaeSha256 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kRsaPssRsaeSha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  // Test `Kcer::SignRsaPkcs1Raw()` (kRsaPkcs1Sha256, but for pre-hashed
  // values).
  {
    // A caller would need to hash the data themself before calling
    // `SignRsaPkcs1Digest`, do that here.
    auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hasher->Update(data_to_sign->data(), data_to_sign->size());
    std::vector<uint8_t> hash(hasher->GetHashLength());
    hasher->Finish(hash.data(), hash.size());
    DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

    // Generate the signature.
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->SignRsaPkcs1Raw(PrivateKeyHandle(public_key),
                           std::move(digest_with_prefix),
                           sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    // Verify the signature.
    EXPECT_TRUE(VerifySignature(SigningScheme::kRsaPkcs1Sha256,
                                public_key.GetSpki(), data_to_sign, signature));
    // Manual hashing + `SignRsaPkcs1Digest` should produce the same signature
    // as just `Sign`.
    EXPECT_EQ(signature, rsa256_signature);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::Sign() works correctly for ECC keys.
// TODO(miersh): Expand crypto::SignatureVerifier to work with more signature
// schemes and add them to the test.
TEST_F(KcerNssTest, SignEcc) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_key_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true,
                       generate_key_waiter.GetCallback());
  ASSERT_TRUE(generate_key_waiter.Get().has_value());
  const PublicKey& public_key = generate_key_waiter.Get().value();

  DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  // Test kEcdsaSecp256r1Sha256 signature.
  {
    SigningScheme signing_scheme = SigningScheme::kEcdsaSecp256r1Sha256;
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key), signing_scheme, data_to_sign,
                sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key.GetSpki(),
                                data_to_sign, signature));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that a certificate can not be imported, if there's no key for it on
// the token.
TEST_F(KcerNssTest, ImportCertWithoutKeyThenFail) {
  InitializeKcer({Token::kUser});

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder = MakeCertBuilder(
      issuer.get(), base::Base64Decode(kPublicKeyBase64).value());

  CertDer cert(StrToBytes(cert_builder->GetDER()));

  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportCertFromBytes(Token::kUser, std::move(cert),
                             import_waiter.GetCallback());
  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);

  // Double check that ListCerts doesn't find the cert.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<0>().empty());  // Cert list is empty.
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that a certificate can not be imported, if there's no key for it on
// the token.
TEST_F(KcerNssTest, ImportCertWithKeyOnDifferentTokenThenFail) {
  InitializeKcer({Token::kUser, Token::kDevice});

  // Generate new key on the user token.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
  std::unique_ptr<net::CertBuilder> cert_builder =
      MakeCertBuilder(issuer.get(), public_key.GetSpki().value());

  // Import a cert for the key into the device token.
  base::test::TestFuture<base::expected<void, Error>> import_waiter;
  kcer_->ImportX509Cert(Token::kDevice, cert_builder->GetX509Certificate(),
                        import_waiter.GetCallback());
  ASSERT_FALSE(import_waiter.Get().has_value());
  EXPECT_EQ(import_waiter.Get().error(), Error::kKeyNotFound);

  // Double check that ListCerts doesn't find the cert.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<0>().empty());  // Cert list is empty.
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test that Kcer::GetTokenInfo() method returns meaningful values.
TEST_F(KcerNssTest, GetTokenInfo) {
  InitializeKcer({Token::kUser});

  base::test::TestFuture<base::expected<TokenInfo, Error>>
      get_token_info_waiter;
  kcer_->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
  ASSERT_TRUE(get_token_info_waiter.Get().has_value());
  const TokenInfo& token_info = get_token_info_waiter.Get().value();

  // These values don't have to be exactly like this, they are what a software
  // NSS slot returns in tests. Still useful to test that they are not
  // completely off.
  EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
  EXPECT_THAT(token_info.token_name,
              testing::StartsWith("NSS Application Slot"));
  EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test RSA specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForRsaKey) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(Token::kUser, RsaModulusLength::k2048,
                        /*hardware_backed=*/true,
                        generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                    key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kRsa);
  EXPECT_THAT(
      key_info.supported_signing_schemes,
      UnorderedElementsAre(
          SigningScheme::kRsaPkcs1Sha1, SigningScheme::kRsaPkcs1Sha256,
          SigningScheme::kRsaPkcs1Sha384, SigningScheme::kRsaPkcs1Sha512,
          SigningScheme::kRsaPssRsaeSha256, SigningScheme::kRsaPssRsaeSha384,
          SigningScheme::kRsaPssRsaeSha512));
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test ECC specific fields from GetKeyInfo's result.
TEST_F(KcerNssTest, GetKeyInfoForEccKey) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
  kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                    key_info_waiter.GetCallback());
  ASSERT_TRUE(key_info_waiter.Get().has_value());
  const KeyInfo& key_info = key_info_waiter.Get().value();
  EXPECT_EQ(key_info.key_type, KeyType::kEcc);
  EXPECT_THAT(key_info.supported_signing_schemes,
              UnorderedElementsAre(SigningScheme::kEcdsaSecp256r1Sha256,
                                   SigningScheme::kEcdsaSecp384r1Sha384,
                                   SigningScheme::kEcdsaSecp521r1Sha512));
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Test generic fields from GetKeyInfo's result and they get updated after
// related Set* methods.
TEST_F(KcerNssTest, GetKeyInfoGeneric) {
  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  KeyInfo expected_key_info;
  // Hardware- vs software-backed indicators on real devices are provided by
  // Chaps and are wrong in unit tests.
  expected_key_info.is_hardware_backed = true;
  // NSS sets an empty nickname by default, this doesn't have to be like this
  // in general.
  expected_key_info.nickname = "";
  // Custom attributes are stored differently in tests and have empty values by
  // default.
  expected_key_info.key_permissions = chaps::KeyPermissions();
  expected_key_info.cert_provisioning_profile_id = "";

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    const KeyInfo& key_info = key_info_waiter.Get().value();

    // Copy some fields, their values are covered by dedicated tests, this
    // test only checks that they don't change when they shouldn't.
    expected_key_info.key_type = key_info.key_type;
    expected_key_info.supported_signing_schemes =
        key_info.supported_signing_schemes;

    EXPECT_TRUE(KeyInfoEquals(expected_key_info, key_info));
  }

  {
    expected_key_info.nickname = "new_nickname";

    base::test::TestFuture<base::expected<void, Error>> set_nickname_waiter;
    kcer_->SetKeyNickname(PrivateKeyHandle(public_key),
                          expected_key_info.nickname.value(),
                          set_nickname_waiter.GetCallback());
    ASSERT_TRUE(set_nickname_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  {
    expected_key_info.key_permissions->mutable_key_usages()->set_corporate(
        true);
    expected_key_info.key_permissions->mutable_key_usages()->set_arc(true);

    base::test::TestFuture<base::expected<void, Error>> set_permissions_waiter;
    kcer_->SetKeyPermissions(PrivateKeyHandle(public_key),
                             expected_key_info.key_permissions.value(),
                             set_permissions_waiter.GetCallback());
    ASSERT_TRUE(set_permissions_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  {
    expected_key_info.cert_provisioning_profile_id = "cert_prov_id_123";

    base::test::TestFuture<base::expected<void, Error>> set_cert_prov_id_waiter;
    kcer_->SetCertProvisioningProfileId(
        PrivateKeyHandle(public_key),
        expected_key_info.cert_provisioning_profile_id.value(),
        set_cert_prov_id_waiter.GetCallback());
    ASSERT_TRUE(set_cert_prov_id_waiter.Get().has_value());
  }

  {
    base::test::TestFuture<base::expected<KeyInfo, Error>> key_info_waiter;
    kcer_->GetKeyInfo(PrivateKeyHandle(public_key),
                      key_info_waiter.GetCallback());
    ASSERT_TRUE(key_info_waiter.Get().has_value());
    EXPECT_TRUE(
        KeyInfoEquals(expected_key_info, key_info_waiter.Get().value()));
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

TEST_F(KcerNssTest, ImportCertForImportedKey) {
  InitializeKcer({Token::kUser});

  absl::optional<std::vector<uint8_t>> key = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.key"));
  ASSERT_TRUE(key.has_value() && (key->size() > 0));
  absl::optional<std::vector<uint8_t>> cert = ReadPemFileReturnDer(
      net::GetTestCertsDirectory().AppendASCII("client_1.pem"));
  ASSERT_TRUE(cert.has_value() && (cert->size() > 0));

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer_->ImportKey(Token::kUser, Pkcs8PrivateKeyInfoDer(std::move(key.value())),
                   import_key_waiter.GetCallback());
  ASSERT_TRUE(import_key_waiter.Get().has_value());

  const PublicKey& public_key = import_key_waiter.Get().value();

  EXPECT_EQ(public_key.GetToken(), Token::kUser);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetPkcs11Id()->size(), 20u);
  // Arbitrary bytes, not much to check about them.
  EXPECT_EQ(public_key.GetSpki()->size(), 294u);

  base::test::TestFuture<base::expected<void, Error>> import_cert_waiter;
  kcer_->ImportCertFromBytes(Token::kUser, CertDer(std::move(cert.value())),
                             import_cert_waiter.GetCallback());
  EXPECT_TRUE(import_cert_waiter.Get().has_value());
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));

  // List certs, make sure the new cert is listed.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  const auto& certs =
      certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
  EXPECT_EQ(certs.size(), 1u);
}

// Test different ways to call DoesPrivateKeyExist() method and that it
// returns correct results when Kcer has access to one token.
TEST_F(KcerNssTest, DoesPrivateKeyExistOneToken) {
  InitializeKcer({Token::kDevice});

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kDevice, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // The private key should be found by the PublicKey.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key.GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // The private key should be found on the specified token by the SPKI.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Looking for a key on a non-existing token should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key.GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kTokenIsNotAvailable);
  }

  // Looking for a key by an invalid SPKI should return an error.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  // Looking for a non-existing key should return a negative result.
  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

TEST_F(KcerNssTest, RemoveKeyAndCertsWithManyCerts) {
  if (NSS_VersionCheck("3.68") != PR_TRUE) {
    // TODO(b/283925148): Remove this when all the builders are updated.
    GTEST_SKIP() << "NSS is too old";
  }

  InitializeKcer({Token::kUser});

  // Generate new key.
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(Token::kUser, EllipticCurve::kP256,
                       /*hardware_backed=*/true, generate_waiter.GetCallback());
  ASSERT_TRUE(generate_waiter.Get().has_value());
  const PublicKey& public_key = generate_waiter.Get().value();

  // Import three certs, ids should be random, so they will be different.
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));
  }
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/2));
  }
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key.GetSpki().value());
    // Import a cert.
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, cert_builder->GetX509Certificate(),
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/3));
  }

  // Check that the imported cert can be found.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter;
  kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
  EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
            3u);

  base::test::TestFuture<base::expected<void, Error>> remove_waiter;
  kcer_->RemoveKeyAndCerts(PrivateKeyHandle(public_key),
                           remove_waiter.GetCallback());
  EXPECT_TRUE(remove_waiter.Get().has_value());
  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/4));

  // Check that the imported cert cannot be found anymore.
  base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                         base::flat_map<Token, Error>>
      certs_waiter_2;
  kcer_->ListCerts({Token::kUser}, certs_waiter_2.GetCallback());
  EXPECT_TRUE(certs_waiter_2.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(
      certs_waiter_2.Get<std::vector<scoped_refptr<const Cert>>>().empty());

  // Check that the generated key cannot be found anymore.
  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_keys_waiter;
  kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
  ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
  EXPECT_TRUE(list_keys_waiter.Get<std::vector<PublicKey>>().empty());
}

class KcerNssAllKeyTypesTest : public KcerNssTest,
                               public testing::WithParamInterface<TestKeyType> {
 protected:
  TestKeyType GetKeyType() { return GetParam(); }

  // Requires Kcer to be initialized.
  absl::optional<PublicKey> CreateKey(Token token, TestKeyType key_type) {
    base::test::TestFuture<base::expected<PublicKey, Error>> key_waiter;
    switch (key_type) {
      case TestKeyType::kRsa:
        kcer_->GenerateRsaKey(token, RsaModulusLength::k2048,
                              /*hardware_backed=*/true,
                              key_waiter.GetCallback());
        key_can_be_listed_ = true;
        key_type_ = KeyType::kRsa;
        break;
      case TestKeyType::kEcc:
        kcer_->GenerateEcKey(token, EllipticCurve::kP256,
                             /*hardware_backed=*/true,
                             key_waiter.GetCallback());
        key_can_be_listed_ = true;
        key_type_ = KeyType::kEcc;
        break;
      case TestKeyType::kImportedRsa: {
        absl::optional<std::vector<uint8_t>> key_to_import =
            ReadPemFileReturnDer(
                net::GetTestCertsDirectory().AppendASCII("client_1.key"));
        kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(key_to_import.value()),
                         key_waiter.GetCallback());
        key_can_be_listed_ = false;
        key_type_ = KeyType::kRsa;
        break;
      }
      case TestKeyType::kImportedEcc: {
        absl::optional<std::vector<uint8_t>> key_to_import =
            ReadPemFileReturnDer(
                net::GetTestCertsDirectory().AppendASCII("key_usage_p256.key"));
        kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(key_to_import.value()),
                         key_waiter.GetCallback());
        key_can_be_listed_ = false;
        key_type_ = KeyType::kEcc;
        break;
      }
    }
    if (!key_waiter.Get().has_value()) {
      return absl::nullopt;
    }
    return key_waiter.Take().value();
  }

  SigningScheme GetSuitableSigningScheme() {
    switch (GetKeyType()) {
      case TestKeyType::kRsa:
      case TestKeyType::kImportedRsa:
        return SigningScheme::kRsaPkcs1Sha256;
      case TestKeyType::kEcc:
      case TestKeyType::kImportedEcc:
        return SigningScheme::kEcdsaSecp256r1Sha256;
    }
  }

  // TODO(miersh): The implementation of ImportKey that uses NSS is not able to
  // list imported keys (even though it can do other operations with them). This
  // should be fixed in Kcer-without-NSS.
  bool key_can_be_listed_ = false;
  KeyType key_type_ = KeyType::kRsa;
};

// Test different ways to call DoesPrivateKeyExist() method and that it
// returns correct results when Kcer has access to two tokens.
TEST_P(KcerNssAllKeyTypesTest, DoesPrivateKeyExistTwoTokens) {
  InitializeKcer({Token::kUser, Token::kDevice});
  absl::optional<PublicKey> public_key =
      CreateKey(Token::kDevice, GetKeyType());
  ASSERT_TRUE(public_key.has_value());

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key.value()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key->GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kDevice, public_key->GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(Token::kUser, public_key->GetSpki()),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::vector<uint8_t>{1, 2, 3})),
        does_exist_waiter.GetCallback());
    ASSERT_FALSE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().error(), Error::kFailedToGetKeyId);
  }

  {
    std::vector<uint8_t> non_existing_key =
        base::Base64Decode(kPublicKeyBase64).value();
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(
        PrivateKeyHandle(PublicKeySpki(std::move(non_existing_key))),
        does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value())
        << does_exist_waiter.Get().error();
    EXPECT_EQ(does_exist_waiter.Get().value(), false);
  }

  EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
}

// Simulate a potential lifecycle of a key and related objects.
TEST_P(KcerNssAllKeyTypesTest, KeyLifecycle) {
  InitializeKcer({Token::kUser, Token::kDevice});

  // Check that the initialized tokens are returned as available.
  base::test::TestFuture<base::flat_set<Token>> get_tokens_waiter;
  kcer_->GetAvailableTokens(get_tokens_waiter.GetCallback());
  EXPECT_EQ(get_tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));

  // Check that the information about both initialized tokens is available.
  {
    base::test::TestFuture<base::expected<TokenInfo, Error>>
        get_token_info_waiter;
    kcer_->GetTokenInfo(Token::kUser, get_token_info_waiter.GetCallback());
    ASSERT_TRUE(get_token_info_waiter.Get().has_value());
    const TokenInfo& token_info = get_token_info_waiter.Get().value();
    EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
    EXPECT_THAT(token_info.token_name,
                testing::StartsWith("NSS Application Slot"));
    EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
  }
  {
    base::test::TestFuture<base::expected<TokenInfo, Error>>
        get_token_info_waiter;
    kcer_->GetTokenInfo(Token::kDevice, get_token_info_waiter.GetCallback());
    ASSERT_TRUE(get_token_info_waiter.Get().has_value());
    const TokenInfo& token_info = get_token_info_waiter.Get().value();
    EXPECT_THAT(token_info.pkcs11_id, testing::Lt(1000u));
    EXPECT_THAT(token_info.token_name,
                testing::StartsWith("NSS Application Slot"));
    EXPECT_EQ(token_info.module_name, "NSS Internal PKCS #11 Module");
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/0));
  }

  // Add a new key.
  absl::optional<PublicKey> public_key = CreateKey(Token::kUser, GetKeyType());
  ASSERT_TRUE(public_key.has_value());

  // Check that the key is listed.
  if (key_can_be_listed_) {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_EQ(list_keys_waiter.Get<std::vector<PublicKey>>().size(), 1u);
  }

  // Check that the key exists.
  {
    base::test::TestFuture<base::expected<bool, Error>> does_exist_waiter;
    kcer_->DoesPrivateKeyExist(PrivateKeyHandle(public_key->GetSpki()),
                               does_exist_waiter.GetCallback());
    ASSERT_TRUE(does_exist_waiter.Get().has_value());
    EXPECT_EQ(does_exist_waiter.Get().value(), true);
  }

  // Check that the key can sign data.
  {
    DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    SigningScheme signing_scheme = GetSuitableSigningScheme();
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->Sign(PrivateKeyHandle(public_key.value()), signing_scheme,
                data_to_sign, sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value()) << sign_waiter.Get().error();

    EXPECT_TRUE(VerifySignature(signing_scheme, public_key->GetSpki(),
                                data_to_sign, sign_waiter.Get().value()));
  }

  // Check that the key can sign pre-hashed data.
  if (key_type_ == KeyType::kRsa) {
    DataToSign data_to_sign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    // A caller would need to hash the data themself before calling
    // `SignRsaPkcs1Digest`, do that here.
    auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hasher->Update(data_to_sign->data(), data_to_sign->size());
    std::vector<uint8_t> hash(hasher->GetHashLength());
    hasher->Finish(hash.data(), hash.size());
    DigestWithPrefix digest_with_prefix(PrependSHA256DigestInfo(hash));

    // Generate the signature.
    base::test::TestFuture<base::expected<Signature, Error>> sign_waiter;
    kcer_->SignRsaPkcs1Raw(PrivateKeyHandle(public_key.value()),
                           std::move(digest_with_prefix),
                           sign_waiter.GetCallback());
    ASSERT_TRUE(sign_waiter.Get().has_value());
    const Signature& signature = sign_waiter.Get().value();

    // Verify the signature.
    EXPECT_TRUE(VerifySignature(SigningScheme::kRsaPkcs1Sha256,
                                public_key->GetSpki(), data_to_sign,
                                signature));
  }

  // Import a cert for the key.
  scoped_refptr<net::X509Certificate> x509_cert_1;
  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key->GetSpki().value());
    x509_cert_1 = cert_builder->GetX509Certificate();

    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportX509Cert(Token::kUser, x509_cert_1,
                          import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/1));
  }

  // List certs, make sure the new cert is listed.
  scoped_refptr<const Cert> kcer_cert_1;
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    const auto& certs =
        certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>();
    ASSERT_EQ(certs.size(), 1u);
    kcer_cert_1 = certs.front();
    EXPECT_TRUE(
        kcer_cert_1->GetX509Cert()->EqualsExcludingChain(x509_cert_1.get()));
  }

  // Remove the cert.
  {
    base::test::TestFuture<base::expected<void, Error>> remove_cert_waiter;
    kcer_->RemoveCert(kcer_cert_1, remove_cert_waiter.GetCallback());
    ASSERT_TRUE(remove_cert_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/2));
    kcer_cert_1 = nullptr;
  }

  // Check that the cert cannot be found anymore.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
              0u);
  }

  {
    std::unique_ptr<net::CertBuilder> issuer = MakeCertIssuer();
    std::unique_ptr<net::CertBuilder> cert_builder =
        MakeCertBuilder(issuer.get(), public_key->GetSpki().value());

    // Import another cert for the key to check that the key was not removed and
    // is still usable.
    CertDer cert_der(StrToBytes(cert_builder->GetDER()));
    base::test::TestFuture<base::expected<void, Error>> import_waiter;
    kcer_->ImportCertFromBytes(Token::kUser, std::move(cert_der),
                               import_waiter.GetCallback());
    EXPECT_TRUE(import_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/3));
  }

  // Check that the cert can be found.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter_3;
    kcer_->ListCerts({Token::kUser}, certs_waiter_3.GetCallback());
    EXPECT_TRUE(certs_waiter_3.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(
        certs_waiter_3.Get<std::vector<scoped_refptr<const Cert>>>().size(),
        1u);
  }

  if ((key_type_ == KeyType::kEcc) && NSS_VersionCheck("3.68") != PR_TRUE) {
    // TODO(b/283925148): Old NSS crashes on an attempt to remove an ECC key.
    // Most test running builders are up-to-date enough, but for the remaining
    // few just skip the rest of the test.
    return;
  }

  // Remove key and its certs.
  {
    base::test::TestFuture<base::expected<void, Error>> remove_waiter;
    kcer_->RemoveKeyAndCerts(PrivateKeyHandle(public_key.value()),
                             remove_waiter.GetCallback());
    EXPECT_TRUE(remove_waiter.Get().has_value());
    EXPECT_TRUE(observer_.WaitUntil(/*notifications=*/4));
  }

  // Check that the cert cannot be found anymore.
  {
    base::test::TestFuture<std::vector<scoped_refptr<const Cert>>,
                           base::flat_map<Token, Error>>
        certs_waiter;
    kcer_->ListCerts({Token::kUser}, certs_waiter.GetCallback());
    EXPECT_TRUE(certs_waiter.Get<1>().empty());  // Error map is empty.
    ASSERT_EQ(certs_waiter.Get<std::vector<scoped_refptr<const Cert>>>().size(),
              0u);
  }

  // Check that the key is not listed anymore.
  if (key_can_be_listed_) {
    base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
        list_keys_waiter;
    kcer_->ListKeys({Token::kUser}, list_keys_waiter.GetCallback());
    ASSERT_TRUE(list_keys_waiter.Get<1>().empty());  // Error map is empty.
    EXPECT_EQ(list_keys_waiter.Get<std::vector<PublicKey>>().size(), 0u);
  }
}

INSTANTIATE_TEST_SUITE_P(AllKeyTypes,
                         KcerNssAllKeyTypesTest,
                         testing::Values(TestKeyType::kRsa,
                                         TestKeyType::kEcc,
                                         TestKeyType::kImportedRsa,
                                         TestKeyType::kImportedEcc),
                         // Make test names more readable:
                         [](const auto& info) {
                           return TestKeyTypeToStr(info.param);
                         });

}  // namespace
}  // namespace kcer
