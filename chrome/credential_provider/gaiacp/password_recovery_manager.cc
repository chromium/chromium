// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"

#include <windows.h>
#include <winternl.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
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
        base::TimeDelta::FromMilliseconds(12000);

const base::TimeDelta
    PasswordRecoveryManager::kDefaultEscrowServiceDecryptionKeyRequestTimeout =
        base::TimeDelta::FromMilliseconds(3000);

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

// Self deleting escrow service requester. This class will try to make a query
// using the given url fetcher. It will delete itself when the request is
// completed, either because the request completed successfully within the
// timeout or the request has timed out and is allowed to complete in the
// background without having the result read by anyone.
// There are two situations where the request will be deleted:
// 1. If the background thread making the request returns within the given
// timeout, the function is guaranteed to return the result that was fetched.
// 2. If however the background thread times out there are two potential
// race conditions that can occur:
//    1. The main thread making the request can mark that the background thread
//       is orphaned before it can complete. In this case when the background
//       thread completes it will check whether the request is orphaned and self
//       delete.
//    2. The background thread completes before the main thread can mark the
//       request as orphaned. In this case the background thread will have
//       marked that the request is no longer processing and thus the main
//       thread can self delete.
class EscrowServiceRequest {
 public:
  explicit EscrowServiceRequest(std::unique_ptr<WinHttpUrlFetcher> fetcher)
      : fetcher_(std::move(fetcher)) {
    DCHECK(fetcher_);
  }

  // Tries to fetch the request stored in |fetcher_| in a background thread
  // within the given |request_timeout|. If the background thread returns before
  // the timeout expires, it is guaranteed that a result can be returned and the
  // requester will delete itself.
  base::Optional<base::Value> WaitForResponseFromEscrowService(
      const base::TimeDelta& request_timeout) {
    base::Optional<base::Value> result;

    // Start the thread and wait on its handle until |request_timeout| expires
    // or the thread finishes.
    unsigned wait_thread_id;
    uintptr_t wait_thread = ::_beginthreadex(
        nullptr, 0, &EscrowServiceRequest::FetchResultFromEscrowService,
        reinterpret_cast<void*>(this), 0, &wait_thread_id);

    HRESULT hr = S_OK;
    if (wait_thread == 0) {
      return result;
    } else {
      // Hold the handle in the scoped handle so that it can be immediately
      // closed when the wait is complete allowing the thread to finish
      // completely if needed.
      base::win::ScopedHandle thread_handle(
          reinterpret_cast<HANDLE>(wait_thread));
      hr = ::WaitForSingleObject(thread_handle.Get(),
                                 request_timeout.InMilliseconds());
    }

    // The race condition starts here. It is possible that between the expiry of
    // the timeout in the call for WaitForSingleObject and the call to
    // OrphanRequest, the fetching thread could have finished. So there is a two
    // part handshake. Either the background thread has called ProcessingDone
    // in which case it has already passed its own check for |is_orphaned_| and
    // the call to OrphanRequest should delete this object right now. Otherwise
    // the background thread is still running and will be able to query the
    // |is_orphaned_| state and delete the object after thread completion.
    if (hr != WAIT_OBJECT_0) {
      LOGFN(ERROR) << "Wait for response timed out or failed hr=" << putHR(hr);
      OrphanRequest();
      return result;
    }

    result = base::JSONReader::Read(
        base::StringPiece(response_.data(), response_.size()),
        base::JSON_ALLOW_TRAILING_COMMAS);
    if (!result || !result->is_dict()) {
      LOGFN(ERROR) << "Failed to read json result from server response";
      result.reset();
    }

    delete this;
    return result;
  }

 private:
  void OrphanRequest() {
    bool delete_self = false;
    {
      base::AutoLock locker(orphan_lock_);
      CHECK(!is_orphaned_);
      if (!is_processing_) {
        delete_self = true;
      } else {
        is_orphaned_ = true;
      }
    }

    if (delete_self)
      delete this;
  }

  void ProcessingDone() {
    bool delete_self = false;
    {
      base::AutoLock locker(orphan_lock_);
      CHECK(is_processing_);
      if (is_orphaned_) {
        delete_self = true;
      } else {
        is_processing_ = false;
      }
    }

    if (delete_self)
      delete this;
  }

