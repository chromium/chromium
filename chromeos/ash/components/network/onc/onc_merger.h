// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_MERGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_MERGER_H_

#include "base/component_export.h"
#include "base/values.h"

namespace chromeos::onc {
struct OncValueSignature;
}

namespace ash::onc {

// Merges the given |user_settings| and |shared_settings| settings with the
// given |user_policy| and |device_policy| settings. Each can be omitted by
// providing nullptr. Each dictionary has to be part of a valid ONC dictionary.
// They don't have to describe top-level ONC but should refer to the same
// section in ONC. |user_settings| and |shared_settings| should not contain
// kRecommended fields. The resulting dictionary is valid ONC but may contain
// dispensable fields (e.g. in a network with type: "WiFi", the field "VPN" is
// dispensable) that can be removed by the caller using the ONC normalizer. ONC
// conformance of the arguments is not checked. Use ONC validator for that.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value::Dict MergeSettingsAndPoliciesToEffective(
    const base::Value::Dict* user_policy,
    const base::Value::Dict* device_policy,
    const base::Value::Dict* user_settings,
    const base::Value::Dict* shared_settings);

// Like MergeSettingsWithPoliciesToEffective but creates one dictionary in place
// of each field that exists in any of the argument dictionaries. Each of these
// dictionaries contains the onc::kAugmentations* fields (see onc_constants.h)
// for which a value is available. The onc::kAugmentationEffectiveSetting field
// contains the field name of the field containing the effective field that
// overrides all other values. Credentials from policies are not written to the
// result.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value::Dict MergeSettingsAndPoliciesToAugmented(
    const chromeos::onc::OncValueSignature& signature,
    const base::Value::Dict* user_policy,
    const base::Value::Dict* device_policy,
    const base::Value::Dict* user_settings,
    const base::Value::Dict* shared_settings,
    const base::Value::Dict* active_settings);

}  // namespace ash::onc

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_MERGER_H_
