// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_PROFILE_IMPORT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_PROFILE_IMPORT_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// Command line flag that enables importing AutofillProfiles for manual testing.
// It expects the path to a file as a value, which contains profile descriptions
// in JSON format. The following format is expected:
// {
//   "profiles" : [
//     {
//       "source" : "localOrSyncable",
//       "field-type" : "value",
//       ...
//     },
//     ...
//   ]
// }
// The "source" is optional and can either be "account" or "localOrSyncable".
// This corresponds to the AutofillProfile::Source of the resulting profile. It
// default to "localOrSyncable".
// "field-type" corresponds to ServerFieldTypes like "NAME_FULL".
// All profiles specified in the file are imported. They replace any existing
// profiles.
// The profiles are expected to be fully structured
constexpr char kManualProfileImportForTestingFlag[] =
    "autofill-profiles-for-manual-testing";

// Given a description of fully structured profiles in the aforementioned JSON
// format, converts it to a vector of AutofillProfiles.
// If the JSON doesn't adhere to the above format, or if any of the profiles is
// not fully structured, an error message is returned.
// A profile is considered "fully structured" if `FinalizeAfterImport()` doesn't
// change it. This condition exists to prevent profiles from silently changing,
// since `FinalizeAfterImport()` is called when retrieving a profile from the
// database. For example, if the structure is invalid because the last name is
// not part of the full name, the routine will clear this information.
base::expected<std::vector<AutofillProfile>, std::string>
AutofillProfilesFromJSON(const base::Value& json);

// Checks if the `kManualProfileImportForTestingFlag` flag is present. If so,
// reads the specified file, parses the profile description and imports the
// profiles into the `pdm`.
// In case the import fails, the browser intentionally exist ungracefully.
// This is to prevent manual testing with incorrect data.
// Since importing is done in a separate thread, the `pdm` is passed as a weak
// ptr. It is updated once the import has finished.
void MaybeImportProfilesForManualTesting(
    base::WeakPtr<PersonalDataManager> pdm);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_PROFILE_IMPORT_H_
