// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"

#include <windows.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>
#include <winternl.h>

#include <string_view>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include <algorithm>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"
#include "crypto/aead.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace credential_provider {

const base::TimeDelta
    PasswordRecoveryManager::kDefaultEscrowServiceEncryptionKeyRequestTimeout =
        base::Milliseconds(12000);

const base::TimeDelta
    PasswordRecoveryManager::kDefaultEscrowServiceDecryptionKeyRequestTimeout =
        base::Milliseconds(3000);

namespace {

typedef std::vector<std::pair<std::string, std::string*>>
    UrlFetchResultNeedOutputs;

// Constants for storing password recovery information in the LSA.
constexpr char kUserPasswordLsaStoreIdKey[] = "resource_id";
constexpr char kUserPasswordLsaStoreEncryptedPasswordKey[] =
    "encrypted_password";

// Constants used for contacting the password escrow service.
const char kEscrowServiceGenerateKeyPairPath[] = "/v1/generatekeypair";
const char kGenerateKeyPairRequestDeviceIdParameterName[] = "deviceId";
const char kGenerateKeyPairResponsePublicKeyParameterName[] = "base64PublicKey";
const char kGenerateKeyPairResponseResourceIdParameterName[] = "resourceId";

const char kEscrowServiceGetPrivateKeyPath[] = "/v1/getprivatekey";
const char kGetPrivateKeyResponsePrivateKeyParameterName[] = "base64PrivateKey";

// Constants used during padding and unpadding given secret.
constexpr char kPaddedPassword[] = "password";

constexpr char kPasswordLength[] = "password_length";

constexpr char kPaddingChar = '-';

constexpr size_t kMinPaddedPasswordLength = 64;

// Constants used during encrypting and decrypting given secret.
constexpr size_t kNonceLength = 12;

constexpr size_t kSessionKeyLength = 32;

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 3;

bool Base64DecodeCryptographicKey(const std::string& cryptographic_key,
                                  std::string* out) {
  std::string cryptographic_key_copy;
  base::RemoveChars(cryptographic_key, "\n", &cryptographic_key_copy);
  if (!base::Base64Decode(cryptographic_key_copy, out)) {
    LOGFN(ERROR) << "Base64Decode failed";
    return false;
  }
  return true;
}

// Callback to log password encryption/decryption errors.
static int LogBoringSSLError(const char* str, size_t len, void* ctx) {
  LOGFN(ERROR) << std::string_view(str, len);
  return 1;
}

// PadSecret pads the given |secret| with kPaddingChar and serializes the padded
// secret into JSON along with original secret length.
bool PadSecret(const std::string& secret, std::string* out) {
  size_t padded_length = (secret.size() + kMinPaddedPasswordLength - 1) &
                         ~(kMinPaddedPasswordLength - 1);
  std::string padded_secret(padded_length, kPaddingChar);
  std::memcpy(&padded_secret[padded_length - secret.size()], secret.data(),
              secret.size());

  auto pwd_padding_dict =
      base::Value::Dict()
          .Set(kPaddedPassword, padded_secret)
          .Set(kPasswordLength, static_cast<int>(secret.size()));
  SecurelyClearString(padded_secret);

  auto result = base::JSONWriter::Write(pwd_padding_dict, out);
  if (auto* password_value = pwd_padding_dict.FindString(kPaddedPassword)) {
    SecurelyClearString(*password_value);
  }
  return result;
}

// UnpadSecret deserializes given |padded_secret| into json object and tries to
// find padded secret. It then removes the padding and returns original secret.
bool UnpadSecret(const std::string& serialized_padded_secret,
                 std::string* out) {
  std::optional<base::Value::Dict> pwd_padding = base::JSONReader::ReadDict(
      serialized_padded_secret, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!pwd_padding) {
    LOGFN(ERROR) << "Failed to deserialize given secret from json.";
    return false;
  }

  auto* padded_secret = pwd_padding->FindString(kPaddedPassword);
  auto pwd_length = pwd_padding->FindInt(kPasswordLength);

  auto result = true;
  if (!padded_secret || !pwd_length.has_value()) {
    result = false;
  } else {
    out->assign(&(*padded_secret)[padded_secret->size() - *pwd_length],
                *pwd_length);
  }
  SecurelyClearDictionaryValueWithKey(pwd_padding, kPaddedPassword);

  return result;
}

// Encrypts the given |secret| with the provided |public_key|. Returns a vector
// of uint8_t as the encrypted secret.
std::optional<std::vector<uint8_t>> PublicKeyEncrypt(
    const std::string& public_key,
    const std::string& secret) {
  CBS pub_key_cbs;
  CBS_init(&pub_key_cbs, reinterpret_cast<const uint8_t*>(&public_key[0]),
           public_key.size());
  bssl::UniquePtr<EVP_PKEY> pub_key(EVP_parse_public_key(&pub_key_cbs));
  if (!pub_key || CBS_len(&pub_key_cbs)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return std::nullopt;
  }

  RSA* rsa = EVP_PKEY_get0_RSA(pub_key.get());
  if (!rsa) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return std::nullopt;
  }

