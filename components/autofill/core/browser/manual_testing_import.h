// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_IMPORT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_IMPORT_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// Command line flags that enable importing AutofillProfiles for manual testing:
//   --autofill-profiles-content-for-manual-testing
//       Expects profile and credit card descriptions as a string in JSON
//       format.
//   --autofill-profiles-for-manual-testing
//       The same as above, but instead of the JSON content, expects the path
//       to a file with profile and credit card descriptions in JSON format.
// The following format is expected:
// {
//   "profiles" : [
//     {
//       "record_type" : "localOrSyncable",
//       "initial_creator_id" : 123,
//       "field-type" : "value",
//       ...
//     },
//     ...
//   ],
//   "credit-cards" : [
//     {
//       "nickname" : "...",
//       "field-type" : "value",
//       ...
//     }
//   ]
// }
// The "record_type" is optional and can either be "account" or
// "localOrSyncable". This corresponds to the AutofillProfile::RecordType of the
// resulting profile. It default to "localOrSyncable". The "initial_creator_id"
// is an optional int value which sets the profile's property of the same name.
// The "nickname" in credit cards optional as well. "field-type" corresponds to
// FieldTypes like "NAME_FULL". For profiles and credit cards, only field types
// valid for AutofillProfile or CreditCard are accepted. All profiles and credit
// cards specified in the file are imported. They replace any existing data. The
// profiles are expected to be fully structured.
// TODO(crbug.com/40255599): remove `profiles` from flags' name.
constexpr char kManualFileImportForTestingFlag[] =
    "autofill-profiles-for-manual-testing";
constexpr char kManualContentImportForTestingFlag[] =
    "autofill-profiles-content-for-manual-testing";

// Reads the contents of `file`, parses it as a JSON file and converts its
// content into a list of AutofillProfiles.
// If any step fails, an error message is logged and std::nullopt is returned.
std::optional<std::vector<AutofillProfile>> LoadProfilesFromFile(
    base::FilePath file);

// Reads the contents of `file`, parses it as a JSON file and converts its
// content into a list of CreditCards.
// If any step fails, an error message is logged and std::nullopt is returned.
std::optional<std::vector<CreditCard>> LoadCreditCardsFromFile(
    base::FilePath file);

// Given the array of descriptions of fully structured profiles in the
// aforementioned JSON format, converts it to a vector of AutofillProfiles.
// If the JSON list doesn't adhere to the above format, or if any of the
// profiles is not fully structured, an error is logged and std::nullopt is
// returned. A profile is considered "fully structured" if
// `FinalizeAfterImport()` doesn't change it. This condition exists to prevent
// profiles from silently changing, since `FinalizeAfterImport()` is called when
// retrieving a profile from the database. For example, if the structure is
// invalid because the last name is not part of the full name, the routine will
// clear this information.
std::optional<std::vector<AutofillProfile>> AutofillProfilesFromJSON(
    const base::Value::List* const profiles_json);

// Given the array of valid credit cards in the aforementioned JSON format,
// converts it to a vector of CreditCards.
// If the JSON list doesn't adhere to the above format, an error message is
// logged and std::nullopt is returned.
std::optional<std::vector<CreditCard>> CreditCardsFromJSON(
    const base::Value::List* const cards_json);

// Checks if the `kManualImportForTestingFlag` flag is present. If so,
// reads the specified file, parses the profiles and credit cards description
// and imports them into the `pdm`.
// In case the import fails, an error message is logged and the browser
// intentionally exits ungracefully. This is to prevent manual testing with
// incorrect data.
// Since importing is done in a separate thread, the `pdm` is passed as a weak
// ptr. It is updated once the import has finished.
void MaybeImportDataForManualTesting(base::WeakPtr<PersonalDataManager> pdm);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MANUAL_TESTING_IMPORT_H_
