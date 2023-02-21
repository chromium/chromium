// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"

namespace ash {

struct NetworkProfile;

namespace policy_util {

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
                                   const base::Value* user_settings,
                                   const base::Value::Dict* active_settings,
                                   const NetworkProfile* profile);

// Adds properties to |shill_properties_to_update|, which are enforced on an
// unmanaged network by the global config |global_network_policy| of the policy.
// |shill_dictionary| are the network's current properties read from Shill.
void SetShillPropertiesForGlobalPolicy(
    const base::Value::Dict& shill_dictionary,
    const base::Value::Dict& global_network_policy,
    base::Value::Dict* shill_properties_to_update);

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
    const base::Value* user_settings);

// Returns true if |policy| matches |actual_network|, which must be part of a
// ONC NetworkConfiguration. This should be the only such matching function
// within Chrome. Shill does such matching in several functions for network
// identification. For compatibility, we currently should stick to Shill's
// matching behavior.
bool IsPolicyMatching(const base::Value::Dict& policy,
                      const base::Value::Dict& actual_network);

// Returns if the given |onc_config| is Cellular type configuration.
bool IsCellularPolicy(const base::Value::Dict& onc_config);

// Returns the ICCID value from the given |onc_config|, returns nullptr if it
// is not a Cellular type ONC or no ICCID field is found.
const std::string* GetIccidFromONC(const base::Value::Dict& onc_config);

// Returns the Cellular.SMDPAddress ONC field of the passed ONC
// NetworkConfiguration if it is a Cellular NetworkConfiguration.
// If there is no SMDPAddress, returns nullptr.
const std::string* GetSMDPAddressFromONC(const base::Value::Dict& onc_config);

}  // namespace policy_util
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_UTIL_H_
