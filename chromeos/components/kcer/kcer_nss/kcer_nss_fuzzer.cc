// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"

#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/ranges/algorithm.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_nss/test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/secure_hash.h"
#include "net/test/cert_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::UnorderedElementsAreArray;

namespace kcer {

const char kPkcs8Key[] =
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDa+"
    "Dq7TTFSw1AxRkaftrCM8tuPbYH7NTxLdHil0F2y4G+PvrlqN0qB43tRaKJPQEYhG+"
    "RnppXeOk6/AbgOFXBQCPoVJWOjxwMX3ea3rSLM5C9xUP9Rsnf/"
    "fkngD6G6pOo2nYinfgpINQDhGB/"
    "r8BJs69RNhvgdbN4aV7Bz8WGYqKF3DVhV+"
    "Di5zIOPNC9zoZQPey4duMS06OERG7Op8fFws3QoCzEywVdAbe/"
    "R+m5oeg875vLVvmONwDi52mqv4rgbfl+"
    "aPhyyPzoR3hdIPEi13AQB5hmyLAcTDtvcib3beNLw586NXcYgQZdcbLmDkjVRDK4uniE4QaR"
    "UGeRJD2+xAgMBAAECggEABMvoYMcg2WGDuESZZ5u6nn0eZUlT4329H6ECQzg/"
    "KTEvOGydhqUF6eD4B/"
    "vnsZ6POrVFSaZK76EtgukbJUcqcee0b1yljrDyvCXaoojgHjFcaa90HE/"
    "Gvvm++AcXoZfwX826cILQtQK2OCK4EPTDY+U+6LYtaVruZZDTVxgr7V+v4v1EDKEjQc+"
    "Ttupwo6aXSeiTKuqNsXuodoDvcv/"
    "uJgzMDCxi14TZTjaWOz7Xw2JZ+NLbTrsiqTyzmyJousV6/+4sfTYt8/"
    "tz0gMt3Qaddvs+BpTTrYIKTpsGYwPkKDqUdEkC87OQ6a2mXB1lpA7FMpZiiyJ1HpIXHkd+"
    "eLoNkQKBgQDlQtZdTlHu0YLM2WlORFYP8zyx/"
    "9k6rXZVVZLNBcZazEUkcgXJ7MS8pXieiXo6UJEf0PNCCY21ooOMPq6LI0NNF/"
    "yDbvT9Ri7eRNZRZosXIDaB17S93vgZ4Ukri7JJv3Bx+V2Wwl3P2/"
    "g080shqx8ZZsg1aQqpKov9qsavyXAuGQKBgQD0gh2P+rt0jQ9SnJ8ETGzFAXmhZyt4my+"
    "vBuFX8tFvByxMiUIxzsmHxCA6td/6KCWPwoo3xhe7r/"
    "+NHEFe6k9NR3KgrHOrHFcCqOAHWYPuPCKaG0ycbEF3tjzuDbZUJzH4WBgQDEsPlwDYEpkR17"
    "FX6sIMaAPMRvlOcZQZqnXRWQKBgQC6t0+c2E+UYB/"
    "WNG82ZiNthB13nrbNuj54y2PvBHgCtQDO6OpcBTBJr75n5/"
    "GbEsjPD78+lkdKmdvnWZmQCh0i6ZkndjOjHwjGz2t5CjnXkM2zu/kg9jo74aZVB8Yhl/"
    "+9Y2lcglojErS4czlKZ3LBnlsKXM1o7xTqeK6utjFd6QKBgQCAaH0CClm8IgC0EBDq/v/"
    "4jofEHhyUYFuwfdqGh705o/i90S/"
    "0XHc2V+fdLXsNM1xWnYJdPClmpk19XCNwp3kySp2GiErOyDlh6jKNaZOB4A8EA+Y+"
    "GBRhvFFPa+AfXd4+YHVyqCIbc+A7mbjNyAsY8u8p+"
    "M5Vz8hKTBfNStpJMQKBgH6YBNtXqyPDXgqyvRkjZt01I2ZYn2iIHBhHyDWE5j0QjhzF2kLpK"
    "IASO1uOTsaurAhPuyUTR+8EXu2OSP3RoaJ5buwUb8yL2b8Q075WCSp+eXdU6ZmEYdfnKX+"
    "63I9QIWvTFsREZybFR4tPgsQJGq/L8jNIxDxG9D0P1I2Zxrfd";

enum class Method {
  kGenerateRsaKey,
  kGenerateEcKey,
  kImportKey,
  kRunImportCertFromBytesUseRandomInput,
  kRunImportCertFromBytesUseValidCert,
  kImportX509Cert,
  // TODO(244408716): Enable when the methods are implemented.
  // kImportPkcs12Cert,
  // kExportPkcs12Cert,
  kRemoveKeyAndCerts,
  kRemoveCert,
  kListKeys,
  kListCerts,
  kDoesPrivateKeyExist,
  kSign,
  kSignRsaPkcs1Digest,
  kRunSignRsaPkcs1DigestAndVerifySignature,
  kGetAvailableTokens,
  kGetTokenInfo,
  kGetKeyInfo,
  kSetKeyNickname,
  kSetKeyPermissions,
  kSetCertProvisioningProfileId,
  kMaxValue = kSetCertProvisioningProfileId,
};

// Test-only overloads for better errors from EXPECT_EQ, etc.
std::ostream& operator<<(std::ostream& stream, Error val) {
  stream << static_cast<int>(val);
  return stream;
}
std::ostream& operator<<(std::ostream& stream, Token val) {
  stream << static_cast<int>(val);
  return stream;
}

struct Environment {
  Environment() {
    base::test::AllowCheckIsTestForTesting();
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

base::span<const uint8_t> GetCertData(
    const scoped_refptr<net::X509Certificate>& cert) {
  return base::make_span(CRYPTO_BUFFER_data(cert->cert_buffer()),
                         CRYPTO_BUFFER_len(cert->cert_buffer()));
}
base::span<const uint8_t> GetCertData(const scoped_refptr<const Cert>& cert) {
  return GetCertData(cert->GetX509Cert());
}

struct FuzzHash {
  using is_transparent = void;

  size_t operator()(const PublicKeySpki& spki) const {
    return base::FastHash(spki.value());
  }
  size_t operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return base::FastHash(GetCertData(cert));
  }
  size_t operator()(const scoped_refptr<const Cert>& cert) const {
    return operator()(cert->GetX509Cert());
  }
};

struct FuzzEqual {
  using is_transparent = void;

