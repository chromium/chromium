// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_mapper.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_validator.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace onc {
namespace {

// Error messages that can be reported when decrypting encrypted ONC.
constexpr char kUnableToDecrypt[] = "Unable to decrypt encrypted ONC";
constexpr char kUnableToDecode[] = "Unable to decode encrypted ONC";

bool GetString(const base::Value& dict, const char* key, std::string* result) {
  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  if (!value)
    return false;
  *result = value->GetString();
  return true;
}

bool GetInt(const base::Value& dict, const char* key, int* result) {
  const base::Value* value =
      dict.FindKeyOfType(key, base::Value::Type::INTEGER);
  if (!value)
    return false;
  *result = value->GetInt();
  return true;
}

// Runs |variable_expander.ExpandString| on the field |fieldname| in
// |onc_object|.
void ExpandField(const std::string& fieldname,
                 const VariableExpander& variable_expander,
                 base::Value::Dict* onc_object) {
  std::string* field_value = onc_object->FindString(fieldname);
  if (!field_value)
    return;
  variable_expander.ExpandString(field_value);
}

bool CanContainPasswordPlaceholder(const std::string& field_name,
                                   const OncValueSignature& object_signature) {
  return (&object_signature == &kEAPSignature &&
          field_name == ::onc::eap::kPassword) ||
         (&object_signature == &kL2TPSignature &&
          field_name == ::onc::l2tp::kPassword);
}

bool IsUserLoginPasswordPlaceholder(const std::string& field_name,
                                    const OncValueSignature& object_signature,
                                    const base::Value& onc_value) {
  if (!CanContainPasswordPlaceholder(field_name, object_signature)) {
    return false;
  }
  DCHECK(onc_value.is_string());
  return onc_value.GetString() ==
         ::onc::substitutes::kPasswordPlaceholderVerbatim;
}

// A |Mapper| for masking sensitive fields (e.g. credentials such as
// passphrases) in ONC.
class OncMaskValues : public Mapper {
 public:
  static base::Value Mask(const OncValueSignature& signature,
                          const base::Value& onc_object,
                          const std::string& mask) {
    OncMaskValues masker(mask);
    bool error = false;
    base::Value result(
        masker.MapObject(signature, onc_object.GetDict(), &error));
    return result;
  }

 protected:
  explicit OncMaskValues(const std::string& mask) : mask_(mask) {}

  base::Value MapField(const std::string& field_name,
                       const OncValueSignature& object_signature,
                       const base::Value& onc_value,
                       bool* found_unknown_field,
                       bool* error) override {
    if (FieldIsCredential(object_signature, field_name)) {
      // If it's the password field and the substitution string is used, don't
      // mask it.
      if (IsUserLoginPasswordPlaceholder(field_name, object_signature,
                                         onc_value)) {
        return Mapper::MapField(field_name, object_signature, onc_value,
                                found_unknown_field, error);
      }
      return base::Value(mask_);
    } else {
      return Mapper::MapField(field_name, object_signature, onc_value,
                              found_unknown_field, error);
    }
  }