  // Generate a random session key and random nonce.
  uint8_t session_key_with_nonce[kSessionKeyLength + kNonceLength];
  RAND_bytes(session_key_with_nonce, sizeof(session_key_with_nonce));

  // Encrypt the session key with the RSA public key.
  size_t rsa_len;
  std::vector<uint8_t> ciphertext(RSA_size(rsa));
  if (!RSA_encrypt(rsa, &rsa_len, ciphertext.data(), ciphertext.size(),
                   session_key_with_nonce, sizeof(session_key_with_nonce),
                   RSA_PKCS1_OAEP_PADDING)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return std::nullopt;
  }

  std::string session_key(session_key_with_nonce,
                          session_key_with_nonce + kSessionKeyLength);

  std::string sealed_secret;
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&session_key);
  aead.Seal(secret,
            std::string_view(reinterpret_cast<const char*>(
                                 &session_key_with_nonce[kSessionKeyLength]),
                             kNonceLength),
            /*ad=*/"", &sealed_secret);

  ciphertext.insert(ciphertext.end(), sealed_secret.data(),
                    sealed_secret.data() + sealed_secret.size());
  return ciphertext;
}

// Decrypts the provided |ciphertext| with the given |private_key|. Returns
// an std::optional<std::string> as the decrypted secret.
std::optional<std::string> PrivateKeyDecrypt(
    const std::string& private_key,
    base::span<const uint8_t> ciphertext) {
  CBS priv_key_cbs;
  CBS_init(&priv_key_cbs, reinterpret_cast<const uint8_t*>(&private_key[0]),
           private_key.size());
  bssl::UniquePtr<EVP_PKEY> priv_key(EVP_parse_private_key(&priv_key_cbs));
  if (!priv_key || CBS_len(&priv_key_cbs)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return std::nullopt;
  }

  RSA* rsa = EVP_PKEY_get0_RSA(priv_key.get());
  if (!rsa) {
    LOGFN(ERROR) << "No RSA is found in EVP_PKEY_get0_RSA";
    return std::nullopt;
  }
  const size_t rsa_size = RSA_size(rsa);
  if (ciphertext.size() < rsa_size) {
    LOGFN(ERROR) << "Incorrect RSA size for given cipher text";
    return std::nullopt;
  }

  // Decrypt the encrypted session key using given provided key.
  std::vector<uint8_t> session_key_with_nonce(rsa_size);
  size_t session_key_with_nonce_len;
  if (!RSA_decrypt(rsa, &session_key_with_nonce_len,
                   session_key_with_nonce.data(), session_key_with_nonce.size(),
                   ciphertext.data(), rsa_size, RSA_PKCS1_OAEP_PADDING)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return std::nullopt;
  }
  session_key_with_nonce.resize(session_key_with_nonce_len);

  std::string session_key(session_key_with_nonce.data(),
                          session_key_with_nonce.data() + kSessionKeyLength);

  std::string plaintext;
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&session_key);
  aead.Open(
      std::string_view(reinterpret_cast<const char*>(&ciphertext[rsa_size]),
                       ciphertext.size() - rsa_size),
      std::string_view(reinterpret_cast<const char*>(
                           &session_key_with_nonce[kSessionKeyLength]),
                       kNonceLength),
      /*ad=*/"", &plaintext);

  return plaintext;
}

