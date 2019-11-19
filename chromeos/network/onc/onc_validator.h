// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
#define CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/onc/onc_mapper.h"
#include "components/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {
namespace onc {

struct OncValueSignature;

// The ONC Validator searches for the following invalid cases:
// - a value is found that has the wrong type or is not expected according to
//   the ONC spec (always an error)
//
// - a field name is found that is not part of the signature
//   (controlled by flag |error_on_unknown_field|)
//
// - a kRecommended array contains a field name that is not part of the
//   enclosing object's signature or if that field is dictionary typed
//   (controlled by flag |error_on_wrong_recommended|)
//
// - |managed_onc| is false and a field with name kRecommended is found
//   (always ignored)
//
// - a required field is missing. Controlled by flag |error_on_missing_field|.
//   If true this is an error. If false, a message is logged but no error or
//   warning is flagged.
//
// If one of these invalid cases occurs and, in case of a controlling flag, that
// flag is true, then it is an error. The function ValidateAndRepairObject sets
// |result| to INVALID and returns NULL.
//
// Otherwise, a DeepCopy of the validated object is created, which contains
// all but the invalid fields and values.
//
// If one of the invalid cases occurs and the controlling flag is false, then
// it is a warning. The function ValidateAndRepairObject sets |result| to
// VALID_WITH_WARNINGS and returns the repaired copy.
//
// If no error occurred, |result| is set to VALID and an exact DeepCopy is
// returned.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) Validator : public Mapper {
 public:
  enum Result {
    VALID,
    VALID_WITH_WARNINGS,
    INVALID
  };

  struct ValidationIssue {
    // If true, the ONC value does not adhere to the specification and may be
    // rejected.
    bool is_error;

    // Detailed description containing the error/warning type and specific
    // location (path).
    std::string message;
  };

  // See the class comment.
  Validator(bool error_on_unknown_field,
            bool error_on_wrong_recommended,
            bool error_on_missing_field,
            bool managed_onc,
            bool log_warnings);

  ~Validator() override;

  // Sets the ONC source to |source|. If not set, defaults to ONC_SOURCE_NONE.
  // If the source is set to ONC_SOURCE_DEVICE_POLICY, validation additionally
  // checks:
  // - only the network types Wifi and Ethernet are allowed
  // - client certificate patterns are disallowed
  void SetOncSource(::onc::ONCSource source) {
    onc_source_ = source;
  }

  // Validate the given |onc_object| dictionary according to |object_signature|.
  // The |object_signature| has to be a pointer to one of the signatures in
  // |onc_signature.h|. If an error is found, the function returns NULL and sets
  // |result| to INVALID. If possible (no error encountered) a DeepCopy is
  // created that contains all but the invalid fields and values and returns
  // this "repaired" object. That means, if not handled as an error, then the
  // following are dropped from the copy:
  // - unknown fields
  // - invalid field names in kRecommended arrays
  // - kRecommended fields in an unmanaged ONC
  // If any of these cases occurred, sets |result| to VALID_WITH_WARNINGS and
  // otherwise to VALID.
  // For details, see the class comment.
  std::unique_ptr<base::DictionaryValue> ValidateAndRepairObject(
      const OncValueSignature* object_signature,
      const base::Value& onc_object,
      Result* result);

  // Returns the list of validation results that occured within validation
  // initiated by the last call to |ValidateAndRepairObject|.
  const std::vector<ValidationIssue>& validation_issues() const {
    return validation_issues_;
  }

 private:
  // Overridden from Mapper:
  // Compare |onc_value|s type with |onc_type| and validate/repair according to
  // |signature|. On error returns NULL.
  std::unique_ptr<base::Value> MapValue(const OncValueSignature& signature,
                                        const base::Value& onc_value,
                                        bool* error) override;

  // Dispatch to the right validation function according to
  // |signature|. Iterates over all fields and recursively validates/repairs
  // these. All valid fields are added to the result dictionary. Returns the
  // repaired dictionary. Only on error returns NULL.
  std::unique_ptr<base::DictionaryValue> MapObject(
      const OncValueSignature& signature,
      const base::DictionaryValue& onc_object,
      bool* error) override;

  // Pushes/pops the |field_name| to |path_|, otherwise like |Mapper::MapField|.
  std::unique_ptr<base::Value> MapField(
      const std::string& field_name,
      const OncValueSignature& object_signature,
      const base::Value& onc_value,
      bool* found_unknown_field,
      bool* error) override;

  // Ignores nested errors in NetworkConfigurations and Certificates, otherwise
  // like |Mapper::MapArray|.
  std::unique_ptr<base::ListValue> MapArray(
      const OncValueSignature& array_signature,
      const base::ListValue& onc_array,
      bool* nested_error) override;

  // Pushes/pops the index to |path_|, otherwise like |Mapper::MapEntry|.
  std::unique_ptr<base::Value> MapEntry(int index,
                                        const OncValueSignature& signature,
                                        const base::Value& onc_value,
                                        bool* error) override;

  // This is the default validation of objects/dictionaries. Validates
  // |onc_object| according to |object_signature|. |result| must point to a
  // dictionary into which the repaired fields are written.
  bool ValidateObjectDefault(const OncValueSignature& object_signature,
                             const base::DictionaryValue& onc_object,
                             base::DictionaryValue* result);