 private:
  // Mask to insert in place of the sensitive values.
  std::string mask_;
};

// Returns a map GUID->PEM of all server and authority certificates defined in
// the Certificates section of ONC, which is passed in as |certificates|.
CertPEMsByGUIDMap GetServerAndCACertsByGUID(
    const base::Value::List& certificates) {
  CertPEMsByGUIDMap certs_by_guid;
  for (const auto& cert : certificates) {
    DCHECK(cert.is_dict());

    const std::string* guid = cert.FindStringKey(::onc::certificate::kGUID);
    if (!guid || guid->empty()) {
      NET_LOG(ERROR) << "Certificate with missing or empty GUID.";
      continue;
    }
    const std::string* cert_type =
        cert.FindStringKey(::onc::certificate::kType);
    DCHECK(cert_type);
    if (*cert_type != ::onc::certificate::kServer &&
        *cert_type != ::onc::certificate::kAuthority) {
      continue;
    }
    const std::string* x509_data =
        cert.FindStringKey(::onc::certificate::kX509);
    std::string der;
    if (x509_data)
      der = DecodePEM(*x509_data);
    std::string pem;
    if (der.empty() || !net::X509Certificate::GetPEMEncodedFromDER(der, &pem)) {
      NET_LOG(ERROR) << "Certificate not PEM encoded, GUID: " << *guid;
      continue;
    }
    certs_by_guid[*guid] = pem;
  }

  return certs_by_guid;
}

// Fills HexSSID fields in all entries in the |network_configs| list.
void FillInHexSSIDFieldsInNetworks(base::Value::List& network_configs) {
  for (auto& network : network_configs) {
    FillInHexSSIDFieldsInOncObject(kNetworkConfigurationSignature,
                                   network.GetDict());
  }
}

// Sets HiddenSSID fields in all entries in the |network_configs| list.
void SetHiddenSSIDFieldsInNetworks(base::Value::List& network_configs) {
  for (auto& network : network_configs) {
    SetHiddenSSIDFieldInOncObject(kNetworkConfigurationSignature,
                                  network.GetDict());
  }
}

// Given a GUID->PEM certificate mapping |certs_by_guid|, looks up the PEM
// encoded certificate referenced by |guid_ref|. If a match is found, sets
// |*pem_encoded| to the PEM encoded certificate and returns true. Otherwise,
// returns false.
bool GUIDRefToPEMEncoding(const CertPEMsByGUIDMap& certs_by_guid,
                          const std::string& guid_ref,
                          std::string* pem_encoded) {
  CertPEMsByGUIDMap::const_iterator it = certs_by_guid.find(guid_ref);
  if (it == certs_by_guid.end()) {
    LOG(ERROR) << "Couldn't resolve certificate reference " << guid_ref;
    return false;
  }
  *pem_encoded = it->second;
  if (pem_encoded->empty()) {
    LOG(ERROR) << "Couldn't PEM-encode certificate with GUID " << guid_ref;
    return false;
  }
  return true;
}

// Given a GUID-> PM certificate mapping |certs_by_guid|, attempts to resolve
// the certificate referenced by the |key_guid_ref| field in |onc_object|.
// * If |onc_object| has no |key_guid_ref| field, returns true.
// * If no matching certificate is found in |certs_by_guid|, returns false.
// * If a matching certificate is found, removes the |key_guid_ref| field,
//   fills the |key_pem| field in |onc_object| and returns true.
bool ResolveSingleCertRef(const CertPEMsByGUIDMap& certs_by_guid,
                          const std::string& key_guid_ref,
                          const std::string& key_pem,
                          base::Value* onc_object) {
  std::string* guid_ref = onc_object->FindStringKey(key_guid_ref);
  if (!guid_ref)
    return true;

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, *guid_ref, &pem_encoded))
    return false;

  onc_object->RemoveKey(key_guid_ref);
  onc_object->SetKey(key_pem, base::Value(pem_encoded));
  return true;
}

// Given a GUID-> PM certificate mapping |certs_by_guid|, attempts to resolve
// the certificates referenced by the list-of-strings field |key_guid_ref_list|
// in |onc_object|.
// * If |key_guid_ref_list| does not exist in |onc_object|, returns true.
// * If any element |key_guid_ref_list| can not be found in |certs_by_guid|,
//   aborts processing and returns false. |onc_object| is unchanged in this
//   case.
// * Otherwise, sets |key_pem_list| to be a list-of-strings field in
//   |onc_object|, containing all PEM encoded resolved certificates in order and
//   returns true.
bool ResolveCertRefList(const CertPEMsByGUIDMap& certs_by_guid,
                        const std::string& key_guid_ref_list,
                        const std::string& key_pem_list,
                        base::Value::Dict& onc_object) {
  const base::Value::List* guid_ref_list =
      onc_object.FindList(key_guid_ref_list);
  if (!guid_ref_list)
    return true;

  base::Value::List pem_list;
  for (const auto& entry : *guid_ref_list) {
    std::string pem_encoded;
    if (!GUIDRefToPEMEncoding(certs_by_guid, entry.GetString(), &pem_encoded))
      return false;

    pem_list.Append(pem_encoded);
  }

  onc_object.Remove(key_guid_ref_list);
  onc_object.Set(key_pem_list, std::move(pem_list));
  return true;
}

// Same as |ResolveSingleCertRef|, but the output |key_pem_list| will be set to
// a list with exactly one value when resolution succeeds.
bool ResolveSingleCertRefToList(const CertPEMsByGUIDMap& certs_by_guid,
                                const std::string& key_guid_ref,
                                const std::string& key_pem_list,
                                base::Value::Dict& onc_object) {
  std::string* guid_ref = onc_object.FindString(key_guid_ref);
  if (!guid_ref)
    return true;

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, *guid_ref, &pem_encoded))
    return false;

  base::Value::List pem_list;
  pem_list.Append(pem_encoded);
  onc_object.Remove(key_guid_ref);
  onc_object.Set(key_pem_list, std::move(pem_list));
  return true;
}