  bool operator()(const scoped_refptr<net::X509Certificate>& a,
                  const scoped_refptr<net::X509Certificate>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<net::X509Certificate>& a,
                  const scoped_refptr<const Cert>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<const Cert>& a,
                  const scoped_refptr<net::X509Certificate>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
  bool operator()(const scoped_refptr<const Cert>& a,
                  const scoped_refptr<const Cert>& b) const {
    return base::ranges::equal(GetCertData(a), GetCertData(b));
  }
};

// Enable testing::UnorderedElementsAreArray to compare against pointers (to
// avoid unnecessary copying).
bool operator==(const PublicKey& a, const PublicKey* b_ptr) {
  CHECK(b_ptr);
  return a == *b_ptr;
}

// A wrapper around `PublicKey` that also stores related certs and attributes
// for convenience.
struct FuzzKey {
  FuzzKey(PublicKey pub_key, Token token, KeyType type, bool can_be_listed)
      : public_key(std::move(pub_key)),
        token(token),
        key_type(type),
        can_be_listed(can_be_listed) {
    // NSS sets an empty nickname by default, this doesn't have to be like this
    // in general.
    nickname = "";
    // Custom attributes are stored differently in tests and have
    // empty values by default.
    key_permissions = chaps::KeyPermissions();
    cert_provisioning_profile_id = "";
  }
  FuzzKey(FuzzKey&&) = default;
  FuzzKey& operator=(FuzzKey&&) = default;
  FuzzKey(const FuzzKey&) = delete;
  auto operator=(const FuzzKey&) = delete;

