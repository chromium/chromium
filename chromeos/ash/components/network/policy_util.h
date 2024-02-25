// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_

#include <optional>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/values.h"

namespace ash {

struct NetworkProfile;

namespace policy_util {

// This class represents a cellular activation code and its corresponding type
// and is used to simplify all cellular code related to enterprise policy.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) SmdxActivationCode {
 public:
  enum class Type {
    SMDP = 0,
    SMDS = 1,
  };

  SmdxActivationCode(Type type, std::string value);
  SmdxActivationCode(SmdxActivationCode&& other);
  SmdxActivationCode& operator=(SmdxActivationCode&& other);
  SmdxActivationCode(const SmdxActivationCode&) = delete;
  SmdxActivationCode& operator=(const SmdxActivationCode&) = delete;
  ~SmdxActivationCode() = default;

  // These functions return a string with information about this activation code
  // that is safe for logging. The ToErrorString() function will include a
  // sanitized version of the activation code value itself.
  std::string ToString() const;
  std::string ToErrorString() const;

  Type type() const { return type_; }
  const std::string& value() const { return value_; }

 private:
  std::string GetString(bool for_error_message) const;

  Type type_;
  std::string value_;
};

// This fake credential contains a random postfix which is extremely unlikely to
// be used by any user. Used to determine saved but unknown credential
// (PSK/Passphrase/Password) in UI (see onc_mojo.js).
extern COMPONENT_EXPORT(CHROMEOS_NETWORK) const char kFakeCredential[];

// Creates a managed ONC dictionary from the given arguments. Depending on the
// profile type, the policies are assumed to come from the user or device policy
// and and |user_settings| to be the user's non-shared or shared settings.
// Each of the arguments can be null.
// TODO(pneubeck): Add documentation of the returned format, see
//   https://crbug.com/408990 .
base::Value::Dict CreateManagedONC(const base::Value::Dict* global_policy,
                                   const base::Value::Dict* network_policy,
                                   const base::Value::Dict* user_settings,
                                   const base::Value::Dict* active_settings,
                                   const NetworkProfile* profile);

// Adds properties to |shill_properties_to_update|, which are enforced on an
// unmanaged network by the global config |global_network_policy| of the policy.
// |shill_dictionary| are the network's current properties read from Shill.
void SetShillPropertiesForGlobalPolicy(
    const base::Value::Dict& shill_dictionary,
    const base::Value::Dict& global_network_policy,
    base::Value::Dict& shill_properties_to_update);

// Creates a Shill property dictionary from the given arguments. The resulting
// dictionary will be sent to Shill by the caller. Depending on the profile
// type, |network_policy| is interpreted as the user or device policy and
// |user_settings| as the user or shared settings. |network_policy| or
// |user_settings| can be NULL, but not both.
base::Value::Dict CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::Value::Dict* global_policy,
    const base::Value::Dict* network_policy,
    const base::Value::Dict* user_settings);

// Returns true if |policy| matches |actual_network|, which must be part of a
// ONC NetworkConfiguration. This should be the only such matching function
// within Chrome. Shill does such matching in several functions for network
// identification. For compatibility, we currently should stick to Shill's
// matching behavior.
bool IsPolicyMatching(const base::Value::Dict& policy,
                      const base::Value::Dict& actual_network);

// Returns if the given |onc_config| is Cellular type configuration.
bool IsCellularPolicy(const base::Value::Dict& onc_config);

// Returns true if `onc_config` has any field that is marked as "Recommended".
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasAnyRecommendedField(const base::Value::Dict& onc_config);

// Returns the ICCID value from the given |onc_config|, returns nullptr if it
// is not a Cellular type ONC or no ICCID field is found.
const std::string* GetIccidFromONC(const base::Value::Dict& onc_config);

// Returns the Cellular.SMDPAddress ONC field of the passed ONC
// NetworkConfiguration if it is a Cellular NetworkConfiguration.
// If there is no SMDPAddress, returns nullptr.
const std::string* GetSMDPAddressFromONC(const base::Value::Dict& onc_config);

// This function returns the SM-DX activation code found in |onc_config|. If
// both an SM-DP+ activation code and an SM-DS activation code are provided, or
// if neither are provided, this function returns |std::nullopt|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::optional<SmdxActivationCode> GetSmdxActivationCodeFromONC(
    const base::Value::Dict& onc_config);

// When this is called, `AreEphemeralNetworkPoliciesEnabled()` will return true
// until the process is restarted (or
// ResetEphemeralNetworkPoliciesEnabledForTesting is called).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void SetEphemeralNetworkPoliciesEnabled();

// Resets the effect of SetEphemeralNetworkPoliciesEnabled.
// This is for unittests only - supporting this properly in production code
// would be difficult (e.g. no DCHECKs that the feature is enabled in posted
// tasks).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ResetEphemeralNetworkPoliciesEnabledForTesting();

// Returns true if ephemeral network policies are enabled.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool AreEphemeralNetworkPoliciesEnabled();

}  // namespace policy_util
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_
