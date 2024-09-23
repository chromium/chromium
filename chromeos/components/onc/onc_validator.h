// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_ONC_VALIDATOR_H_
#define CHROMEOS_COMPONENTS_ONC_ONC_VALIDATOR_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_mapper.h"
#include "components/onc/onc_constants.h"

namespace chromeos::onc {

struct OncValueSignature;

// *** ONC Validator Modes of Operation ***
// The ONC validator supports different modes of operation depending on the
// combination of flags passed to the constructor.
//
// ** |log_warnings| **
// If this flag is set to true, warnings will be logged.
//
// ** |error_on_unknown_field| **
// If this flag is set to true, and error will be logged in case of unknown
// fields are encountered in the ONC to be validated and the validation will
// fail. If it is set to false, a warning will be logged instead and the
// validation will not fail for that cause.
//
// ** |error_on_wrong_recommended| **
// If this flag is set to true, an error will be logged and the validation will
// fail in case of encountering recommended fields that are not expected to be
// recommended. If it is set to false, a warning will be logged instead and the
// validation will not fail for that cause.
//
// ** |managed_onc| **
// ONC set by policy are validated differently from ONC set through UI.
// Set this flag to true if policy is the source of the ONC to be validated.
//
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
class COMPONENT_EXPORT(CHROMEOS_ONC) Validator : public Mapper {
 public:
  enum Result { VALID, VALID_WITH_WARNINGS, INVALID };

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

  Validator(const Validator&) = delete;
  Validator& operator=(const Validator&) = delete;

  ~Validator() override;

  // Sets the ONC source to |source|. If not set, defaults to ONC_SOURCE_NONE.
  // If the source is set to ONC_SOURCE_DEVICE_POLICY, validation additionally
  // checks:
  // - only the network types Wifi and Ethernet are allowed
  // - client certificate patterns are disallowed
  void SetOncSource(::onc::ONCSource source) { onc_source_ = source; }

  // Validate the given |onc_object| dictionary according to |object_signature|.
  // The |object_signature| has to be a pointer to one of the signatures in
  // |onc_signature.h|. If an error is found, the function returns nullopt and
  // sets |result| to INVALID. If possible (no error encountered) a Clone is
  // created that contains all but the invalid fields and values and returns
  // this "repaired" object. That means, if not handled as an error, then the
  // following are dropped from the copy:
  // - unknown fields
  // - invalid field names in kRecommended arrays
  // - kRecommended fields in an unmanaged ONC
  // If any of these cases occurred, sets |result| to VALID_WITH_WARNINGS and
  // otherwise to VALID.
  // For details, see the class comment.
  std::optional<base::Value::Dict> ValidateAndRepairObject(
      const OncValueSignature* object_signature,
      const base::Value::Dict& onc_object,
      Result* result);

  // Returns the list of validation results that occurred within validation
  // initiated by the last call to |ValidateAndRepairObject|.
  const std::vector<ValidationIssue>& validation_issues() const {
    return validation_issues_;
  }

 private:
  // Overridden from Mapper:
  // Compare |onc_value|s type with |onc_type| and validate/repair according to
  // |signature|. On error returns a Value of type base::Value::Type::NONE.
  base::Value MapValue(const OncValueSignature& signature,
                       const base::Value& onc_value,
                       bool* error) override;

  // Dispatch to the right validation function according to
  // |signature|. Iterates over all fields and recursively validates/repairs
  // these. All valid fields are added to the result dictionary. Returns the
  // repaired dictionary.
  base::Value::Dict MapObject(const OncValueSignature& signature,
                              const base::Value::Dict& onc_object,
                              bool* error) override;

  // Pushes/pops the |field_name| to |path_|, otherwise like |Mapper::MapField|.
  base::Value MapField(const std::string& field_name,
                       const OncValueSignature& object_signature,
                       const base::Value& onc_value,
                       bool* found_unknown_field,
                       bool* error) override;

  // Ignores nested errors in NetworkConfigurations and Certificates, otherwise
  // like |Mapper::MapArray|.
  base::Value::List MapArray(const OncValueSignature& array_signature,
                             const base::Value::List& onc_array,
                             bool* nested_error) override;

  // Pushes/pops the index to |path_|, otherwise like |Mapper::MapEntry|.
  base::Value MapEntry(int index,
                       const OncValueSignature& signature,
                       const base::Value& onc_value,
                       bool* error) override;

  // This is the default validation of objects/dictionaries. Validates
  // |onc_object| according to |object_signature|. |result| must point to a
  // dictionary into which the repaired fields are written.
  bool ValidateObjectDefault(const OncValueSignature& object_signature,
                             const base::Value::Dict& onc_object,
                             base::Value::Dict* result);

  // Validates/repairs the kRecommended array in |result| according to
  // |object_signature| of the enclosing object.
  bool ValidateRecommendedField(const OncValueSignature& object_signature,
                                base::Value::Dict* result);

  // Validates the ClientCert* fields in a VPN or EAP object. Only if
  // |allow_cert_type_none| is true, the value "None" is allowed as
  // ClientCertType.
  bool ValidateClientCertFields(bool allow_cert_type_none,
                                base::Value::Dict* result);

