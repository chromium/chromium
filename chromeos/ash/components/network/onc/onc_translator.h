// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATOR_H_

#include "base/component_export.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"

namespace chromeos::onc {
struct OncValueSignature;
}

namespace ash {

class NetworkState;

namespace onc {

// Translates a hierarchical ONC dictionary |onc_object| to a flat Shill
// dictionary. The |signature| declares the type of |onc_object| and must point
// to one of the signature objects in "onc_signature.h". The resulting Shill
// dictionary is returned.
//
// This function is used to translate network settings from ONC to Shill's
// format before sending them to Shill.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value::Dict TranslateONCObjectToShill(
    const chromeos::onc::OncValueSignature* signature,
    const base::Value::Dict& onc_object);

// Translates a |shill_dictionary| to an ONC object according to the given
// |onc_signature|. |onc_signature| must point to a signature object in
// "onc_signature.h". The resulting ONC object is returned.
//
// This function is used to translate network settings coming from Shill to ONC
// before sending them to the UI. The result doesn't have to be valid ONC, but
// only a subset of it and includes only the values that are actually required
// by the UI. If |onc_source| != ONC_SOURCE_UNKNOWN then the 'Source' property
// of the ONC dictionary will be set accordingly. Note: ONC_SOURCE_USER_IMPORT
// is treated the same as ONC_SOURCE_NONE. If |network_state| is provided it
// will be used to set the ErrorState property. Otherwise ErrorState will not be
// set.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value::Dict TranslateShillServiceToONCPart(
    const base::Value::Dict& shill_dictionary,
    ::onc::ONCSource onc_source,
    const chromeos::onc::OncValueSignature* onc_signature,
    const NetworkState* network_state);

}  // namespace onc
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_TRANSLATOR_H_