// Resolves the reference list at |key_guid_refs| if present and otherwise the
// single reference at |key_guid_ref|. Returns whether the respective resolving
// was successful.
bool ResolveCertRefsOrRefToList(const CertPEMsByGUIDMap& certs_by_guid,
                                const std::string& key_guid_refs,
                                const std::string& key_guid_ref,
                                const std::string& key_pem_list,
                                base::Value* onc_object) {
  base::Value::Dict& onc_dict = onc_object->GetDict();
  if (onc_dict.contains(key_guid_refs)) {
    if (onc_dict.contains(key_guid_ref)) {
      LOG(ERROR) << "Found both " << key_guid_refs << " and " << key_guid_ref
                 << ". Ignoring and removing the latter.";
      onc_dict.Remove(key_guid_ref);
    }
    return ResolveCertRefList(certs_by_guid, key_guid_refs, key_pem_list,
                              onc_dict);
  }

  // Only resolve |key_guid_ref| if |key_guid_refs| isn't present.
  return ResolveSingleCertRefToList(certs_by_guid, key_guid_ref, key_pem_list,
                                    onc_dict);
}

// Resolve known server and authority certiifcate reference fields in
// |onc_object|.
bool ResolveServerCertRefsInObject(const CertPEMsByGUIDMap& certs_by_guid,
                                   const OncValueSignature& signature,
                                   base::Value* onc_object) {
  DCHECK(onc_object->is_dict());
  if (&signature == &kCertificatePatternSignature) {
    if (!ResolveCertRefList(certs_by_guid, ::onc::client_cert::kIssuerCARef,
                            ::onc::client_cert::kIssuerCAPEMs,
                            onc_object->GetDict())) {
      return false;
    }
  } else if (&signature == &kEAPSignature) {
    if (!ResolveCertRefsOrRefToList(certs_by_guid, ::onc::eap::kServerCARefs,
                                    ::onc::eap::kServerCARef,
                                    ::onc::eap::kServerCAPEMs, onc_object)) {
      return false;
    }
  } else if (&signature == &kIPsecSignature) {
    if (!ResolveCertRefsOrRefToList(certs_by_guid, ::onc::ipsec::kServerCARefs,
                                    ::onc::ipsec::kServerCARef,
                                    ::onc::ipsec::kServerCAPEMs, onc_object)) {
      return false;
    }
  } else if (&signature == &kIPsecSignature ||
             &signature == &kOpenVPNSignature) {
    if (!ResolveSingleCertRef(certs_by_guid, ::onc::openvpn::kServerCertRef,
                              ::onc::openvpn::kServerCertPEM, onc_object) ||
        !ResolveCertRefsOrRefToList(
            certs_by_guid, ::onc::openvpn::kServerCARefs,
            ::onc::openvpn::kServerCARef, ::onc::openvpn::kServerCAPEMs,
            onc_object)) {
      return false;
    }
  }

  // Recurse into nested objects.
  for (auto it : onc_object->DictItems()) {
    if (!it.second.is_dict())
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature)
      continue;

    if (!ResolveServerCertRefsInObject(
            certs_by_guid, *field_signature->value_signature, &it.second)) {
      return false;
    }
  }
  return true;
}

}  // namespace

absl::optional<base::Value::Dict> ReadDictionaryFromJson(
    const std::string& json) {
  if (json.empty()) {
    // Policy may contain empty values, just log a debug message.
    NET_LOG(DEBUG) << "Empty json string";
    return absl::nullopt;
  }
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json,
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value()) {
    NET_LOG(ERROR) << "Invalid JSON Dictionary: "
                   << parsed_json.error().message;
    return absl::nullopt;
  }
  if (!parsed_json->is_dict()) {
    NET_LOG(ERROR) << "Invalid JSON Dictionary: Expected a dictionary.";
    return absl::nullopt;
  }
  return std::move(*parsed_json).TakeDict();
}