  // Background thread function that is used to query the request to the
  // escrow service. This thread never times out and simply marks the fetcher
  // as finished processing when it is done.
  static unsigned __stdcall FetchResultFromEscrowService(void* param) {
    DCHECK(param);
    EscrowServiceRequest* requester =
        reinterpret_cast<EscrowServiceRequest*>(param);

    HRESULT hr = requester->fetcher_->Fetch(&requester->response_);
    if (FAILED(hr))
      LOGFN(ERROR) << "fetcher.Fetch hr=" << putHR(hr);

    requester->ProcessingDone();
    return 0;
  }

  base::Lock orphan_lock_;
  std::unique_ptr<WinHttpUrlFetcher> fetcher_;
  std::vector<char> response_;
  bool is_orphaned_ = false;
  bool is_processing_ = true;
};

// Builds the required json request to be sent to the escrow service and fetches
// the json response from the escrow service (if any). Returns S_OK if
// |needed_outputs| can be filled correctly with the requested data, otherwise
// returns an error code.
// |request_url| is the full query url from which to fetch a response.
// |headers| are all the header key value pairs to be sent with the request.
// |parameters| are all the json parameters to be sent with the request. This
// argument will be converted to a json string and sent as part of the body of
// the request.
// |request_timeout| is the maximum time to wait for a response.
// |needed_outputs| is the mapping of the desired result key to an address where
// the result can be stored.
// If any |needed_outputs| is missing, all of the outputs are cleared.
HRESULT BuildRequestAndFetchResultFromEscrowService(
    const GURL& request_url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::vector<std::pair<std::string, std::string>>& parameters,
    const UrlFetchResultNeedOutputs& needed_outputs,
    const base::TimeDelta& request_timeout) {
  DCHECK(needed_outputs.size());

  if (request_url.is_empty()) {
    LOGFN(ERROR) << "No escrow service url specified";
    return E_FAIL;
  }

  auto url_fetcher = WinHttpUrlFetcher::Create(request_url);
  if (!url_fetcher) {
    LOGFN(ERROR) << "Could not create valid fetcher for url="
                 << request_url.spec();
    return E_FAIL;
  }

  url_fetcher->SetRequestHeader("Content-Type", "application/json");

  for (auto& header : headers)
    url_fetcher->SetRequestHeader(header.first.c_str(), header.second.c_str());

  HRESULT hr = S_OK;

  if (!parameters.empty()) {
    base::Value request_dict(base::Value::Type::DICTIONARY);

    for (auto& parameter : parameters)
      request_dict.SetStringKey(parameter.first, parameter.second);

    std::string json;
    if (!base::JSONWriter::Write(request_dict, &json)) {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return E_FAIL;
    }

    hr = url_fetcher->SetRequestBody(json.c_str());
    if (FAILED(hr)) {
      LOGFN(ERROR) << "fetcher.SetRequestBody hr=" << putHR(hr);
      return E_FAIL;
    }
  }

  base::Optional<base::Value> request_result =
      (new EscrowServiceRequest(std::move(url_fetcher)))
          ->WaitForResponseFromEscrowService(request_timeout);

  if (!request_result)
    return E_FAIL;

  for (const std::pair<std::string, std::string*>& output : needed_outputs) {
    const std::string* output_value =
        request_result->FindStringKey(output.first);
    if (!output_value) {
      LOGFN(ERROR) << "Could not extract value '" << output.first
                   << "' from server response";
      hr = E_FAIL;
      break;
    }
    DCHECK(output.second);
    *output.second = *output_value;
  }

  if (FAILED(hr)) {
    for (const std::pair<std::string, std::string*>& output : needed_outputs)
      output.second->clear();
  }

  return hr;
}