  // Validates/repairs the kRecommended array in |result| according to
  // |object_signature| of the enclosing object.
  bool ValidateRecommendedField(const OncValueSignature& object_signature,
                                base::DictionaryValue* result);

  // Validates the ClientCert* fields in a VPN or EAP object. Only if
  // |allow_cert_type_none| is true, the value "None" is allowed as
  // ClientCertType.
  bool ValidateClientCertFields(bool allow_cert_type_none,
                                base::DictionaryValue* result);

  bool ValidateToplevelConfiguration(base::DictionaryValue* result);
  bool ValidateNetworkConfiguration(base::DictionaryValue* result);
  bool ValidateEthernet(base::DictionaryValue* result);
  bool ValidateIPConfig(base::DictionaryValue* result,
                        bool require_fields = true);
  bool ValidateNameServersConfig(base::DictionaryValue* result);
  bool ValidateWiFi(base::DictionaryValue* result);
  bool ValidateVPN(base::DictionaryValue* result);
  bool ValidateIPsec(base::DictionaryValue* result);
  bool ValidateOpenVPN(base::DictionaryValue* result);
  bool ValidateThirdPartyVPN(base::DictionaryValue* result);
  bool ValidateARCVPN(base::DictionaryValue* result);
  bool ValidateVerifyX509(base::DictionaryValue* result);
  bool ValidateCertificatePattern(base::DictionaryValue* result);
  bool ValidateGlobalNetworkConfiguration(base::DictionaryValue* result);
  bool ValidateProxySettings(base::DictionaryValue* result);
  bool ValidateProxyLocation(base::DictionaryValue* result);
  bool ValidateEAP(base::DictionaryValue* result);
  bool ValidateCertificate(base::DictionaryValue* result);
  bool ValidateScope(base::DictionaryValue* result);
  bool ValidateTether(base::DictionaryValue* result);

  bool IsValidValue(const std::string& field_value,
                    const std::vector<const char*>& valid_values);

  bool IsInDevicePolicy(base::DictionaryValue* result,
                        const std::string& field_name);

  bool FieldExistsAndHasNoValidValue(
      const base::DictionaryValue& object,
      const std::string& field_name,
      const std::vector<const char*>& valid_values);

  bool FieldExistsAndIsNotInRange(const base::DictionaryValue& object,
                                  const std::string &field_name,
                                  int lower_bound,
                                  int upper_bound);

  bool FieldExistsAndIsEmpty(const base::DictionaryValue& object,
                             const std::string& field_name);

  // Validates 'StaticIPConfig' field of the given network configuration. This
  // method takes 'NetworkConfiguration' dict instead of 'StaticIPConfig' dict
  // because it needs other 'NetworkConfiguration' fields (e.g.
  // 'IPAddressConfigType' and 'NameServersConfigType') to check correctness of
  // the 'StaticIPConfig' field.
  bool NetworkHasCorrectStaticIPConfig(base::DictionaryValue* network);

  // Validates that the given field either exists or is recommended.
  bool FieldShouldExistOrBeRecommended(const base::DictionaryValue& object,
                                       const std::string& field_name);

  bool OnlyOneFieldSet(const base::DictionaryValue& object,
                       const std::string& field_name1,
                       const std::string& field_name2);

  bool ListFieldContainsValidValues(
      const base::DictionaryValue& object,
      const std::string& field_name,
      const std::vector<const char*>& valid_values);

  bool ValidateSSIDAndHexSSID(base::DictionaryValue* object);

  // Returns true if |key| is a key of |dict|. Otherwise, returns false and,
  // depending on |error_on_missing_field_| raises an error or a warning.
  bool RequireField(const base::DictionaryValue& dict, const std::string& key);

  // Returns true if the GUID is unique or if the GUID is not a string
  // and false otherwise. The function also adds the GUID to a set in
  // order to identify duplicates.
  bool CheckGuidIsUniqueAndAddToSet(const base::DictionaryValue& dict,
                                    const std::string& kGUID,
                                    std::set<std::string> *guids);

  // Prohibit global network configuration in user ONC imports.
  bool IsGlobalNetworkConfigInUserImport(
      const base::DictionaryValue& onc_object);

  void AddValidationIssue(bool is_error, const std::string& debug_info);

  const bool error_on_unknown_field_;
  const bool error_on_wrong_recommended_;
  const bool error_on_missing_field_;
  const bool managed_onc_;
  const bool log_warnings_;

  ::onc::ONCSource onc_source_;

  // The path of field names and indices to the current value. Indices
  // are stored as strings in decimal notation.
  std::vector<std::string> path_;

  // Accumulates all network GUIDs during validation. Used to identify
  // duplicate GUIDs.
  std::set<std::string> network_guids_;

  // Accumulates all certificate GUIDs during validation. Used to identify
  // duplicate GUIDs.
  std::set<std::string> certificate_guids_;

  // List of all validation issues that occured within validation initiated by
  // function ValidateAndRepairObject.
  std::vector<ValidationIssue> validation_issues_;

  DISALLOW_COPY_AND_ASSIGN(Validator);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_VALIDATOR_H_
