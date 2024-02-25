// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/chaps_util_impl.h"

#include <dlfcn.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <pkcs11.h>
#include <pkcs11t.h>
#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "crypto/chaps_support.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"
#include "third_party/boringssl/src/include/openssl/stack.h"

namespace chromeos {

namespace {

// TODO(b/202374261): Move these into a shared header.
// Signals to chaps that a generated key should be software-backed.
constexpr CK_ATTRIBUTE_TYPE kForceSoftwareAttribute = CKA_VENDOR_DEFINED + 4;
// Chaps sets this for keys that are software-backed.
constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;
struct KeyPairHandles {
  CK_OBJECT_HANDLE public_key;
  CK_OBJECT_HANDLE private_key;
};

using Pkcs11Operation = base::RepeatingCallback<CK_RV()>;

// Performs |operation| and handles return values indicating that the PKCS11
// session has been closed by attempting to re-open the |chaps_session|.
// This is useful because the session could be closed e.g. because NSS could
// have called C_CloseAllSessions.
bool PerformWithRetries(ChapsSlotSession* chaps_session,
                        std::string_view operation_name,
                        const Pkcs11Operation& operation) {
  const int kMaxAttempts = 5;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    CK_RV result = operation.Run();
    if (result == CKR_OK) {
      return true;
    }
    if (result != CKR_SESSION_HANDLE_INVALID && result != CKR_SESSION_CLOSED) {
      LOG(ERROR) << operation_name << " failed with " << result;
      return false;
    }
    if (!chaps_session->ReopenSession()) {
      return false;
    }
  }
  LOG(ERROR) << operation_name << " failed";
  return false;
}

// Uses |chaps_session| to generate a software-backed RSA key pair with modulus
// length |num_bits|.
std::optional<KeyPairHandles> GenerateSoftwareBackedRSAKeyPair(
    ChapsSlotSession* chaps_session,
    uint16_t num_bits) {
  CK_ULONG modulus_bits = num_bits;
  CK_BBOOL true_value = CK_TRUE;
  CK_BBOOL false_value = CK_FALSE;
  CK_BYTE public_exponent[3] = {0x01, 0x00, 0x01};  // 65537

  // Public key attributes
  // Note: CKA_ID is set later (computed from the public key modulus) and
  // CKA_LABEL is not set to match NSS behavior
  // (https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/PKCS11_Implement).
  CK_ATTRIBUTE pub_attributes[] = {
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_PRIVATE, &false_value, sizeof(false_value)},
      {CKA_VERIFY, &true_value, sizeof(true_value)},
      {CKA_MODULUS_BITS, &modulus_bits, sizeof(modulus_bits)},
      {CKA_PUBLIC_EXPONENT, public_exponent, sizeof(public_exponent)}};

  // Private key attributes
  // Note: CKA_ID is set later (computed from the public key modulus) and
  // CKA_LABEL is not set to match NSS behavior
  // (https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/PKCS11_Implement).
  CK_ATTRIBUTE priv_attributes[] = {
      {CKA_TOKEN, &true_value, sizeof(true_value)},
      {CKA_PRIVATE, &true_value, sizeof(true_value)},
      {CKA_SENSITIVE, &true_value, sizeof(true_value)},
      {CKA_EXTRACTABLE, &false_value, sizeof(false_value)},
      {kForceSoftwareAttribute, &true_value, sizeof(true_value)},
      {CKA_SIGN, &true_value, sizeof(true_value)}};
  CK_MECHANISM mechanism = {CKM_RSA_PKCS_KEY_PAIR_GEN, /* pParameter */ nullptr,
                            /* ulParameterLen*/ 0};

  KeyPairHandles key_pair;

  if (!PerformWithRetries(
          chaps_session, "GenerateKeyPair",
          base::BindRepeating(&ChapsSlotSession::GenerateKeyPair,
                              base::Unretained(chaps_session), &mechanism,
                              pub_attributes, std::size(pub_attributes),
                              priv_attributes, std::size(priv_attributes),
                              &(key_pair.public_key),
                              &(key_pair.private_key)))) {
    return {};
  }
  return key_pair;
}

// Read the modulus of the public key identified by |pub_key_handle| and return
// it.
std::optional<std::vector<CK_BYTE>> ExtractModulus(
    ChapsSlotSession* chaps_session,
    CK_OBJECT_HANDLE pub_key_handle) {
  std::vector<CK_BYTE> modulus(256);
  CK_ATTRIBUTE attrs_get_modulus[] = {
      {CKA_MODULUS, modulus.data(), modulus.size()}};
  if (!PerformWithRetries(
          chaps_session, "GetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::GetAttributeValue,
                              base::Unretained(chaps_session), pub_key_handle,
                              attrs_get_modulus,
                              std::size(attrs_get_modulus)))) {
    return {};
  }
  return modulus;
}

crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes) {
  SECItem secitem_modulus;
  secitem_modulus.data = const_cast<uint8_t*>(public_key_bytes.data());
  secitem_modulus.len = public_key_bytes.size();
  return crypto::ScopedSECItem(PK11_MakeIDFromPubKey(&secitem_modulus));
}

// Read the modulus of the public key identified by |pub_key_handle| and return
// it.
std::optional<bool> IsKeySoftwareBacked(ChapsSlotSession* chaps_session,
                                        CK_OBJECT_HANDLE private_key_handle) {
  CK_BBOOL key_in_software = CK_FALSE;
  CK_ATTRIBUTE attrs_get_key_in_software[] = {
      {kKeyInSoftware, &key_in_software, sizeof(key_in_software)}};
  if (!PerformWithRetries(
          chaps_session, "GetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::GetAttributeValue,
                              base::Unretained(chaps_session),
                              private_key_handle, attrs_get_key_in_software,
                              std::size(attrs_get_key_in_software)))) {
    return {};
  }
  return key_in_software;
}

// Create the CKA_ID value that NSS would use for |key_pair| and return it.
crypto::ScopedSECItem CreateNssCkaId(ChapsSlotSession* chaps_session,
                                     const KeyPairHandles& key_pair) {
  auto modulus = ExtractModulus(chaps_session, key_pair.public_key);
  if (!modulus) {
    return nullptr;
  }
  return MakeIdFromPubKeyNss(modulus.value());
}

// Set the CKA_ID attribute of the public and private key objects in |key_pair|
// to |cka_id|.
bool SetCkaId(ChapsSlotSession* chaps_session,
              KeyPairHandles& key_pair,
              SECItem* cka_id) {
  CK_ATTRIBUTE attrs_set_id[] = {{CKA_ID, cka_id->data, cka_id->len}};
  if (!PerformWithRetries(
          chaps_session, "SetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::SetAttributeValue,
                              base::Unretained(chaps_session),
                              key_pair.private_key, attrs_set_id,
                              std::size(attrs_set_id)))) {
    return false;
  }
  if (!PerformWithRetries(
          chaps_session, "SetAttributeValue",
          base::BindRepeating(&ChapsSlotSession::SetAttributeValue,
                              base::Unretained(chaps_session),
                              key_pair.public_key, attrs_set_id,
                              std::size(attrs_set_id)))) {
    return false;
  }
  return true;
}

}  // namespace

ChapsUtilImpl::ChapsUtilImpl(
    std::unique_ptr<ChapsSlotSessionFactory> chaps_slot_session_factory)
    : chaps_slot_session_factory_(std::move(chaps_slot_session_factory)) {}
ChapsUtilImpl::~ChapsUtilImpl() = default;

bool ChapsUtilImpl::GenerateSoftwareBackedRSAKey(
    PK11SlotInfo* slot,
    uint16_t num_bits,
    crypto::ScopedSECKEYPublicKey* out_public_key,
    crypto::ScopedSECKEYPrivateKey* out_private_key) {
  DCHECK(out_public_key);
  DCHECK(out_private_key);

  std::unique_ptr<ChapsSlotSession> chaps_session =
      GetChapsSlotSessionForSlot(slot);
  if (!chaps_session) {
    return false;
  }

  std::optional<KeyPairHandles> key_pair =
      GenerateSoftwareBackedRSAKeyPair(chaps_session.get(), num_bits);
  if (!key_pair) {
    return false;
  }

  // Safety check that software-backed key generation was triggered.
  std::optional<bool> is_software_backed =
      IsKeySoftwareBacked(chaps_session.get(), key_pair->private_key);
  if (!is_software_backed || !is_software_backed.value()) {
    return false;
  }

  crypto::ScopedSECItem cka_id =
      CreateNssCkaId(chaps_session.get(), key_pair.value());
  if (!cka_id) {
    return false;
  }
  if (!SetCkaId(chaps_session.get(), key_pair.value(), cka_id.get())) {
    return false;
  }

  out_private_key->reset(PK11_FindKeyByKeyID(slot, cka_id.get(), nullptr));
  if (!*out_private_key) {
    LOG(ERROR) << "Failed to find private key.";
    return false;
  }
  out_public_key->reset(SECKEY_ConvertToPublicKey(out_private_key->get()));
  if (!*out_public_key) {
    LOG(ERROR) << "Failed to extract public key.";
    return false;
  }
  return true;
}

std::unique_ptr<ChapsSlotSession> ChapsUtilImpl::GetChapsSlotSessionForSlot(
    PK11SlotInfo* slot) {
  if (!slot || (!is_chaps_provided_slot_for_testing_ &&
                !crypto::IsSlotProvidedByChaps(slot))) {
    return nullptr;
  }

  // Note that ChapsSlotSession(Factory) expects something else to have called
  // C_Initialize. It is a safe assumption that NSS has called C_Initialize for
  // chaps if |slot| is actually a chaps-provided slot, which is verified above.
  return chaps_slot_session_factory_->CreateChapsSlotSession(
      PK11_GetSlotID(slot));
}

}  // namespace chromeos