// Request a new public key and corresponding resource id from the escrow
// service in order to encrypt |password|. |access_token| is used to authorize
// the request on the escrow service. |device_id| is used to identify the device
// making the request. Fills in |encrypted_data| the resource id for the
// encryption key and also with the encryped password.
HRESULT EncryptUserPasswordUsingEscrowService(
    const std::string& access_token,
    const std::string& device_id,
    const std::wstring& password,
    const base::TimeDelta& request_timeout,
    std::optional<base::Value::Dict>& encrypted_data) {
  DCHECK(!encrypted_data);

  std::string resource_id;
  std::string public_key;
  base::Value::Dict request_dict;
  request_dict.Set(kGenerateKeyPairRequestDeviceIdParameterName, device_id);
  std::optional<base::Value> request_result;

  // Fetch the results and extract the |resource_id| for the key and the
  // |public_key| to be used for encryption.
  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGenerateKeyPairUrl(),
      access_token, {}, request_dict, request_timeout, kMaxNumHttpRetries,
      &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  if (!request_result.has_value() || !request_result->is_dict() ||
      !ExtractKeysFromDict(
          request_result->GetDict(),
          {
              {kGenerateKeyPairResponseResourceIdParameterName, &resource_id},
              {kGenerateKeyPairResponsePublicKeyParameterName, &public_key},
          })) {
    return E_FAIL;
  }

  std::string decoded_public_key;
  if (!Base64DecodeCryptographicKey(public_key, &decoded_public_key)) {
    LOGFN(ERROR) << "Failed to base64 decode public key";
    return E_FAIL;
  }

  std::string password_utf8 = base::WideToUTF8(password);
  std::string padded_password;
  auto result = PadSecret(password_utf8, &padded_password);
  SecurelyClearString(password_utf8);
  if (!result) {
    LOGFN(ERROR) << "Failed while padding password";
    return E_FAIL;
  }

  auto opt = PublicKeyEncrypt(decoded_public_key, padded_password);
  SecurelyClearString(padded_password);
  if (opt == std::nullopt) {
    return E_FAIL;
  }

  std::string cipher_text = base::Base64Encode(*opt);

  encrypted_data =
      base::Value::Dict()
          .Set(kUserPasswordLsaStoreIdKey, resource_id)
          .Set(kUserPasswordLsaStoreEncryptedPasswordKey, cipher_text);

  return hr;
}

