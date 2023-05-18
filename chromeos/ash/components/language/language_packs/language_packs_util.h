// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_

#include <string>

#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/language/language_packs/language_pack_manager.h"

namespace ash::language_packs {

// Returns the enum value of a feature ID that matches the corresponding value
// in the UMA Histogram enum.
FeatureIdsEnum GetFeatureIdValueForUma(const std::string& feature_id);

// Returns the enum value of a success or failure for a given Feature ID.
// These values match the corresponding UMA histogram enum
// "LanguagePackFeatureSuccess".
FeatureSuccessEnum GetSuccessValueForUma(const std::string& feature_id,
                                         const bool success);

// Returns the enum value of a error type received from DLC Service.
DlcErrorTypeEnum GetDlcErrorTypeForUma(const std::string& error_str);

// PackResult that is returned by an invalid feature ID is specified.
PackResult CreateInvalidDlcPackResult();

// Converts the state defined by the DLC Service into our own PackResult proto.
PackResult ConvertDlcStateToPackResult(const dlcservice::DlcState& dlc_state);

// Resolves the received locale to a canonical one that we keep in our mapping
// from locales to DLC IDs.
const std::string ResolveLocaleForHandwriting(const std::string& input_locale);
const std::string ResolveLocaleForTts(const std::string& input_locale);

// Returns true if we currently are in the OOBE flow.
bool IsOobe();

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_