  PublicKey public_key;
  Token token;
  KeyType key_type;
  // Contains imported net::X509Certificate certs. The corresponding kcer::Cert
  // certs will be found on the next ListCerts (from the related token) and
  // pending certs will be "converted" into kcer::Cert certs and stored in
  // `certs`. The fuzzer uses unordered maps (and not a base::flat_set as the
  // Kcer itself) because the fuzzer might theoretically work with more certs
  // than an average user.
  std::unordered_set<scoped_refptr<net::X509Certificate>, FuzzHash, FuzzEqual>
      pending_certs;
  std::unordered_set<scoped_refptr<const Cert>, FuzzHash, FuzzEqual> certs;
  // TODO(miersh): This is hacky, but NSS doesn't seem to create public key
  // objects for imported keys, and they cannot be found by the "list" method
  // because of that. This should be fixed in Kcer-without-NSS.
  bool can_be_listed = true;
  // TODO(miersh): NSS copies the nickname from a cert into its key in some
  // cases. Kcer-without-NSS won't do that. For now for simplicity the nickname
  // is only checked after a SetNickname() call, and not after importing certs.
  bool nickname_known = false;
  absl::optional<std::string> nickname;
  absl::optional<chaps::KeyPermissions> key_permissions;
  absl::optional<std::string> cert_provisioning_profile_id;
};

//==============================================================================

class KcerFuzzer {
 public:
  KcerFuzzer(const uint8_t* data, size_t size) : data_provider_(data, size) {}

  void Run();

 private:
  void InitializeKcer();
  base::WeakPtr<internal::KcerToken> CreateToken(Token token);

  void RunNextMethod();
  void RunGenerateRsaKey();
  void RunGenerateEcKey();
  void RunImportKey();
  void RunImportCertFromBytesUseRandomInput();
  void RunImportCertFromBytesUseValidCert();
  void RunImportX509Cert();
  void RunRemoveKeyAndCerts();
  void RunRemoveCert();
  void RunListKeys();
  void RunListCerts();
  void RunDoesPrivateKeyExist();
  void RunSign();
  void RunSignRsaPkcs1Digest();
  void RunSignRsaPkcs1DigestAndVerifySignature();
  void RunGetAvailableTokens();
  void RunGetTokenInfo();
  void RunGetKeyInfo();
  void RunSetKeyNickname();
  void RunSetKeyPermissions();
  void RunSetCertProvisioningProfileId();

  // Returns a randomized set of tokens. Can return tokens that were not
  // initialized for the current instance of Kcer.
  base::flat_set<Token> SelectTokens();
  // Returns a pointer to a random FuzzKey from `key_data_`. The pointer is
  // invalidated on `kcer_data_` update. Returns nullptr if there are no
  // existing keys to choose from.
  FuzzKey* SelectFuzzKey();
  // Generates a `PrivateKeyHandle` that can be used to call Kcer methods.
  // `out_kcer_key` will contain a pointer to an existing FuzzKey corresponding
  // to the handle or nullptr (if the handle doesn't describe any existing key).
  // `out_kcer_key` is invalidated on `kcer_data_` update.
  PrivateKeyHandle GeneratePrivateKeyHandle(FuzzKey** out_kcer_key);

  size_t GetSizeT(size_t max);
  std::vector<uint8_t> GetBytes(size_t min);

  // Returns the enum value with the next id for `val`. The enum `T` must have
  // kMaxValue defined.
  template <typename T>
  T NextEnumValue(T val);

  bool keep_fuzzing_ = true;
  bool user_token_available_ = false;
  bool device_token_available_ = false;

  // Tracks whether the key from `kPkcs8Key` was already imported.
  bool example_pkcs8_key_used_ = false;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  base::flat_map<Token, std::unique_ptr<TokenHolder>> available_tokens_;
  std::unique_ptr<Kcer> kcer_;
  // Keeps track of what Kcer is expected to contain.
  std::unordered_map<PublicKeySpki, FuzzKey, FuzzHash> kcer_data_;
  // A counter for the total number of certs in `kcer_data_`. Useful for
  // selecting a random cert.
  size_t certs_counter_ = 0;

