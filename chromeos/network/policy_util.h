// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_POLICY_UTIL_H_
#define CHROMEOS_NETWORK_POLICY_UTIL_H_

#include <map>
#include <memory>
#include <string>

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

struct NetworkProfile;

namespace policy_util {

// This fake credential contains a random postfix which is extremely unlikely to
// be used by any user. Used to determine saved but unknown credential
// (PSK/Passphrase/Password) in UI (see network_password_input.js).
extern const char kFakeCredential[];

using GuidToPolicyMap =
    std::map<std::string, std::unique_ptr<base::DictionaryValue>>;

// Creates a managed ONC dictionary from the given arguments. Depending on the
// profile type, the policies are assumed to come from the user or device policy
// and and |user_settings| to be the user's non-shared or shared settings.
// Each of the arguments can be NULL.
// TODO(pneubeck): Add documentation of the returned format, see
//   https://crbug.com/408990 .
std::unique_ptr<base::Value> CreateManagedONC(
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings,
    const base::DictionaryValue* active_settings,
    const NetworkProfile* profile);

// Adds properties to |shill_properties_to_update|, which are enforced on an
// unmanaged network by the global config |global_network_policy| of the policy.
// |shill_dictionary| are the network's current properties read from Shill.
void SetShillPropertiesForGlobalPolicy(
    const base::DictionaryValue& shill_dictionary,
    const base::DictionaryValue& global_network_policy,
    base::DictionaryValue* shill_properties_to_update);

// Creates a Shill property dictionary from the given arguments. The resulting
// dictionary will be sent to Shill by the caller. Depending on the profile
// type, |network_policy| is interpreted as the user or device policy and
// |user_settings| as the user or shared settings. |network_policy| or
// |user_settings| can be NULL, but not both.
std::unique_ptr<base::DictionaryValue> CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::DictionaryValue* global_policy,
    const base::DictionaryValue* network_policy,
    const base::DictionaryValue* user_settings);

// Returns the policy from |policies| matching |actual_network|, if any exists.
// Returns NULL otherwise. |actual_network| must be part of a ONC
// NetworkConfiguration.
const base::DictionaryValue* FindMatchingPolicy(
    const GuidToPolicyMap& policies,
    const base::DictionaryValue& actual_network);

}  // namespace policy_util

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_POLICY_UTIL_H_