// Makes a standard: "Authorization: Bearer $TOKEN" header for passing
// authorization information to a server.
std::pair<std::string, std::string> MakeAuthorizationHeader(
    const std::string& access_token) {
  return {"Authorization", "Bearer " + access_token};
}

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
  LOGFN(ERROR) << base::StringPiece(str, len);
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

  base::Value pwd_padding_dict(base::Value::Type::DICTIONARY);
  pwd_padding_dict.SetStringKey(kPaddedPassword, padded_secret);
  pwd_padding_dict.SetIntKey(kPasswordLength, secret.size());
  SecurelyClearString(padded_secret);

  auto result = base::JSONWriter::Write(pwd_padding_dict, out);
  const std::string* password_value =
      pwd_padding_dict.FindStringKey(kPaddedPassword);
  if (password_value)
    SecurelyClearString(*const_cast<std::string*>(password_value));

  return result;
}

// UnpadSecret deserializes given |padded_secret| into json object and tries to
// find padded secret. It then removes the padding and returns original secret.
bool UnpadSecret(const std::string& serialized_padded_secret,
                 std::string* out) {
  base::Optional<base::Value> pwd_padding_dict = base::JSONReader::Read(
      serialized_padded_secret, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!pwd_padding_dict.has_value() || !pwd_padding_dict->is_dict()) {
    LOGFN(ERROR) << "Failed to deserialize given secret from json.";
    return false;
  }

  auto* padded_secret = pwd_padding_dict->FindStringKey(kPaddedPassword);
  auto pwd_length = pwd_padding_dict->FindIntKey(kPasswordLength);

  auto result = true;
  if (!padded_secret || !pwd_length.has_value()) {
    result = false;
  } else {
    out->assign(&(*padded_secret)[padded_secret->size() - *pwd_length],
                *pwd_length);
  }
  SecurelyClearDictionaryValueWithKey(&pwd_padding_dict, kPaddedPassword);

  return result;
}

// Encrypts the given |secret| with the provided |public_key|. Returns a vector
// of uint8_t as the encrypted secret.
base::Optional<std::vector<uint8_t>> PublicKeyEncrypt(
    const std::string& public_key,
    const std::string& secret) {
  CBS pub_key_cbs;
  CBS_init(&pub_key_cbs, reinterpret_cast<const uint8_t*>(&public_key[0]),
           public_key.size());
  bssl::UniquePtr<EVP_PKEY> pub_key(EVP_parse_public_key(&pub_key_cbs));
  if (!pub_key || CBS_len(&pub_key_cbs)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return base::nullopt;
  }

  RSA* rsa = EVP_PKEY_get0_RSA(pub_key.get());
  if (!rsa) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return base::nullopt;
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
    return base::nullopt;
  }

  std::string session_key(session_key_with_nonce,
                          session_key_with_nonce + kSessionKeyLength);

  std::string sealed_secret;
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&session_key);
  aead.Seal(secret,
            base::StringPiece(reinterpret_cast<const char*>(
                                  &session_key_with_nonce[kSessionKeyLength]),
                              kNonceLength),
            /*ad=*/nullptr, &sealed_secret);

  ciphertext.insert(ciphertext.end(), sealed_secret.data(),
                    sealed_secret.data() + sealed_secret.size());
  return ciphertext;
}