  bool ValidateToplevelConfiguration(base::Value::Dict* result);
  bool ValidateNetworkConfiguration(base::Value::Dict* result);
  bool ValidateCellular(base::Value::Dict* result);
  bool ValidateAPN(base::Value::Dict* result);
  bool ValidateEthernet(base::Value::Dict* result);
  bool ValidateIPConfig(base::Value::Dict* result, bool require_fields = true);
  bool ValidateNameServersConfig(base::Value::Dict* result);
  bool ValidateWiFi(base::Value::Dict* result);
  bool ValidateVPN(base::Value::Dict* result);
  bool ValidateIPsec(base::Value::Dict* result);
  bool ValidateOpenVPN(base::Value::Dict* result);
  bool ValidateWireGuard(base::Value::Dict* result);
  bool ValidateThirdPartyVPN(base::Value::Dict* result);
  bool ValidateARCVPN(base::Value::Dict* result);
  bool ValidateVerifyX509(base::Value::Dict* result);
  bool ValidateCertificatePattern(base::Value::Dict* result);
  bool ValidateGlobalNetworkConfiguration(base::Value::Dict* result);
  bool ValidateProxySettings(base::Value::Dict* result);
  bool ValidateProxyLocation(base::Value::Dict* result);
  bool ValidateEAP(base::Value::Dict* result);
  bool ValidateSubjectAlternativeNameMatch(base::Value::Dict* result);
  bool ValidateCertificate(base::Value::Dict* result);
  bool ValidateScope(base::Value::Dict* result);
  bool ValidateTether(base::Value::Dict* result);
  void ValidateEthernetConfigs(base::Value::List* result);
  void OnlyKeepLast(base::Value::List* network_configurations_list,
                    const std::vector<std::string>& guids,
                    const char* type_for_messages);
  void RemoveNetworkConfigurationWithGuid(
      base::Value::List* network_configurations_list,
      const std::string& guid_to_remove);

  bool IsValidValue(const std::string& field_value,
                    const std::vector<const char*>& valid_values);

  bool IsInDevicePolicy(base::Value::Dict* result, std::string_view field_name);

  bool FieldExistsAndHasNoValidValue(
      const base::Value::Dict& object,
      const std::string& field_name,
      const std::vector<const char*>& valid_values);

  bool FieldExistsAndIsNotInRange(const base::Value::Dict& object,
                                  const std::string& field_name,
                                  int lower_bound,
                                  int upper_bound);

  bool FieldExistsAndIsEmpty(const base::Value::Dict& object,
                             const std::string& field_name);

  // Validates 'StaticIPConfig' field of the given network configuration. This
  // method takes 'NetworkConfiguration' dict instead of 'StaticIPConfig' dict
  // because it needs other 'NetworkConfiguration' fields (e.g.
  // 'IPAddressConfigType' and 'NameServersConfigType') to check correctness of
  // the 'StaticIPConfig' field.
  bool NetworkHasCorrectStaticIPConfig(base::Value::Dict* network);

  // Validates that the given field either exists or is recommended.
  bool FieldShouldExistOrBeRecommended(const base::Value::Dict& object,
                                       const std::string& field_name);

  bool OnlyOneFieldSet(const base::Value::Dict& object,
                       const std::string& field_name1,
                       const std::string& field_name2);

  bool ListFieldContainsValidValues(
      const base::Value::Dict& object,
      const std::string& field_name,
      const std::vector<const char*>& valid_values);

  bool ValidateSSIDAndHexSSID(base::Value::Dict* object);

  // Returns true if |key| is a key of |dict|. Otherwise, returns false and,
  // depending on |error_on_missing_field_| raises an error or a warning.
  bool RequireField(const base::Value::Dict& dict, const std::string& key);

  // Returns true if the GUID is unique or if the GUID is not a string
  // and false otherwise. The function also adds the GUID to a set in
  // order to identify duplicates.
  bool CheckGuidIsUniqueAndAddToSet(const base::Value::Dict& dict,
                                    const std::string& kGUID,
                                    std::set<std::string>* guids);

  // Returns true if the list of admin APN IDs provided by the |dict|'s
  // |key_list_of_ids| field are all non-empty. The function also adds the IDs
  // to |admin_assigned_apn_ids_|. |key_list_of_ids| must be either
  // onc::cellular::kAdminAssignedAPNIds or
  // onc::global_network_config::kPSIMAdminAssignedAPNIds.
  bool CheckAdminAssignedAPNIdsAreNonEmptyAndAddToSet(
      const base::Value::Dict& dict,
      const std::string& key_list_of_ids);

  // Prohibit global network configuration in user ONC imports.
  bool IsGlobalNetworkConfigInUserImport(const base::Value::Dict& onc_object);

  void AddValidationIssue(bool is_error, const std::string& debug_info);

  const bool error_on_unknown_field_;
  const bool error_on_wrong_recommended_;
  const bool error_on_missing_field_;
  const bool managed_onc_;
  const bool log_warnings_;

  ::onc::ONCSource onc_source_ = ::onc::ONC_SOURCE_NONE;

  // The path of field names and indices to the current value. Indices
  // are stored as strings in decimal notation.
  std::vector<std::string> path_;

  // Accumulates all network GUIDs during validation. Used to identify
  // duplicate GUIDs.
  std::set<std::string> network_guids_;

  // Accumulates all admin assigned APN IDs during validation. Used to identify
  // if the APNs provided by the admin at ::onc::toplevel_config::kAdminAPNList
  // contains APNs for all admin APN IDs referenced.
  std::set<std::string> admin_assigned_apn_ids_;

  // Accumulates all certificate GUIDs during validation. Used to identify
  // duplicate GUIDs.
  std::set<std::string> certificate_guids_;

  // List of all validation issues that occurred within validation initiated by
  // function ValidateAndRepairObject.
  std::vector<ValidationIssue> validation_issues_;
};

}  // namespace chromeos::onc

#endif  // CHROMEOS_COMPONENTS_ONC_ONC_VALIDATOR_H_
