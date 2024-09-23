// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/onc_utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
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
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace chromeos::onc {
namespace {

using IdToAPNMap = std::map<std::string, const base::Value::Dict*>;

// Error messages that can be reported when decrypting encrypted ONC.
constexpr char kUnableToDecrypt[] = "Unable to decrypt encrypted ONC";
constexpr char kUnableToDecode[] = "Unable to decode encrypted ONC";

bool GetString(const base::Value::Dict& dict,
               const char* key,
               std::string* result) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return false;
  }
  *result = *value;
  return true;
}

bool GetInt(const base::Value::Dict& dict, const char* key, int* result) {
  const std::optional<int> value = dict.FindInt(key);
  if (!value) {
    return false;
  }
  *result = value.value();
  return true;
}

// Runs |variable_expander.ExpandString| on the field |fieldname| in
// |onc_object|.
void ExpandField(const std::string& fieldname,
                 const VariableExpander& variable_expander,
                 base::Value::Dict* onc_object) {
  std::string* field_value = onc_object->FindString(fieldname);
  if (!field_value) {
    return;
  }
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
  static base::Value::Dict Mask(const OncValueSignature& signature,
                                const base::Value::Dict& onc_object,
                                const std::string& mask) {
    OncMaskValues masker(mask);
    bool error = false;
    base::Value::Dict result = masker.MapObject(signature, onc_object, &error);
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
  for (const auto& cert_value : certificates) {
    const base::Value::Dict& cert = cert_value.GetDict();

    const std::string* guid = cert.FindString(::onc::certificate::kGUID);
    if (!guid || guid->empty()) {
      NET_LOG(ERROR) << "Certificate with missing or empty GUID.";
      continue;
    }
    const std::string* cert_type = cert.FindString(::onc::certificate::kType);
    DCHECK(cert_type);
    if (*cert_type != ::onc::certificate::kServer &&
        *cert_type != ::onc::certificate::kAuthority) {
      continue;
    }
    const std::string* x509_data = cert.FindString(::onc::certificate::kX509);
    std::string der;
    if (x509_data) {
      der = DecodePEM(*x509_data);
    }
    std::string pem;
    if (der.empty() || !net::X509Certificate::GetPEMEncodedFromDER(der, &pem)) {
      NET_LOG(ERROR) << "Certificate not PEM encoded, GUID: " << *guid;
      continue;
    }
    certs_by_guid[*guid] = pem;
  }

  return certs_by_guid;
}

// Set APN dictionary and associated recommended values to solve the issue
// of setting the APN for managed eSIM profiles (see http://b/295226668) in
// old APN UI.
void SetAPNDictAndRecommendedIfNone(base::Value::Dict& cellular_fields) {
  if (cellular_fields.Find(::onc::cellular::kAPN)) {
    return;
  }

  auto apn_recommended_list = base::Value::List()
                                  .Append(::onc::cellular_apn::kAccessPointName)
                                  .Append(::onc::cellular_apn::kAttach)
                                  .Append(::onc::cellular_apn::kAuthentication)
                                  .Append(::onc::cellular_apn::kUsername)
                                  .Append(::onc::cellular_apn::kPassword);

  base::Value* apn_dict = cellular_fields.Set(
      ::onc::cellular::kAPN, base::Value(base::Value::Type::DICT));
  apn_dict->GetDict().Set(::onc::kRecommended, std::move(apn_recommended_list));
}

// Modify recommended list to include custom APN list field to solve the issue
// of setting the APN for managed eSIM profiles (see http://b/295226668) in
// revamp APN UI.
void AddCustomAPNListToRecommended(base::Value::Dict& cellular_fields) {
  auto* recommended = cellular_fields.Find(::onc::kRecommended);
  if (!recommended) {
    recommended = cellular_fields.Set(::onc::kRecommended,
                                      base::Value(base::Value::Type::LIST));
  }
  for (const auto& field : recommended->GetList()) {
    if (field == ::onc::cellular::kCustomAPNList) {
      return;
    }
  }
  recommended->GetList().Append(::onc::cellular::kCustomAPNList);
}

void FillInCellularDefaultsInOncObject(const OncValueSignature& signature,
                                       base::Value::Dict& onc_object,
                                       bool allow_apn_modification) {
  if (&signature == &kCellularSignature) {
    if (allow_apn_modification) {
      AddCustomAPNListToRecommended(onc_object);
    } else {
      onc_object.Set(::onc::cellular::kCustomAPNList, base::Value::List());
    }
    SetAPNDictAndRecommendedIfNone(onc_object);

    return;
  }

  // The function takes any ONC object and recursively searches until it finds a
  // Cellular dictionary to set the default values.
  for (auto it : onc_object) {
    if (!it.second.is_dict()) {
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature) {
      continue;
    }

    FillInCellularDefaultsInOncObject(*field_signature->value_signature,
                                      it.second.GetDict(),
                                      allow_apn_modification);
  }
}

// Creates an APN dict with nested recommended field in cellular entries lacking
// an APN dict in |network_configs| list. If |allow_apn_modification| is true,
// "CustomAPNList" is added as a recommended field to the cellular config,
// otherwise, the CustomAPNList field is set to an empty list.
void FillInCellularDefaultsInNetworks(base::Value::List& network_configs,
                                      bool allow_apn_modification) {
  for (auto& network : network_configs) {
    FillInCellularDefaultsInOncObject(kNetworkConfigurationSignature,
                                      network.GetDict(),
                                      allow_apn_modification);
  }
}

// Creates a map from APN IDs to their corresponding configuration dictionaries.
IdToAPNMap BuildIdToAPNMap(const base::Value::List* apn_list) {
  IdToAPNMap apn_map;

  if (!apn_list) {
    return apn_map;
  }

  for (const base::Value& apn_value : *apn_list) {
    const base::Value::Dict& apn_dict = apn_value.GetDict();
    const std::string* apn_id = apn_dict.FindString(::onc::cellular_apn::kId);

    if (apn_id) {
      apn_map.emplace(*apn_id, &apn_dict);
    }
  }

  return apn_map;
}

// Extracts a list of APN dictionaries based on a provided list of APN IDs, such
// that |apn_id_list| is a list of string IDs representing the APNs to extract,
// and |apn_map| is a map of all available APN dictionaries with key being APN
// ID. Returns a base::List if IDs are successfully extracted and the source is
// set successfully, and an std::nullopt otherwise.
std::optional<base::Value::List> ExtractAPNsByIdsAndSetAdminSource(
    const base::Value::List* apn_id_list,
    const IdToAPNMap& apn_map) {
  base::Value::List result = base::Value::List();

  for (const base::Value& apn_id_value : *apn_id_list) {
    const std::string apn_id = apn_id_value.GetString();

    // Find the APN in the map
    auto it = apn_map.find(apn_id);
    if (it == apn_map.end()) {
      NET_LOG(ERROR)
          << "Failed to find an admin provided APN associated to an ID of "
          << apn_id;
      return std::nullopt;
    }
    base::Value::Dict apn_cpy = it->second->Clone();
    apn_cpy.Set(::onc::cellular_apn::kSource,
                ::onc::cellular_apn::kSourceAdmin);

    result.Append(std::move(apn_cpy));
  }

  return result;
}

// Updates a cellular network configuration with custom APN information from
// admin-assigned APNs. Looks for a list of admin-assigned APN IDs in
// |cellular_fields|. If found, it extracts the corresponding APN dictionaries
// from |admin_apn_by_id| and sets the CustomAPNList field in |cellular_fields|.
// Note that if |admin_apn_by_id| is null, no changes are made to
// |cellular_fields|. Also note that each extracted APN will have a
// |::onc::cellular_apn::kSource| of
// |::onc::cellular_apn::kSourceAdmin|. Returns true if |cellular_fields| are
// successfully updated.
bool UpdateCellularFieldsWithAdminApns(base::Value::Dict& cellular_fields,
                                       const IdToAPNMap& admin_apn_by_id) {
  const base::Value::List* admin_apn_id_list =
      cellular_fields.FindList(::onc::cellular::kAdminAssignedAPNIds);
  if (!admin_apn_id_list) {
    return true;
  }

  if (admin_apn_id_list->empty()) {
    cellular_fields.Set(::onc::cellular::kCustomAPNList, base::Value::List());
    return true;
  }

  std::optional<base::Value::List> admin_apns =
      ExtractAPNsByIdsAndSetAdminSource(admin_apn_id_list, admin_apn_by_id);
  if (!admin_apns.has_value()) {
    NET_LOG(ERROR) << "Failed to extract admin APNs";
    return false;
  }

  cellular_fields.Set(::onc::cellular::kCustomAPNList, std::move(*admin_apns));
  return true;
}

bool ConstructAndSetPSIMAdminAPNs(base::Value::Dict& global_network_config,
                                  const IdToAPNMap& admin_apn_by_id) {
  if (admin_apn_by_id.empty()) {
    return true;
  }
  const base::Value::List* psim_admin_apn_id_list =
      global_network_config.FindList(
          ::onc::global_network_config::kPSIMAdminAssignedAPNIds);
  if (!psim_admin_apn_id_list) {
    return true;
  }

  std::optional<base::Value::List> psim_admin_apns =
      ExtractAPNsByIdsAndSetAdminSource(psim_admin_apn_id_list,
                                        admin_apn_by_id);
  if (!psim_admin_apns.has_value()) {
    NET_LOG(ERROR) << "Failed to extract pSIM admin APNs";
    return false;
  }

  global_network_config.Set(
      ::onc::global_network_config::kPSIMAdminAssignedAPNs,
      std::move(*psim_admin_apns));
  return true;
}

// Recursively traverses the |onc_object|, searching for
// cellular dictionaries. If found, it updates the 'CustomAPNList' field within
// the Cellular dictionary using |admin_apn_by_id| if applicable.
//
// The recursion is guided by the |signature|, which defines the structure of
// the ONC object and helps the function determine which fields to traverse.
// Returns true if admin APNs are successfully applied.
bool ApplyAdminApnsToOncObject(const OncValueSignature& signature,
                               base::Value::Dict& onc_object,
                               const IdToAPNMap& admin_apn_by_id) {
  if (&signature == &kCellularSignature) {
    return UpdateCellularFieldsWithAdminApns(onc_object, admin_apn_by_id);
  }

  // The function takes any ONC object and recursively searches until it finds a
  // Cellular dictionary to set the Custom APN List.
  for (auto it : onc_object) {
    if (!it.second.is_dict()) {
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature) {
      continue;
    }

    if (!ApplyAdminApnsToOncObject(*field_signature->value_signature,
                                   it.second.GetDict(), admin_apn_by_id)) {
      return false;
    }
  }
  return true;
}

// Processes a list of network configurations, identifying those of cellular
// type. For each cellular configuration, it associates and embeds the
// corresponding admin defined APN details found in |admin_apn_by_id|. This is
// achieved by recursively traversing the cellular configuration's structure and
// updating the APN information where applicable.
//
// The function relies on a top-level ONC configuration that contains a list of
// APNs provided by administrators. Cellular networks within the configuration
// may reference these APNs using unique identifiers (IDs).
//
// Ultimately, this function ensures that the cellular networks in the provided
// |network_configs| list are populated with the complete APN configurations
// that they are associated with. Otherwise, it returns false.
bool ConfigureAdminApnsInCellularNetworks(base::Value::List& network_configs,
                                          const IdToAPNMap& admin_apn_by_id) {
  if (admin_apn_by_id.empty()) {
    return true;
  }
  for (auto& network : network_configs) {
    if (!ApplyAdminApnsToOncObject(kNetworkConfigurationSignature,
                                   network.GetDict(), admin_apn_by_id)) {
      return false;
    }
  }
  return true;
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
                          base::Value::Dict& onc_object) {
  std::string* guid_ref = onc_object.FindString(key_guid_ref);
  if (!guid_ref) {
    return true;
  }

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, *guid_ref, &pem_encoded)) {
    return false;
  }

  onc_object.Remove(key_guid_ref);
  onc_object.Set(key_pem, pem_encoded);
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
  if (!guid_ref_list) {
    return true;
  }

  base::Value::List pem_list;
  for (const auto& entry : *guid_ref_list) {
    std::string pem_encoded;
    if (!GUIDRefToPEMEncoding(certs_by_guid, entry.GetString(), &pem_encoded)) {
      return false;
    }

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
  if (!guid_ref) {
    return true;
  }

  std::string pem_encoded;
  if (!GUIDRefToPEMEncoding(certs_by_guid, *guid_ref, &pem_encoded)) {
    return false;
  }

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
                                base::Value::Dict& onc_dict) {
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

// Resolve known server and authority certificate reference fields in
// |onc_object|.
bool ResolveServerCertRefsInObject(const CertPEMsByGUIDMap& certs_by_guid,
                                   const OncValueSignature& signature,
                                   base::Value::Dict& onc_object) {
  if (&signature == &kCertificatePatternSignature) {
    if (!ResolveCertRefList(certs_by_guid, ::onc::client_cert::kIssuerCARef,
                            ::onc::client_cert::kIssuerCAPEMs, onc_object)) {
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
  for (auto it : onc_object) {
    if (!it.second.is_dict()) {
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature) {
      continue;
    }

    if (!ResolveServerCertRefsInObject(certs_by_guid,
                                       *field_signature->value_signature,
                                       it.second.GetDict())) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<base::Value::Dict> ReadDictionaryFromJson(
    const std::string& json) {
  if (json.empty()) {
    // Policy may contain empty values, just log a debug message.
    NET_LOG(DEBUG) << "Empty json string";
    return std::nullopt;
  }
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json,
      base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.has_value()) {
    NET_LOG(ERROR) << "Invalid JSON Dictionary: "
                   << parsed_json.error().message;
    return std::nullopt;
  }
  if (!parsed_json->is_dict()) {
    NET_LOG(ERROR) << "Invalid JSON Dictionary: Expected a dictionary.";
    return std::nullopt;
  }
  return std::move(*parsed_json).TakeDict();
}

std::optional<base::Value::Dict> Decrypt(const std::string& passphrase,
                                         const base::Value::Dict& root) {
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
    return std::nullopt;
  }

  if (hmac_method != ::onc::encrypted::kSHA1 ||
      cipher != ::onc::encrypted::kAES256 ||
      stretch_method != ::onc::encrypted::kPBKDF2) {
    NET_LOG(ERROR) << "Encrypted ONC unsupported encryption scheme.";
    return std::nullopt;
  }

  // Make sure iterations != 0, since that's not valid.
  if (iterations == 0) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return std::nullopt;
  }

  // Simply a sanity check to make sure we can't lock up the machine
  // for too long with a huge number (or a negative number).
  if (iterations < 0 || iterations > kMaxIterationCount) {
    NET_LOG(ERROR) << "Too many iterations in encrypted ONC";
    return std::nullopt;
  }

  if (!base::Base64Decode(salt, &salt)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return std::nullopt;
  }

  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, passphrase, salt, iterations,
          kKeySizeInBits));

  if (!base::Base64Decode(initial_vector, &initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return std::nullopt;
  }
  if (!base::Base64Decode(ciphertext, &ciphertext)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return std::nullopt;
  }
  if (!base::Base64Decode(hmac, &hmac)) {
    NET_LOG(ERROR) << kUnableToDecode;
    return std::nullopt;
  }

  crypto::HMAC hmac_verifier(crypto::HMAC::SHA1);
  if (!hmac_verifier.Init(key.get()) ||
      !hmac_verifier.Verify(ciphertext, hmac)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return std::nullopt;
  }

  crypto::Encryptor decryptor;
  if (!decryptor.Init(key.get(), crypto::Encryptor::CBC, initial_vector)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return std::nullopt;
  }

  std::string plaintext;
  if (!decryptor.Decrypt(ciphertext, &plaintext)) {
    NET_LOG(ERROR) << kUnableToDecrypt;
    return std::nullopt;
  }

  std::optional<base::Value::Dict> new_root = ReadDictionaryFromJson(plaintext);
  if (!new_root) {
    NET_LOG(ERROR) << "Property dictionary malformed.";
  }
  return new_root;
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
  NOTREACHED_IN_MIGRATION();
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

void FillInCellularCustomAPNListField(
    base::Value::Dict& cellular_fields,
    const base::Value::List* custom_apn_list) {
  if (cellular_fields.Find(::onc::cellular::kCustomAPNList)) {
    NET_LOG(DEBUG) << "kCustomAPNList found, skipping";
    return;
  }

  NET_LOG(DEBUG) << "Filling in kCustomAPNList with "
                 << custom_apn_list->DebugString();
  cellular_fields.Set(::onc::cellular::kCustomAPNList,
                      custom_apn_list->Clone());
}

void FillInCellularCustomAPNListFieldsInOncObject(
    const OncValueSignature& signature,
    base::Value::Dict& onc_object,
    const base::Value::List* custom_apn_list) {
  if (&signature == &kCellularSignature) {
    FillInCellularCustomAPNListField(onc_object, custom_apn_list);
  }

  for (auto it : onc_object) {
    if (!it.second.is_dict()) {
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature) {
      continue;
    }

    FillInCellularCustomAPNListFieldsInOncObject(
        *field_signature->value_signature, it.second.GetDict(),
        custom_apn_list);
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
  if (wifi_fields.Find(::onc::wifi::kHexSSID)) {
    return;
  }
  std::string* ssid = wifi_fields.FindString(::onc::wifi::kSSID);
  if (!ssid) {
    return;
  }
  if (ssid->empty()) {
    NET_LOG(ERROR) << "Found empty SSID field.";
    return;
  }
  wifi_fields.Set(::onc::wifi::kHexSSID, base::HexEncode(*ssid));
}

void SetHiddenSSIDFieldInOncObject(const OncValueSignature& signature,
                                   base::Value::Dict& onc_object) {
  if (&signature == &kWiFiSignature) {
    SetHiddenSSIDField(onc_object);
  }

  // Recurse into nested objects.
  for (auto it : onc_object) {
    if (!it.second.is_dict()) {
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.first);
    if (!field_signature) {
      continue;
    }

    SetHiddenSSIDFieldInOncObject(*field_signature->value_signature,
                                  it.second.GetDict());
  }
}

void SetHiddenSSIDField(base::Value::Dict& wifi_fields) {
  if (wifi_fields.Find(::onc::wifi::kHiddenSSID)) {
    return;
  }
  wifi_fields.Set(::onc::wifi::kHiddenSSID, false);
}

base::Value::Dict MaskCredentialsInOncObject(
    const OncValueSignature& signature,
    const base::Value::Dict& onc_object,
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

  bssl::PEMTokenizer pem_tokenizer(pem_encoded, pem_headers);
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
  if (network_configs) {
    network_configs->clear();
  }
  if (global_network_config) {
    global_network_config->clear();
  }
  if (certificates) {
    certificates->clear();
  }
  if (onc_blob.empty()) {
    return true;
  }

  std::optional<base::Value::Dict> toplevel_onc =
      ReadDictionaryFromJson(onc_blob);
  if (!toplevel_onc) {
    NET_LOG(ERROR) << "Not a valid ONC JSON dictionary: "
                   << GetSourceAsString(onc_source);
    return false;
  }

  // Check and see if this is an encrypted ONC file. If so, decrypt it.
  std::string onc_type;
  if (GetString(toplevel_onc.value(), ::onc::toplevel_config::kType,
                &onc_type) &&
      onc_type == ::onc::toplevel_config::kEncryptedConfiguration) {
    toplevel_onc = Decrypt(passphrase, toplevel_onc.value());
    if (!toplevel_onc.has_value()) {
      NET_LOG(ERROR) << "Unable to decrypt ONC from "
                     << GetSourceAsString(onc_source);
      return false;
    }
  }

  bool from_policy = (onc_source == ::onc::ONC_SOURCE_USER_POLICY ||
                      onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY);

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  Validator validator(/*error_on_unknown_field=*/false,
                      /*error_on_wrong_recommended=*/false,
                      /*error_on_missing_field=*/true,
                      /*managed_onc=*/from_policy,
                      /*log_warnings=*/true);
  validator.SetOncSource(onc_source);

  Validator::Result validation_result;
  std::optional<base::Value::Dict> validated_toplevel_onc =
      validator.ValidateAndRepairObject(&kToplevelConfigurationSignature,
                                        toplevel_onc.value(),
                                        &validation_result);

  bool success = true;
  if (validation_result == Validator::VALID_WITH_WARNINGS) {
    NET_LOG(DEBUG) << "ONC validation produced warnings: "
                   << GetSourceAsString(onc_source);
    success = false;
  } else if (validation_result == Validator::INVALID ||
             !validated_toplevel_onc.has_value()) {
    NET_LOG(ERROR) << "ONC is invalid and couldn't be repaired: "
                   << GetSourceAsString(onc_source);
    return false;
  }

  if (certificates) {
    base::Value::List* validated_certs =
        validated_toplevel_onc->FindList(::onc::toplevel_config::kCertificates);
    if (validated_certs)
      *certificates = std::move(*validated_certs);
  }

  // Note that this processing is performed even if |network_configs| is
  // nullptr, because ResolveServerCertRefsInNetworks could affect the return
  // value of the function (which is supposed to aggregate validation issues in
  // all segments of the ONC blob).
  base::Value::List* validated_networks_list = validated_toplevel_onc->FindList(
      ::onc::toplevel_config::kNetworkConfigurations);

  base::Value::Dict* validated_global_config = validated_toplevel_onc->FindDict(
      ::onc::toplevel_config::kGlobalNetworkConfiguration);

  const IdToAPNMap id_to_apn_map = BuildIdToAPNMap(
      validated_toplevel_onc->FindList(::onc::toplevel_config::kAdminAPNList));

  if (validated_networks_list) {
    FillInHexSSIDFieldsInNetworks(*validated_networks_list);

    bool allow_apn_modification = true;
    if (validated_global_config) {
      allow_apn_modification =
          (validated_global_config->FindBool(
               ::onc::global_network_config::kAllowAPNModification))
              .value_or(allow_apn_modification);
    }

    FillInCellularDefaultsInNetworks(*validated_networks_list,
                                     allow_apn_modification);

    // Sets the CustomAPNList for cellular networks if an AdminAPNList and
    // AdminAssignedAPNIds have been specified for a cellular network.
    if (!ConfigureAdminApnsInCellularNetworks(*validated_networks_list,
                                              id_to_apn_map)) {
      success = false;
    }

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

    if (network_configs) {
      *network_configs = std::move(*validated_networks_list);
    }
  }

  if (global_network_config) {
    if (validated_global_config) {
      // Constructs and sets the PSIMAdminAssignedAPNs global network
      // configuration field if an AdminAPNList and PSIMAdminAssignedAPNIds have
      // been specified.
      if (!ConstructAndSetPSIMAdminAPNs(*validated_global_config,
                                        id_to_apn_map)) {
        success = false;
      }
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
    if (!ResolveServerCertRefsInNetwork(certs_by_guid, network.GetDict())) {
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
                                    base::Value::Dict& network_config) {
  return ResolveServerCertRefsInObject(
      certs_by_guid, kNetworkConfigurationSignature, network_config);
}

}  // namespace chromeos::onc
