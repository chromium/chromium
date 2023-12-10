// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"

class PrefService;

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

// Converts the state defined by the DLC Service into our own PackResult.
PackResult ConvertDlcStateToPackResult(const dlcservice::DlcState& dlc_state);

// Converts the install result defined by the DLC Service into our own
// PackResult.
PackResult ConvertDlcInstallResultToPackResult(
    const DlcserviceClient::InstallResult& install_result);

// Converts the error string returned by the DLC Service into our own
// ErrorCode enum.
PackResult::ErrorCode ConvertDlcErrorToErrorCode(std::string_view dlc_error);

// Resolves the received locale to a canonical one that we keep in our mapping
// from locales to DLC IDs.
const std::string ResolveLocale(const std::string& feature_id,
                                const std::string& input_locale);

// Returns true if we currently are in the OOBE flow.
bool IsOobe();

// This function takes a collection of strings and a callback that performs
// strings mapping. It applies mapping and outputs a set that includes all the
// filtered strings from the input.
base::flat_set<std::string> MapThenFilterStrings(
    base::span<const std::string> inputs,
    base::RepeatingCallback<std::optional<std::string>(const std::string&)>
        input_mapping);

// Extracts the set of input method IDs from the appropriate user Pref.
std::vector<std::string> ExtractInputMethodsFromPrefs(PrefService* prefs);

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACKS_UTIL_H_
