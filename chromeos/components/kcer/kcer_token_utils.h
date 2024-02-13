// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_
#define CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_

#include "chromeos/components/kcer/attributes.pb.h"
#include "chromeos/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/components/kcer/helpers/pkcs12_reader.h"
#include "chromeos/components/kcer/kcer.h"

namespace kcer::internal {

// Adds an attribute with the given `type` to `attr_list` and sets the value to
// `data`.
void AddAttribute(chaps::AttributeList& attr_list,
                  chromeos::PKCS11_CK_ATTRIBUTE_TYPE type,
                  base::span<const uint8_t> data);

// Reinterprets the `value` as a sequence of bytes and returns it as a span.
// `T` must be a simple type, i.e. no internal pointers, etc.
// `value` must outlive the returned span.
template <typename T>
base::span<const uint8_t> MakeSpan(T* value) {
  static_assert(std::is_integral_v<T>);
  return base::as_bytes(base::span<T>(value, /*count=*/1u));
}

// Calculate PKCS#11 id (see CKA_ID) from the bytes of the public key. Designed
// to be backwards compatible with ids produced by NSS.
Pkcs11Id MakePkcs11Id(base::span<const uint8_t> public_key_data);

// Creates Public key SPKI for an RSA public key from its `modulus` and
// `exponent`.
PublicKeySpki MakeRsaSpki(const base::span<const uint8_t>& modulus,
                          const base::span<const uint8_t>& exponent);

// Creates kcer::PublicKey from an RSA public key data.
base::expected<PublicKey, Error> MakeRsaPublicKey(
    Token token,
    base::span<const uint8_t> modulus,
    base::span<const uint8_t> public_exponent);

// Creates Public key SPKI for an EC public key from its `ec_point`.
PublicKeySpki MakeEcSpki(const base::span<const uint8_t>& ec_point);

// Creates kcer::PublicKey from an EC public key data.
base::expected<PublicKey, Error> MakeEcPublicKey(
    Token token,
    base::span<const uint8_t> ec_point);

// Temporary class to share code between KcerTokenImpl and KcerTokenImplNss. Can
// be merged into KcerTokenImpl when KcerTokenImplNss is removed. Mostly
// contains operations that have to communicate with Chaps directly.
class KcerTokenUtils {
 public:
  using ObjectHandle = SessionChapsClient::ObjectHandle;

  // `chaps_client` must outlive KcerTokenUtils.
  KcerTokenUtils(Token token, HighLevelChapsClient* chaps_client);
  ~KcerTokenUtils();

  // Should be called before any other methods.
  void Initialize(SessionChapsClient::SlotId pkcs_11_slot_id);

  // Returns handles for private key objects with PKCS#11 `id` and PKCS11_CKR_OK
  // on success, or some other result code on failure. (In practice there should
  // be only 1 or 0 handles.)
  void FindPrivateKey(Pkcs11Id id,
                      base::OnceCallback<void(std::vector<ObjectHandle>,
                                              uint32_t result_code)> callback);

  // Creates a certificate object in Chaps. Does not check whether such an
  // object already exists. If `kcer_error` is not empty - import failed without
  // talking with Chaps. Otherwise returns the result from Chaps.
  void ImportCert(bssl::UniquePtr<X509> cert,
                  Pkcs11Id pkcs11_id,
                  std::string nickname,
                  CertDer cert_der,
                  base::OnceCallback<void(std::optional<Error> kcer_error,
                                          ObjectHandle cert_handle,
                                          uint32_t result_code)> callback);

  // Imports an EVP_KEY into Chaps as a pair of public and private objects.
  // Skips the actual import if the key already exists.
  struct ImportKeyTask {
    ImportKeyTask(KeyData in_key_data, Kcer::GenerateKeyCallback in_callback);
    ImportKeyTask(ImportKeyTask&& other);
    ~ImportKeyTask();

    KeyData key_data;
    Kcer::GenerateKeyCallback callback;
    int attemps_left = 5;
  };
  void ImportKey(ImportKeyTask task);

 private:
  void ImportRsaKey(ImportKeyTask task);
  void ImportRsaKeyWithExistingKey(ImportKeyTask task,
                                   bssl::UniquePtr<RSA> rsa_key,
                                   PublicKey kcer_public_key,
                                   std::vector<ObjectHandle> handles,
                                   uint32_t result_code);
  void DidImportRsaPrivateKey(ImportKeyTask task,
                              PublicKey kcer_public_key,
                              std::vector<uint8_t> public_modulus_bytes,
                              std::vector<uint8_t> public_exponent_bytes,
                              ObjectHandle priv_key_handle,
                              uint32_t result_code);
  void ImportEcKey(ImportKeyTask task);
  void ImportEcKeyWithExistingKey(ImportKeyTask task,
                                  bssl::UniquePtr<EC_KEY> ec_key,
                                  PublicKey kcer_public_key,
                                  std::vector<uint8_t> ec_point_oct,
                                  std::vector<ObjectHandle> handles,
                                  uint32_t result_code);
  void DidImportEcPrivateKey(ImportKeyTask task,
                             PublicKey kcer_public_key,
                             std::vector<uint8_t> ec_point_der,
                             std::vector<uint8_t> ec_params_der,
                             ObjectHandle priv_key_handle,
                             uint32_t result_code);
  void DidImportKey(ImportKeyTask task,
                    PublicKey kcer_public_key,
                    ObjectHandle priv_key_handle,
                    ObjectHandle pub_key_handle,
                    uint32_t result_code);

  const Token token_;
  // The id of the slot associated with this token. It's used to perform D-Bus
  // requests to Chaps. The default value is very unlikely to represent any real
  // slot and is not used until it's overwritten in Initialize.
  SessionChapsClient::SlotId pkcs_11_slot_id_ =
      SessionChapsClient::SlotId(0xFFFFFFFF);
  const raw_ptr<HighLevelChapsClient> chaps_client_;
  base::WeakPtrFactory<KcerTokenUtils> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_