base::Value Decrypt(const std::string& passphrase, const base::Value& root) {
  const int kKeySizeInBits = 256;
  const int kMaxIterationCount = 500000;
  std::string onc_type;
  std::string initial_vector;
  std::string salt;
  std::string cipher;
  std::string stretch_method;
  std::string hmac_method;
  std::string hmac;
  int iterations;
  std::string ciphertext;

  if (!GetString(root, ::onc::encrypted::kCiphertext, &ciphertext) ||
      !GetString(root, ::onc::encrypted::kCipher, &cipher) ||
      !GetString(root, ::onc::encrypted::kHMAC, &hmac) ||
      !GetString(root, ::onc::encrypted::kHMACMethod, &hmac_method) ||
      !GetString(root, ::onc::encrypted::kIV, &initial_vector) ||
      !GetInt(root, ::onc::encrypted::kIterations, &iterations) ||
      !GetString(root, ::onc::encrypted::kSalt, &salt) ||
      !GetString(root, ::onc::encrypted::kStretch, &stretch_method) ||
      !GetString(root, ::onc::toplevel_config::kType, &onc_type) ||
      onc_type != ::onc::toplevel_config::kEncryptedConfiguration) {
    NET_LOG(ERROR) << "Encrypted ONC malformed.";
    return base::Value();
  }

  if (hmac_method != ::onc::encrypted::kSHA1 ||
      cipher != ::onc::encrypted::kAES256 ||
      stretch_method != ::onc::encrypted::kPBKDF2) {
    NET_LOG(ERROR) << "Encrypted ONC unsupported encryption scheme.";
    return base::Value();
  }

  // Make sure iterations != 0, since that's not valid.
  if (iterations == 0) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return base::Value();
  }

  // Simply a sanity check to make sure we can't lock up the machine
  // for too long with a huge number (or a negative number).
  if (iterations < 0 || iterations > kMaxIterationCount) {
    NET_LOG(ERROR) << "Too many iterations in encrypted ONC";
    return base::Value();
  }

  if (!base::Base64Decode(salt, &salt)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return base::Value();
  }

  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, passphrase, salt, iterations,
          kKeySizeInBits));

  if (!base::Base64Decode(initial_vector, &initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return base::Value();
  }
  if (!base::Base64Decode(ciphertext, &ciphertext)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return base::Value();
  }
  if (!base::Base64Decode(hmac, &hmac)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return base::Value();
  }

  crypto::HMAC hmac_verifier(crypto::HMAC::SHA1);
  if (!hmac_verifier.Init(key.get()) ||
      !hmac_verifier.Verify(ciphertext, hmac)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return base::Value();
  }

  crypto::Encryptor decryptor;
  if (!decryptor.Init(key.get(), crypto::Encryptor::CBC, initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return base::Value();
  }

  std::string plaintext;
  if (!decryptor.Decrypt(ciphertext, &plaintext)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return base::Value();
  }

  absl::optional<base::Value::Dict> new_root =
      ReadDictionaryFromJson(plaintext);
  if (!new_root) {
    NET_LOG(ERROR) << "Property dictionary malformed.";
    return base::Value();
  }
  return base::Value(std::move(*new_root));
}

std::string GetSourceAsString(::onc::ONCSource source) {
  switch (source) {
    case ::onc::ONC_SOURCE_UNKNOWN:
      return "unknown";
    case ::onc::ONC_SOURCE_NONE:
      return "none";
    case ::onc::ONC_SOURCE_DEVICE_POLICY:
      return "device policy";
    case ::onc::ONC_SOURCE_USER_POLICY:
      return "user policy";
    case ::onc::ONC_SOURCE_USER_IMPORT:
      return "user import";
  }
  NOTREACHED();
  return "unknown";
}

void ExpandStringsInOncObject(const OncValueSignature& signature,
                              const VariableExpander& variable_expander,
                              base::Value::Dict* onc_object) {
  if (&signature == &kEAPSignature) {
    ExpandField(::onc::eap::kAnonymousIdentity, variable_expander, onc_object);
    ExpandField(::onc::eap::kIdentity, variable_expander, onc_object);
  } else if (&signature == &kL2TPSignature ||
             &signature == &kOpenVPNSignature) {
    ExpandField(::onc::vpn::kUsername, variable_expander, onc_object);
  }

  // Recurse into nested objects.
  for (auto it : *onc_object) {
    if (!it.second.is_dict())
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature)
      continue;

    ExpandStringsInOncObject(*field_signature->value_signature,
                             variable_expander, &it.second.GetDict());
  }
}

