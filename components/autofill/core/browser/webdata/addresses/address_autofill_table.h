// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

// This class manages the various address Autofill tables within the SQLite
// database passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
// -----------------------------------------------------------------------------
// addresses            This table contains Autofill profile metadata.
//
//   guid               A guid string to uniquely identify the profile.
//   record_type        The AutofillProfile::RecordType of the profile, encoded
//                      by the enum's underlying integer.
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
// address_type_tokens  Contains the values for all relevant FieldTypes of an
//                      addresses entry. At most one entry per (guid, type)
//                      pair exists.
//
//  guid                The guid of the corresponding profile in addresses.
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

  // Removes the Autofill profile with the given `guid`. `record_type`
  // indicates where the profile was synced from.
  bool RemoveAutofillProfile(const std::string& guid);

  // Removes all profiles of the given `record_types`.
  bool RemoveAllAutofillProfiles(
      DenseSet<AutofillProfile::RecordType> record_types);

  // Retrieves a profile with guid `guid`.
  std::optional<AutofillProfile> GetAutofillProfile(
      const std::string& guid) const;

  // Retrieves profiles in the database. They are returned in unspecified order.
  // `record_types` specifies which record types to consider.
  bool GetAutofillProfiles(DenseSet<AutofillProfile::RecordType> record_types,
                           std::vector<AutofillProfile>& profiles) const;

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
  bool MigrateToVersion134UnifyLocalAndAccountAddressStorage();

 private:
  // Reads profiles from the deprecated autofill_profiles table.
  std::optional<AutofillProfile> GetAutofillProfileFromLegacyTable(
      const std::string& guid) const;
  bool GetAutofillProfilesFromLegacyTable(
      std::vector<AutofillProfile>& profiles) const;

  bool InitLegacyProfileAddressesTable();
  bool InitAddressesTable();
  bool InitAddressTypeTokensTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_