// Given the |encrypted_data_dict| which would contain the resource id of the
// encryption key and the encrypted password, recovers the |decrypted_password|
// by getting the private key from the escrow service and decrypting the
// password. |access_token| is used to authorize the request on the escrow
// service.
HRESULT DecryptUserPasswordUsingEscrowService(
    const std::string& access_token,
    const base::Value::Dict& encrypted_data_dict,
    const base::TimeDelta& request_timeout,
    std::wstring* decrypted_password) {
  DCHECK(decrypted_password);
  const std::string* resource_id =
      encrypted_data_dict.FindString(kUserPasswordLsaStoreIdKey);
  const std::string* encoded_cipher_text =
      encrypted_data_dict.FindString(kUserPasswordLsaStoreEncryptedPasswordKey);

  if (!resource_id) {
    LOGFN(ERROR) << "No password resource id found to restore";
    return E_FAIL;
  }

  if (!encoded_cipher_text) {
    LOGFN(ERROR) << "No encrypted password found to restore";
    return E_FAIL;
  }

  std::string private_key;
  std::optional<base::Value> request_result;

  // Fetch the results and extract the |private_key| to be used for decryption.
  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGetPrivateKeyUrl(
          *resource_id),
      access_token, {}, {}, request_timeout, kMaxNumHttpRetries,
      &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  if (!request_result.has_value() || !request_result->is_dict() ||
      !ExtractKeysFromDict(
          request_result->GetDict(),
          {
              {kGetPrivateKeyResponsePrivateKeyParameterName, &private_key},
          })) {
    return E_FAIL;
  }

  std::string decoded_cipher_text;
  if (!base::Base64Decode(*encoded_cipher_text, &decoded_cipher_text)) {
    LOGFN(ERROR) << "Failed to base64 decode ciphertext";
    return E_FAIL;
  }

  std::string decoded_private_key;
  if (!Base64DecodeCryptographicKey(private_key, &decoded_private_key)) {
    LOGFN(ERROR) << "Failed to base64 decode private key";
    return E_FAIL;
  }

  auto decrypted_secret =
      PrivateKeyDecrypt(decoded_private_key,
                        base::as_bytes(base::make_span(decoded_cipher_text)));

  if (decrypted_secret == std::nullopt) {
    return E_FAIL;
  }

  std::string unpadded;
  UnpadSecret(*decrypted_secret, &unpadded);
  *decrypted_password = base::UTF8ToWide(unpadded);

  SecurelyClearString(*decrypted_secret);
  SecurelyClearString(unpadded);

  return S_OK;
}

}  // namespace

// static
PasswordRecoveryManager* PasswordRecoveryManager::Get() {
  return *GetInstanceStorage();
}

// static
PasswordRecoveryManager** PasswordRecoveryManager::GetInstanceStorage() {
  static PasswordRecoveryManager instance(
      kDefaultEscrowServiceEncryptionKeyRequestTimeout,
      kDefaultEscrowServiceDecryptionKeyRequestTimeout);
  static PasswordRecoveryManager* instance_storage = &instance;

  return &instance_storage;
}

PasswordRecoveryManager::PasswordRecoveryManager(
    base::TimeDelta encryption_key_timeout,
    base::TimeDelta decryption_key_timeout)
    : encryption_key_request_timeout_(encryption_key_timeout),
      decryption_key_request_timeout_(decryption_key_timeout) {}

PasswordRecoveryManager::~PasswordRecoveryManager() = default;

HRESULT PasswordRecoveryManager::ClearUserRecoveryPassword(
    const std::wstring& sid) {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }
  std::wstring store_key = GetUserPasswordLsaStoreKey(sid);
  return policy->RemovePrivateData(store_key.c_str());
}

HRESULT PasswordRecoveryManager::StoreWindowsPasswordIfNeeded(
    const std::wstring& sid,
    const std::string& access_token,
    const std::wstring& password) {
  if (!PasswordRecoveryEnabled())
    return E_NOTIMPL;

  std::wstring machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to get machine GUID hr=" << putHR(hr);
    return hr;
  }

  std::string device_id = base::WideToUTF8(machine_guid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // See if a password key is already stored in the LSA for this user.
  std::wstring store_key = GetUserPasswordLsaStoreKey(sid);

  if (policy->PrivateDataExists(store_key.c_str())) {
    return S_OK;
  }

  std::optional<base::Value::Dict> encrypted_dict;
  hr = EncryptUserPasswordUsingEscrowService(access_token, device_id, password,
                                             encryption_key_request_timeout_,
                                             encrypted_dict);
  if (SUCCEEDED(hr)) {
    std::string lsa_value;
    if (base::JSONWriter::Write(*encrypted_dict, &lsa_value)) {
      std::wstring lsa_value16 = base::UTF8ToWide(lsa_value);
      hr = policy->StorePrivateData(store_key.c_str(), lsa_value16.c_str());
      SecurelyClearString(lsa_value16);
      SecurelyClearString(lsa_value);

      if (FAILED(hr)) {
        LOGFN(ERROR) << "StorePrivateData hr=" << putHR(hr);
        return hr;
      }

      LOGFN(VERBOSE) << "Encrypted and stored secret for sid=" << sid;
    } else {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return E_FAIL;
    }

    SecurelyClearDictionaryValueWithKey(
        encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);
  } else {
    LOGFN(ERROR) << "EncryptUserPasswordUsingEscrowService hr=" << putHR(hr);
    return E_FAIL;
  }
  return S_OK;
}