void ExpandStringsInNetworks(const VariableExpander& variable_expander,
                             base::Value::List& network_configs) {
  for (auto& network : network_configs) {
    ExpandStringsInOncObject(kNetworkConfigurationSignature, variable_expander,
                             &network.GetDict());
  }
}

void FillInHexSSIDFieldsInOncObject(const OncValueSignature& signature,
                                    base::Value::Dict& onc_object) {
  if (&signature == &kWiFiSignature)
    FillInHexSSIDField(onc_object);

  // Recurse into nested objects.
  for (auto it : onc_object) {
    if (!it.second.is_dict())
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature)
      continue;

    FillInHexSSIDFieldsInOncObject(*field_signature->value_signature,
                                   it.second.GetDict());
  }
}

void FillInHexSSIDField(base::Value::Dict& wifi_fields) {
  if (wifi_fields.Find(::onc::wifi::kHexSSID))
    return;
  std::string* ssid = wifi_fields.FindString(::onc::wifi::kSSID);
  if (!ssid)
    return;
  if (ssid->empty()) {
    NET_LOG(ERROR) << "Found empty SSID field.";
    return;
  }
  wifi_fields.Set(::onc::wifi::kHexSSID,
                  base::HexEncode(ssid->c_str(), ssid->size()));
}

void SetHiddenSSIDFieldInOncObject(const OncValueSignature& signature,
                                   base::Value::Dict& onc_object) {
  if (&signature == &kWiFiSignature)
    SetHiddenSSIDField(onc_object);

  // Recurse into nested objects.
  for (auto it : onc_object) {
    if (!it.second.is_dict())
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature)
      continue;

    SetHiddenSSIDFieldInOncObject(*field_signature->value_signature,
                                  it.second.GetDict());
  }
}

void SetHiddenSSIDField(base::Value::Dict& wifi_fields) {
  if (wifi_fields.Find(::onc::wifi::kHiddenSSID))
    return;
  wifi_fields.Set(::onc::wifi::kHiddenSSID, false);
}

base::Value MaskCredentialsInOncObject(const OncValueSignature& signature,
                                       const base::Value& onc_object,
                                       const std::string& mask) {
  return OncMaskValues::Mask(signature, onc_object, mask);
}

std::string DecodePEM(const std::string& pem_encoded) {
  // The PEM block header used for DER certificates
  const char kCertificateHeader[] = "CERTIFICATE";

  // This is an older PEM marker for DER certificates.
  const char kX509CertificateHeader[] = "X509 CERTIFICATE";

  std::vector<std::string> pem_headers;
  pem_headers.push_back(kCertificateHeader);
  pem_headers.push_back(kX509CertificateHeader);

  net::PEMTokenizer pem_tokenizer(pem_encoded, pem_headers);
  std::string decoded;
  if (pem_tokenizer.GetNext()) {
    decoded = pem_tokenizer.data();
  } else {
    // If we failed to read the data as a PEM file, then try plain base64 decode
    // in case the PEM marker strings are missing. For this to work, there has
    // to be no white space, and it has to only contain the base64-encoded data.
    if (!base::Base64Decode(pem_encoded, &decoded)) {
      LOG(ERROR) << "Unable to base64 decode X509 data: " << pem_encoded;
      return std::string();
    }
  }
  return decoded;
}