  FuzzedDataProvider data_provider_;
};

void KcerFuzzer::Run() {
  InitializeKcer();

  while (!testing::Test::HasFailure() && keep_fuzzing_ &&
         data_provider_.remaining_bytes() > 0) {
    RunNextMethod();
  }
}

void KcerFuzzer::InitializeKcer() {
  base::WeakPtr<internal::KcerToken> user_token_ptr;
  user_token_available_ = data_provider_.ConsumeBool();
  if (user_token_available_) {
    user_token_ptr = CreateToken(Token::kUser);
  }

  base::WeakPtr<internal::KcerToken> device_token_ptr;
  device_token_available_ = data_provider_.ConsumeBool();
  if (device_token_available_) {
    device_token_ptr = CreateToken(Token::kDevice);
  }

  kcer_ = internal::CreateKcer(content::GetIOThreadTaskRunner({}),
                               user_token_ptr, device_token_ptr);
}

base::WeakPtr<internal::KcerToken> KcerFuzzer::CreateToken(Token token) {
  available_tokens_[token] =
      std::make_unique<TokenHolder>(token, /*initialized=*/true);
  return available_tokens_[token]->GetWeakPtr();
}

void KcerFuzzer::RunNextMethod() {
  Method next_method = data_provider_.ConsumeEnum<Method>();
  switch (next_method) {
    case Method::kGenerateRsaKey:
      return RunGenerateRsaKey();
    case Method::kGenerateEcKey:
      return RunGenerateEcKey();
    case Method::kImportKey:
      return RunImportKey();
    case Method::kRunImportCertFromBytesUseRandomInput:
      return RunImportCertFromBytesUseRandomInput();
    case Method::kRunImportCertFromBytesUseValidCert:
      return RunImportCertFromBytesUseValidCert();
    case Method::kImportX509Cert:
      return RunImportX509Cert();
    case Method::kRemoveKeyAndCerts:
      return RunRemoveKeyAndCerts();
    case Method::kRemoveCert:
      return RunRemoveCert();
    case Method::kListKeys:
      return RunListKeys();
    case Method::kListCerts:
      return RunListCerts();
    case Method::kDoesPrivateKeyExist:
      return RunDoesPrivateKeyExist();
    case Method::kSign:
      return RunSign();
    case Method::kSignRsaPkcs1Digest:
      return RunSignRsaPkcs1Digest();
    case Method::kRunSignRsaPkcs1DigestAndVerifySignature:
      return RunSignRsaPkcs1DigestAndVerifySignature();
    case Method::kGetAvailableTokens:
      return RunGetAvailableTokens();
    case Method::kGetTokenInfo:
      return RunGetTokenInfo();
    case Method::kGetKeyInfo:
      return RunGetKeyInfo();
    case Method::kSetKeyNickname:
      return RunSetKeyNickname();
    case Method::kSetKeyPermissions:
      return RunSetKeyPermissions();
    case Method::kSetCertProvisioningProfileId:
      return RunSetCertProvisioningProfileId();
  }
}

void KcerFuzzer::RunGenerateRsaKey() {
  Token token = data_provider_.ConsumeEnum<Token>();
  uint32_t modulus_length_bits = data_provider_.ConsumeBool() ? 1024 : 2048;
  // TODO(miersh): Generating software-backed keys requires d-bus communication
  // with Chaps. Figure out how to simulate that for the fuzzer.
  bool hardware_backed = true;
  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateRsaKey(token, modulus_length_bits, hardware_backed,
                        generate_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(generate_waiter.Get().has_value());
    EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  ASSERT_TRUE(generate_waiter.Get().has_value());
  PublicKey public_key = generate_waiter.Take().value();
  PublicKeySpki spki = public_key.GetSpki();
  EXPECT_GE(public_key.GetPkcs11Id()->size(), 4u);
  EXPECT_LE(public_key.GetPkcs11Id()->size(), base::kSHA1Length);
  EXPECT_GE(spki->size(), 4u);
  EXPECT_EQ(public_key.GetToken(), token);

  kcer_data_.emplace(std::move(spki),
                     FuzzKey(std::move(public_key), token, KeyType::kRsa,
                             /*can_be_listed=*/true));
}

void KcerFuzzer::RunGenerateEcKey() {
  Token token = data_provider_.ConsumeEnum<Token>();
  EllipticCurve elliptic_curve = EllipticCurve::kP256;
  // TODO(miersh): Generating software-backed keys requires d-bus communication
  // with Chaps. Figure out how to simulate that for the fuzzer.
  bool hardware_backed = true;

  base::test::TestFuture<base::expected<PublicKey, Error>> generate_waiter;
  kcer_->GenerateEcKey(token, elliptic_curve, hardware_backed,
                       generate_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(generate_waiter.Get().has_value());
    EXPECT_EQ(generate_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  ASSERT_TRUE(generate_waiter.Get().has_value());
  PublicKey public_key = generate_waiter.Take().value();
  PublicKeySpki spki = public_key.GetSpki();
  EXPECT_GE(public_key.GetPkcs11Id()->size(), 4u);
  EXPECT_LE(public_key.GetPkcs11Id()->size(), base::kSHA1Length);
  EXPECT_GE(spki->size(), 4u);
  EXPECT_EQ(public_key.GetToken(), token);

  kcer_data_.emplace(std::move(spki),
                     FuzzKey(std::move(public_key), token, KeyType::kEcc,
                             /*can_be_listed=*/true));
}

void KcerFuzzer::RunImportKey() {
  Token token = data_provider_.ConsumeEnum<Token>();

  std::vector<uint8_t> pkcs8_key;
  bool good_key_is_used = false;
  if (!example_pkcs8_key_used_ && data_provider_.ConsumeBool()) {
    absl::optional<std::vector<uint8_t>> key_der =
        base::Base64Decode(kPkcs8Key);
    ASSERT_TRUE(key_der.has_value());
    pkcs8_key = std::move(key_der).value();
    example_pkcs8_key_used_ = true;
    good_key_is_used = true;
  } else {
    pkcs8_key = GetBytes(/*min=*/0);
  }

  base::test::TestFuture<base::expected<PublicKey, Error>> import_key_waiter;
  kcer_->ImportKey(token, Pkcs8PrivateKeyInfoDer(std::move(pkcs8_key)),
                   import_key_waiter.GetCallback());

  if (!base::Contains(available_tokens_, token)) {
    ASSERT_FALSE(import_key_waiter.Get().has_value());
    EXPECT_EQ(import_key_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (good_key_is_used) {
    EXPECT_TRUE(import_key_waiter.Get().has_value());
    PublicKey public_key = import_key_waiter.Take().value();
    PublicKeySpki spki = public_key.GetSpki();

    kcer_data_.emplace(std::move(spki),
                       FuzzKey(std::move(public_key), token, KeyType::kRsa,
                               /*can_be_listed=*/false));
    return;
  }

  if (import_key_waiter.Get().has_value()) {
    // TODO(miersh): Ideally the fuzzer would figure out the type of the key and
    // add it to `kcer_data_`. But the chances of randomly finding a good key
    // are quite low. For simplicity just finish the fuzzer iteration with
    // success.
    keep_fuzzing_ = false;
    return;
  }
}

void KcerFuzzer::RunImportCertFromBytesUseRandomInput() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunImportCertFromBytesUseValidCert() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunImportX509Cert() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunRemoveKeyAndCerts() {
  FuzzKey* key;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&key);

  base::test::TestFuture<base::expected<void, Error>> remove_key_waiter;
  kcer_->RemoveKeyAndCerts(key_handle, remove_key_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(remove_key_waiter.Get().has_value());
    EXPECT_EQ(remove_key_waiter.Get().error(), Error::kTokenIsNotAvailable);
    return;
  }

  if (!key) {
    EXPECT_FALSE(remove_key_waiter.Get().has_value());
    return;
  }
  EXPECT_TRUE(remove_key_waiter.Get().has_value())
      << remove_key_waiter.Get().error();

  certs_counter_ -= key->certs.size();
  EXPECT_EQ(1u, kcer_data_.erase(key->public_key.GetSpki()));
}

void KcerFuzzer::RunRemoveCert() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunListKeys() {
  base::flat_set<Token> tokens = SelectTokens();

  std::vector<const PublicKey*> expected_result;
  for (const auto& [spki, kcer_key] : kcer_data_) {
    if (base::Contains(tokens, kcer_key.public_key.GetToken()) &&
        kcer_key.can_be_listed) {
      expected_result.push_back(&kcer_key.public_key);
    }
  }

  base::test::TestFuture<std::vector<PublicKey>, base::flat_map<Token, Error>>
      list_waiter;
  kcer_->ListKeys(std::move(tokens), list_waiter.GetCallback());
  EXPECT_THAT(list_waiter.Get<0>(), UnorderedElementsAreArray(expected_result));

  for (Token token : tokens) {
    if (!base::Contains(available_tokens_, token)) {
      EXPECT_TRUE(list_waiter.Get<1>().at(token) ==
                  Error::kTokenIsNotAvailable);
    }
  }
}

void KcerFuzzer::RunListCerts() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunDoesPrivateKeyExist() {
  FuzzKey* expected_key = nullptr;
  PrivateKeyHandle key_handle = GeneratePrivateKeyHandle(&expected_key);

  base::test::TestFuture<base::expected<bool, Error>> key_exist_waiter;
  kcer_->DoesPrivateKeyExist(key_handle, key_exist_waiter.GetCallback());

  if (available_tokens_.empty() ||
      (key_handle.GetTokenInternal().has_value() &&
       !base::Contains(available_tokens_,
                       key_handle.GetTokenInternal().value()))) {
    ASSERT_FALSE(key_exist_waiter.Get().has_value());
    EXPECT_EQ(key_exist_waiter.Get().error(), Error::kTokenIsNotAvailable);

    return;
  }

  if (expected_key) {
    ASSERT_TRUE(key_exist_waiter.Get().has_value());
    EXPECT_TRUE(key_exist_waiter.Get().value());
  } else {
    EXPECT_TRUE(!key_exist_waiter.Get().has_value() ||
                !key_exist_waiter.Get().value());
  }
}

void KcerFuzzer::RunSign() {
  // TODO(b:244408716): Implement.
}

// Runs SignRsaPkcs1Raw() with a random input, most likely incorrect one.
// Primarily check for crashes.
void KcerFuzzer::RunSignRsaPkcs1Digest() {
  // TODO(b:244408716): Implement.
}

// Runs SignRsaPkcs1Raw() with a correct DigestWithPrefix and verifies the
// signature for it.
void KcerFuzzer::RunSignRsaPkcs1DigestAndVerifySignature() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunGetAvailableTokens() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunGetTokenInfo() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunGetKeyInfo() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunSetKeyNickname() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunSetKeyPermissions() {
  // TODO(b:244408716): Implement.
}

void KcerFuzzer::RunSetCertProvisioningProfileId() {
  // TODO(b:244408716): Implement.
}

base::flat_set<Token> KcerFuzzer::SelectTokens() {
  base::flat_set<Token> tokens;
  // Assume there are only two different tokens.
  static_assert(static_cast<int>(Token::kMaxValue) == 1);
  uint number_of_tokens = data_provider_.ConsumeIntegralInRange<uint>(
      /*min=*/0, /*max=*/2);
  if (number_of_tokens == 0) {
    // Do nothing.
  } else if (number_of_tokens == 1) {
    tokens.insert(data_provider_.ConsumeEnum<Token>());
  } else {
    tokens.insert(Token::kUser);
    tokens.insert(Token::kDevice);
  }
  return tokens;
}

// The returned pointer is invalidated on `kcer_data_` update.
FuzzKey* KcerFuzzer::SelectFuzzKey() {
  if (kcer_data_.empty()) {
    return nullptr;
  }

  auto random_iter = kcer_data_.begin();
  size_t offset = GetSizeT(kcer_data_.size() - 1);
  std::advance(random_iter, offset);
  FuzzKey& kcer_key = random_iter->second;
  return &kcer_key;
}

PrivateKeyHandle KcerFuzzer::GeneratePrivateKeyHandle(FuzzKey** out_kcer_key) {
  *out_kcer_key = nullptr;

  // Algorithms (or approaches) to generate a handle.
  enum class Algo {
    kExistingPublicKey,
    kSpkiFromExistingKey,
    kSpkiWithTokenFromExistingKey,
    kSpkiWithInvertedTokenFromExistingKey,
    kRandomSpki,
    kRandomSpkiWithToken,
    kMaxValue = kRandomSpkiWithToken,
  };

  Algo algo = data_provider_.ConsumeEnum<Algo>();
  // Some ways to create a handle only work in specific conditions (e.g. need at
  // least one key to exist). Cycle through them until a working one is found.
  // kRandomSpki always works, so the cycle is not infinite.
  while (true) {
    switch (algo) {
      case Algo::kExistingPublicKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key);
      }
      case Algo::kSpkiFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key.GetSpki());
      }
      case Algo::kSpkiWithTokenFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        *out_kcer_key = kcer_key;
        return PrivateKeyHandle(kcer_key->public_key.GetToken(),
                                kcer_key->public_key.GetSpki());
      }
      case Algo::kSpkiWithInvertedTokenFromExistingKey: {
        FuzzKey* kcer_key = SelectFuzzKey();
        if (!kcer_key) {
          break;
        }
        Token token = kcer_key->public_key.GetToken();
        static_assert(static_cast<int>(Token::kMaxValue) == 1);
        token = token == Token::kUser ? Token::kDevice : Token::kUser;
        return PrivateKeyHandle(token, kcer_key->public_key.GetSpki());
      }
      case Algo::kRandomSpki: {
        PrivateKeyHandle result(PublicKeySpki(GetBytes(/*min=*/1)));
        auto iter = kcer_data_.find(result.GetSpkiInternal());
        if (iter != kcer_data_.end()) {
          *out_kcer_key = &(iter->second);
        }
        return result;
      }
      case Algo::kRandomSpkiWithToken: {
        Token token = data_provider_.ConsumeEnum<Token>();
        PrivateKeyHandle result(token, PublicKeySpki(GetBytes(/*min=*/1)));
        auto iter = kcer_data_.find(result.GetSpkiInternal());
        if ((iter != kcer_data_.end()) &&
            (iter->second.public_key.GetToken() == token)) {
          *out_kcer_key = &(iter->second);
        }
        return result;
      }
    }
    algo = NextEnumValue(algo);
  }
}

size_t KcerFuzzer::GetSizeT(size_t max) {
  return data_provider_.ConsumeIntegralInRange<size_t>(0, max);
}

std::vector<uint8_t> KcerFuzzer::GetBytes(size_t min) {
  std::vector<uint8_t> bytes = data_provider_.ConsumeBytes<uint8_t>(
      GetSizeT(/*max=*/data_provider_.remaining_bytes()));
  while (bytes.size() < min) {
    bytes.push_back(0);  // Append zeros to get to the minimum required length.
  }
  return bytes;
}

template <typename T>
T KcerFuzzer::NextEnumValue(T val) {
  using NumericT = std::underlying_type_t<T>;
  NumericT numeric_val = static_cast<NumericT>(val);
  NumericT number_of_values = static_cast<NumericT>(T::kMaxValue) + 1;
  return static_cast<T>((numeric_val + 1) % number_of_values);
}

}  // namespace kcer

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // The method might run multiple times within the same execution, initialize
  // the environment only once using a static variable.
  static kcer::Environment env;

  kcer::KcerFuzzer fuzzer(data, size);
  fuzzer.Run();

  return testing::Test::HasFailure() ? -1 : 0;
}
