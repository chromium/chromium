// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

// This class manages the various address Autofill tables within the SQLite
// database passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
// -----------------------------------------------------------------------------
// contact_info         This table contains Autofill profile data synced from a
//                      remote source.
//
//                      It has all the same fields as the local_addresses table,
//                      below.
// -----------------------------------------------------------------------------
// local_addresses      This table contains kLocalOrSyncable Autofill profiles.
//                      It has the same layout as the contact_info table.
//
//   guid               A guid string to uniquely identify the profile.
//   use_count          The number of times this profile has been used to fill a
//                      form.
//   use_date           The last (use_date), second last (use_date2) and third
//   use_date2          last date (use_date3) at which this profile was used to
//   use_date3          fill a form, in time_t.
//   date_modified      The date on which this profile was last modified, in
//                      time_t.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//   label              A label intended to be chosen by the user. This was
//                      however never implemented and is currently unused.
//   initial_creator_id The application that initially created the profile.
//                      Represented as an integer. See AutofillProfile.
//   last_modifier_id   The application that performed the last non-metadata
//                      modification of the profile.
//                      Represented as an integer. See AutofillProfile.
// -----------------------------------------------------------------------------
// contact_info_type_tokens
//                      Contains the values for all relevant FieldTyps of a
//                      contact_info entry. At most one entry per (guid, type)
//                      pair exists.
//
//                      It has all the same fields as the
//                      local_addresses_type_tokens table, below.
// -----------------------------------------------------------------------------
// local_addresses_type_tokens
//                      Like contact_info_type_tokens, but for local_addresses.
//
//  guid                The guid of the corresponding profile in contact_info.
//  type                The FieldType, represented by its integer value in
//                      the FieldType enum.
//  value               The string value of the type.
//  verification_status Each token has an additional validation status that
//                      indicates if Autofill parsed the value out of an
//                      unstructured token, or if Autofill formatted the token
//                      from a structured subcomponent, or if the value was
//                      observed in a form submission, or even validated by the
//                      user in the settings.
//  observations        An encoding of the observations stored for this `type`.
//                      See `ProfileTokenConfidence::
//                      SerializeObservationsForStoredType()`.
// -----------------------------------------------------------------------------
class AddressAutofillTable : public WebDatabaseTable {
 public:
  AddressAutofillTable();

  AddressAutofillTable(const AddressAutofillTable&) = delete;
  AddressAutofillTable& operator=(const AddressAutofillTable&) = delete;

  ~AddressAutofillTable() override;

  // Retrieves the AddressAutofillTable* owned by |db|.
  static AddressAutofillTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records a single Autofill profile in the autofill_profiles table.
  bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Multi-value aware.
  bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes the Autofill profile with the given `guid`. `profile_source`
  // indicates where the profile was synced from and thus whether it is stored
  // in `kAutofillProfilesTable` or `kContactInfoTable`.
  bool RemoveAutofillProfile(const std::string& guid,
                             AutofillProfile::Source profile_source);

  // Removes all profiles from the given `profile_source`.
  bool RemoveAllAutofillProfiles(AutofillProfile::Source profile_source);

  // Retrieves a profile with guid `guid` from `kAutofillProfilesTable` or
  // `kContactInfoTable`.
  std::unique_ptr<AutofillProfile> GetAutofillProfile(
      const std::string& guid,
      AutofillProfile::Source profile_source) const;

  // Retrieves profiles in the database. They are returned in unspecified order.
  // The `profile_source` specifies if profiles from the legacy or the remote
  // backend should be retrieved.
  bool GetAutofillProfiles(
      AutofillProfile::Source profile_source,
      std::vector<std::unique_ptr<AutofillProfile>>& profiles) const;

  // Removes rows from local_addresses tables if they were created on or after
  // `delete_begin` and strictly before `delete_end`. Returns the list of
  // of deleted profiles in `profiles`. Return value is true if all rows were
  // successfully removed. Returns false on database error. In that case, the
  // output vector state is undefined, and may be partially filled.
  // TODO(crbug.com/40151750): This function is solely used to remove browsing
  // data. Once explicit save dialogs are fully launched, it can be removed. For
  // this reason profiles in the `contact_info` table are not considered.
  bool RemoveAutofillDataModifiedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      std::vector<std::unique_ptr<AutofillProfile>>& profiles);

  // Table migration functions. NB: These do not and should not rely on other
  // functions in this class. The implementation of a function such as
  // `GetAutofillProfile()` may change over time, but MigrateToVersionXX should
  // never change.
  bool MigrateToVersion88AddNewNameColumns();
  bool MigrateToVersion90AddNewStructuredAddressColumns();
  bool MigrateToVersion91AddMoreStructuredAddressColumns();
  bool MigrateToVersion92AddNewPrefixedNameColumn();
  bool MigrateToVersion93AddAutofillProfileLabelColumn();
  bool MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn();
  bool MigrateToVersion99RemoveAutofillProfilesTrashTable();
  bool MigrateToVersion100RemoveProfileValidityBitfieldColumn();
  bool MigrateToVersion102AddAutofillBirthdatesTable();
  bool MigrateToVersion107AddContactInfoTables();
  bool MigrateToVersion110AddInitialCreatorIdAndLastModifierId();
  bool MigrateToVersion113MigrateLocalAddressProfilesToNewTable();
  bool MigrateToVersion114DropLegacyAddressTables();
  bool MigrateToVersion117AddProfileObservationColumn();
  bool MigrateToVersion121DropServerAddressTables();
  bool MigrateToVersion132AddAdditionalLastUseDateColumns();

 private:
  // Reads profiles from the deprecated autofill_profiles table.
  std::unique_ptr<AutofillProfile> GetAutofillProfileFromLegacyTable(
      const std::string& guid) const;
  bool GetAutofillProfilesFromLegacyTable(
      std::vector<std::unique_ptr<AutofillProfile>>& profiles) const;

  bool InitLegacyProfileAddressesTable();
  bool InitProfileMetadataTable(AutofillProfile::Source source);
  bool InitProfileTypeTokensTable(AutofillProfile::Source source);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_