HRESULT PasswordRecoveryManager::RecoverWindowsPasswordIfPossible(
    const std::wstring& sid,
    const std::string& access_token,
    std::wstring* recovered_password) {
  if (!PasswordRecoveryEnabled())
    return E_NOTIMPL;

  DCHECK(recovered_password);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // See if a password key is already stored in the LSA for this user.
  std::wstring store_key = GetUserPasswordLsaStoreKey(sid);
  wchar_t password_lsa_data[1024];
  HRESULT hr = policy->RetrievePrivateData(store_key.c_str(), password_lsa_data,
                                           std::size(password_lsa_data));

  if (FAILED(hr))
    LOGFN(ERROR) << "RetrievePrivateData hr=" << putHR(hr);

  std::string json_string = base::WideToUTF8(password_lsa_data);
  std::optional<base::Value::Dict> encrypted_dict =
      base::JSONReader::ReadDict(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  SecurelyClearString(json_string);
  SecurelyClearBuffer(password_lsa_data, sizeof(password_lsa_data));

  if (encrypted_dict) {
    std::wstring decrypted_password;
    hr = DecryptUserPasswordUsingEscrowService(access_token, *encrypted_dict,
                                               decryption_key_request_timeout_,
                                               &decrypted_password);

    SecurelyClearDictionaryValueWithKey(
        encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);

    if (SUCCEEDED(hr)) {
      *recovered_password = decrypted_password;
    }
    SecurelyClearString(decrypted_password);
  }

  LOGFN(VERBOSE) << "Decrypted the secret for sid=" << sid;

  return hr;
}

GURL PasswordRecoveryManager::GetEscrowServiceGenerateKeyPairUrl() {
  if (!PasswordRecoveryEnabled())
    return GURL();

  GURL escrow_service_server = EscrowServiceUrl();

  if (escrow_service_server.is_empty()) {
    LOGFN(ERROR) << "No escrow service server specified";
    return GURL();
  }

  return escrow_service_server.Resolve(kEscrowServiceGenerateKeyPairPath);
}

GURL PasswordRecoveryManager::GetEscrowServiceGetPrivateKeyUrl(
    const std::string& resource_id) {
  if (!PasswordRecoveryEnabled())
    return GURL();

  GURL escrow_service_server = EscrowServiceUrl();

  if (escrow_service_server.is_empty()) {
    LOGFN(ERROR) << "No escrow service server specified";
    return GURL();
  }

  return escrow_service_server.Resolve(
      base::StrCat({kEscrowServiceGetPrivateKeyPath, "/", resource_id}));
}

std::string PasswordRecoveryManager::MakeGenerateKeyPairResponseForTesting(
    const std::string& public_key,
    const std::string& resource_id) {
  return base::StringPrintf(
      R"({"%s": "%s", "%s": "%s"})",
      kGenerateKeyPairResponsePublicKeyParameterName, public_key.c_str(),
      kGenerateKeyPairResponseResourceIdParameterName, resource_id.c_str());
}

std::string PasswordRecoveryManager::MakeGetPrivateKeyResponseForTesting(
    const std::string& private_key) {
  return base::StringPrintf(R"({"%s": "%s"})",
                            kGetPrivateKeyResponsePrivateKeyParameterName,
                            private_key.c_str());
}

}  // namespace credential_provider