// Decrypts the provided |ciphertext| with the given |private_key|. Returns
// an base::Optional<std::string> as the decrypted secret.
base::Optional<std::string> PrivateKeyDecrypt(
    const std::string& private_key,
    base::span<const uint8_t> ciphertext) {
  CBS priv_key_cbs;
  CBS_init(&priv_key_cbs, reinterpret_cast<const uint8_t*>(&private_key[0]),
           private_key.size());
  bssl::UniquePtr<EVP_PKEY> priv_key(EVP_parse_private_key(&priv_key_cbs));
  if (!priv_key || CBS_len(&priv_key_cbs)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return base::nullopt;
  }

  RSA* rsa = EVP_PKEY_get0_RSA(priv_key.get());
  if (!rsa) {
    LOGFN(ERROR) << "No RSA is found in EVP_PKEY_get0_RSA";
    return base::nullopt;
  }
  const size_t rsa_size = RSA_size(rsa);
  if (ciphertext.size() < rsa_size) {
    LOGFN(ERROR) << "Incorrect RSA size for given cipher text";
    return base::nullopt;
  }

  // Decrypt the encrypted session key using given provided key.
  std::vector<uint8_t> session_key_with_nonce(rsa_size);
  size_t session_key_with_nonce_len;
  if (!RSA_decrypt(rsa, &session_key_with_nonce_len,
                   session_key_with_nonce.data(), session_key_with_nonce.size(),
                   ciphertext.data(), rsa_size, RSA_PKCS1_OAEP_PADDING)) {
    ERR_print_errors_cb(&LogBoringSSLError, /*unused*/ nullptr);
    return base::nullopt;
  }
  session_key_with_nonce.resize(session_key_with_nonce_len);

  std::string session_key(session_key_with_nonce.data(),
                          session_key_with_nonce.data() + kSessionKeyLength);

  std::string plaintext;
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&session_key);
  aead.Open(
      base::StringPiece(reinterpret_cast<const char*>(&ciphertext[rsa_size]),
                        ciphertext.size() - rsa_size),
      base::StringPiece(reinterpret_cast<const char*>(
                            &session_key_with_nonce[kSessionKeyLength]),
                        kNonceLength),
      /*ad=*/nullptr, &plaintext);

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
    const base::string16& password,
    const base::TimeDelta& request_timeout,
    base::Optional<base::Value>* encrypted_data) {
  DCHECK(encrypted_data);
  DCHECK(!(*encrypted_data));

  std::string resource_id;
  std::string public_key;

  // Fetch the results and extract the |resource_id| for the key and the
  // |public_key| to be used for encryption.
  HRESULT hr = BuildRequestAndFetchResultFromEscrowService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGenerateKeyPairUrl(),
      {MakeAuthorizationHeader(access_token)},
      {{kGenerateKeyPairRequestDeviceIdParameterName, device_id}},
      {
          {kGenerateKeyPairResponseResourceIdParameterName, &resource_id},
          {kGenerateKeyPairResponsePublicKeyParameterName, &public_key},
      },
      request_timeout);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromEscrowService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  std::string decoded_public_key;
  if (!Base64DecodeCryptographicKey(public_key, &decoded_public_key)) {
    LOGFN(ERROR) << "Failed to base64 decode public key";
    return E_FAIL;
  }

  std::string password_utf8 = base::UTF16ToUTF8(password);
  std::string padded_password;
  auto result = PadSecret(password_utf8, &padded_password);
  SecurelyClearString(password_utf8);
  if (!result) {
    LOGFN(ERROR) << "Failed while padding password";
    return E_FAIL;
  }

  auto opt = PublicKeyEncrypt(decoded_public_key, padded_password);
  SecurelyClearString(padded_password);
  if (opt == base::nullopt)
    return E_FAIL;

  encrypted_data->emplace(base::Value(base::Value::Type::DICTIONARY));
  (*encrypted_data)->SetStringKey(kUserPasswordLsaStoreIdKey, resource_id);

  std::string cipher_text;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(opt->data()),
                        opt->size()),
      &cipher_text);
  (*encrypted_data)
      ->SetStringKey(kUserPasswordLsaStoreEncryptedPasswordKey, cipher_text);

  return hr;
}

// Given the |encrypted_data| which would contain the resource id of the
// encryption key and the encrypted password, recovers the |decrypted_password|
// by getting the private key from the escrow service and decrypting the
// password. |access_token| is used to authorize the request on the escrow
// service.
HRESULT DecryptUserPasswordUsingEscrowService(
    const std::string& access_token,
    const base::Optional<base::Value>& encrypted_data,
    const base::TimeDelta& request_timeout,
    base::string16* decrypted_password) {
  if (!encrypted_data)
    return E_FAIL;
  DCHECK(decrypted_password);
  DCHECK(encrypted_data && encrypted_data->is_dict());
  const std::string* resource_id =
      encrypted_data->FindStringKey(kUserPasswordLsaStoreIdKey);
  const std::string* encoded_cipher_text =
      encrypted_data->FindStringKey(kUserPasswordLsaStoreEncryptedPasswordKey);

  if (!resource_id) {
    LOGFN(ERROR) << "No password resource id found to restore";
    return E_FAIL;
  }

  if (!encoded_cipher_text) {
    LOGFN(ERROR) << "No encrypted password found to restore";
    return E_FAIL;
  }

  std::string private_key;

  // Fetch the results and extract the |private_key| to be used for decryption.
  HRESULT hr = BuildRequestAndFetchResultFromEscrowService(
      PasswordRecoveryManager::Get()->GetEscrowServiceGetPrivateKeyUrl(
          *resource_id),
      {MakeAuthorizationHeader(access_token)}, {},
      {
          {kGetPrivateKeyResponsePrivateKeyParameterName, &private_key},
      },
      request_timeout);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromEscrowService hr="
                 << putHR(hr);
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

  if (decrypted_secret == base::nullopt)
    return E_FAIL;

  std::string unpadded;
  UnpadSecret(*decrypted_secret, &unpadded);
  *decrypted_password = base::UTF8ToUTF16(unpadded);

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
    const base::string16& sid) {
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }
  base::string16 store_key = GetUserPasswordLsaStoreKey(sid);
  return policy->RemovePrivateData(store_key.c_str());
}