bool ParseAndValidateOncForImport(const std::string& onc_blob,
                                  ::onc::ONCSource onc_source,
                                  const std::string& passphrase,
                                  base::Value::List* network_configs,
                                  base::Value::Dict* global_network_config,
                                  base::Value::List* certificates) {
  if (network_configs)
    network_configs->clear();
  if (global_network_config)
    global_network_config->clear();
  if (certificates)
    certificates->clear();
  if (onc_blob.empty())
    return true;

  absl::optional<base::Value::Dict> toplevel_onc_dict =
      ReadDictionaryFromJson(onc_blob);
  if (!toplevel_onc_dict) {
    NET_LOG(ERROR) << "Not a valid ONC JSON dictionary: "
                   << GetSourceAsString(onc_source);
    return false;
  }
  base::Value toplevel_onc(std::move(*toplevel_onc_dict));

  // Check and see if this is an encrypted ONC file. If so, decrypt it.
  std::string onc_type;
  if (GetString(toplevel_onc, ::onc::toplevel_config::kType, &onc_type) &&
      onc_type == ::onc::toplevel_config::kEncryptedConfiguration) {
    toplevel_onc = Decrypt(passphrase, toplevel_onc);
    if (toplevel_onc.is_none()) {
      NET_LOG(ERROR) << "Unable to decrypt ONC from "
                     << GetSourceAsString(onc_source);
      return false;
    }
  }

  bool from_policy = (onc_source == ::onc::ONC_SOURCE_USER_POLICY ||
                      onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY);

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  Validator validator(false,  // Ignore unknown fields.
                      false,  // Ignore invalid recommended field names.
                      true,   // Fail on missing fields.
                      from_policy,
                      true);  // Log warnings.
  validator.SetOncSource(onc_source);

  Validator::Result validation_result;
  base::Value validated_toplevel_onc = validator.ValidateAndRepairObject(
      &kToplevelConfigurationSignature, toplevel_onc, &validation_result);

  if (from_policy) {
    UMA_HISTOGRAM_BOOLEAN("Enterprise.ONC.PolicyValidation",
                          validation_result == Validator::VALID);
  }

  bool success = true;
  if (validation_result == Validator::VALID_WITH_WARNINGS) {
    NET_LOG(DEBUG) << "ONC validation produced warnings: "
                   << GetSourceAsString(onc_source);
    success = false;
  } else if (validation_result == Validator::INVALID ||
             validated_toplevel_onc.is_none()) {
    NET_LOG(ERROR) << "ONC is invalid and couldn't be repaired: "
                   << GetSourceAsString(onc_source);
    return false;
  }

  base::Value::Dict& validated_toplevel_onc_dict =
      validated_toplevel_onc.GetDict();
  if (certificates) {
    base::Value::List* validated_certs = validated_toplevel_onc_dict.FindList(
        ::onc::toplevel_config::kCertificates);
    if (validated_certs)
      *certificates = std::move(*validated_certs);
  }

  // Note that this processing is performed even if |network_configs| is
  // nullptr, because ResolveServerCertRefsInNetworks could affect the return
  // value of the function (which is supposed to aggregate validation issues in
  // all segments of the ONC blob).
  base::Value::List* validated_networks_list =
      validated_toplevel_onc_dict.FindList(
          ::onc::toplevel_config::kNetworkConfigurations);
  if (validated_networks_list) {
    FillInHexSSIDFieldsInNetworks(*validated_networks_list);
    // Set HiddenSSID to default value to solve the issue crbug.com/1171837
    SetHiddenSSIDFieldsInNetworks(*validated_networks_list);

    CertPEMsByGUIDMap server_and_ca_certs =
        GetServerAndCACertsByGUID(*certificates);

    if (!ResolveServerCertRefsInNetworks(server_and_ca_certs,
                                         *validated_networks_list)) {
      NET_LOG(ERROR) << "Some certificate references in the ONC policy could "
                        "not be resolved: "
                     << GetSourceAsString(onc_source);
      success = false;
    }

    if (network_configs)
      *network_configs = std::move(*validated_networks_list);
  }

  if (global_network_config) {
    base::Value::Dict* validated_global_config =
        validated_toplevel_onc_dict.FindDict(
            ::onc::toplevel_config::kGlobalNetworkConfiguration);
    if (validated_global_config) {
      *global_network_config = std::move(*validated_global_config);
    }
  }

  return success;
}

bool ResolveServerCertRefsInNetworks(const CertPEMsByGUIDMap& certs_by_guid,
                                     base::Value::List& network_configs) {
  bool success = true;
  base::Value::List filtered_configs;
  for (base::Value& network : network_configs) {
    DCHECK(network.is_dict());
    if (!ResolveServerCertRefsInNetwork(certs_by_guid, &network)) {
      std::string* guid =
          network.GetDict().FindString(::onc::network_config::kGUID);
      // This might happen even with correct validation, if the referenced
      // certificate couldn't be imported.
      LOG(ERROR) << "Couldn't resolve some certificate reference of network "
                 << (guid ? *guid : "(unable to find GUID)");
      success = false;
      continue;
    }

    filtered_configs.Append(std::move(network));
  }
  network_configs = std::move(filtered_configs);
  return success;
}

bool ResolveServerCertRefsInNetwork(const CertPEMsByGUIDMap& certs_by_guid,
                                    base::Value* network_config) {
  return ResolveServerCertRefsInObject(
      certs_by_guid, kNetworkConfigurationSignature, network_config);
}

}  // namespace onc
}  // namespace chromeos