HRESULT PasswordRecoveryManager::StoreWindowsPasswordIfNeeded(
    const base::string16& sid,
    const std::string& access_token,
    const base::string16& password) {
  if (!PasswordRecoveryEnabled())
    return E_NOTIMPL;

  base::string16 machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to get machine GUID hr=" << putHR(hr);
    return hr;
  }

  std::string device_id = base::UTF16ToUTF8(machine_guid);

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  // See if a password key is already stored in the LSA for this user.
  base::string16 store_key = GetUserPasswordLsaStoreKey(sid);

  if (policy->PrivateDataExists(store_key.c_str())) {
    return S_OK;
  }

  base::Optional<base::Value> encrypted_dict;
  hr = EncryptUserPasswordUsingEscrowService(access_token, device_id, password,
                                             encryption_key_request_timeout_,
                                             &encrypted_dict);
  if (SUCCEEDED(hr)) {
    std::string lsa_value;
    if (base::JSONWriter::Write(encrypted_dict.value(), &lsa_value)) {
      base::string16 lsa_value16 = base::UTF8ToUTF16(lsa_value);
      hr = policy->StorePrivateData(store_key.c_str(), lsa_value16.c_str());
      SecurelyClearString(lsa_value16);
      SecurelyClearString(lsa_value);

      if (FAILED(hr)) {
        LOGFN(ERROR) << "StorePrivateData hr=" << putHR(hr);
        return hr;
      }

      LOGFN(INFO) << "Encrypted and stored secret for sid=" << sid;
    } else {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return E_FAIL;
    }

    SecurelyClearDictionaryValueWithKey(
        &encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);
  } else {
    LOGFN(ERROR) << "EncryptUserPasswordUsingEscrowService hr=" << putHR(hr);
    return E_FAIL;
  }
  return S_OK;
}

HRESULT PasswordRecoveryManager::RecoverWindowsPasswordIfPossible(
    const base::string16& sid,
    const std::string& access_token,
    base::string16* recovered_password) {
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
  base::string16 store_key = GetUserPasswordLsaStoreKey(sid);
  wchar_t password_lsa_data[1024];
  HRESULT hr = policy->RetrievePrivateData(store_key.c_str(), password_lsa_data,
                                           base::size(password_lsa_data));

  if (FAILED(hr))
    LOGFN(ERROR) << "RetrievePrivateData hr=" << putHR(hr);

  std::string json_string = base::UTF16ToUTF8(password_lsa_data);
  base::Optional<base::Value> encrypted_dict =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  SecurelyClearString(json_string);
  SecurelyClearBuffer(password_lsa_data, sizeof(password_lsa_data));

  base::string16 decrypted_password;
  hr = DecryptUserPasswordUsingEscrowService(access_token, encrypted_dict,
                                             decryption_key_request_timeout_,
                                             &decrypted_password);

  if (encrypted_dict) {
    SecurelyClearDictionaryValueWithKey(
        &encrypted_dict, kUserPasswordLsaStoreEncryptedPasswordKey);
  }

  if (SUCCEEDED(hr))
    *recovered_password = decrypted_password;
  SecurelyClearString(decrypted_password);

  LOGFN(INFO) << "Decrypted the secret for sid=" << sid;

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
