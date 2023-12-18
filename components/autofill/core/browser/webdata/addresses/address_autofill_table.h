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
//
// DEPRECATED. Use local_addresses instead.
// autofill_profiles    This table contains Autofill profile data added by the
//                      user with the Autofill dialog.  Most of the columns are
//                      standard entries in a contact information form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   label              A user-chosen and user-visible label for the profile to
//                      help identifying the semantics of the profile. The user
//                      can choose an arbitrary string in principle, but the
//                      values '$HOME$' and '$WORK$' indicate a special meaning.
//   company_name
//   street_address     The combined lines of the street address.
//                      Added in version 54.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.  Added in version 54.
//   city
//   state
//   zipcode
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.  The canonical example is CEDEX in France.
//                      Added in version 54.
//   country_code
//   use_count          The number of times this profile has been used to fill
//                      a form. Added in version 61.
//   use_date           The date this profile was last used to fill a form,
//                      in time_t. Added in version 61.
//   date_modified      The date on which this profile was last modified, in
//                      time_t. Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//                      Added in version 56.
//   disallow_settings_visible_updates
//                      If true, a profile does not qualify to get merged with
//                      a profile observed in a form submission.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_addresses
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//                      This table stores the structured address information.
//   street_address     Stores the street address. This field is also stored in
//                      the profile table and is used to detect if a legacy
//                      client that does not support writing to this table
//                      changed the address. If this is true, the address stored
//                      in the table is removed.
//   street_name        The name of the street.
//   dependent_street_name
//                      The name of the crossing street.
//   house_number       The house number.
//   subpremise         The floor, apartment number and staircase.
//                      apartment number.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.
//   city               The city information of the address.
//   state              The state information of the address.
//   zip_code           The zip code of the address.
//   country_code       The code of the country of the address.
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.
//   premise_name       The name of the premise.
//   apartment_number   The number of the apartment.
//   floor              The floor in which the apartment is located.
//   street_address_status
//   street_name_status
//   dependent_street_name_status
//   house_number_status
//   subpremise_status
//   premise_name_status
//   dependent_locality_status
//   city_status
//   state_status
//   zip_code_status
//   country_code_status
//   sorting_code_status
//   apartment_number_status
//   floor_status
//                      Each token of the address has an additional validation
//                      status that indicates if Autofill parsed the value out
//                      of an unstructured (last) name, or if autofill formatted
//                      the token from its structured subcomponents, or if the
//                      value was observed in a form submission, or even
//                      validated by the user in the settings.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_names
//                      This table contains the multi-valued name fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//   honorific_prefix   The honorific prefix of a person like Ms, Mr or Prof
//   first_name         The first name of a person.
//   middle_name        The middle name or even names of a person.
//   last_name          The unstructured last name that is a combination of the
//                      first and second last name.
//   first_last_name    The first part of the last name. Mostly used for
//                      Latinx/Hispanic last names.
//   conjunction_last_name
//                      An optional conjunction that is mostly used in
//                      Hispanic/Latinx last names in between the first and
//                      second last name in the unstructured representation.
//   second_last_name   The second part of the last names. Last names only
//                      consisting of a single part are stored in the second
//                      part by default.
//   full_name          The unstructured full name of a person.
//   full_name_with_honorific_prefix
//                      The combination of the full name and the honorific
//                      prefix.
//   honorific_prefix_status
//   first_name_status
//   middle_name_status
//   last_name_status
//   first_last_name_status
//   conjunction_last_name_status
//   second_last_name_status
//   full_name_status
//   full_name_with_honorific_prefix_status
//                      Each token of the names has an additional validation
//                      status that indicates if Autofill parsed the value out
//                      of an unstructured (last) name, or if autofill formatted
//                      the token from its structured subcomponents, or if the
//                      value was observed in a form submission, or even
//                      validated by the user in the settings.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      phone number belongs.
//   number
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_birthdates
//                      This table contains the multi-valued birthdate fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      birthdate number belongs.
//   day                As an integer between 1 and 31 inclusive, or 0 if unset.
//   month              As an integer between 1 and 12 inclusive, or 0 if unset.
//   year               As a 4 digit integer, or 0 if unset.
//
// contact_info         This table contains Autofill profile data synced from a
//                      remote source.
// local_addresses      This table contains kLocalOrSyncable Autofill profiles.
//                      It has the same layout as the contact_info table.
//
//   guid               A guid string to uniquely identify the profile.
//   use_count          The number of times this profile has been used to fill a
//                      form.
//   use_date           The date this profile was last used to fill a form, in
//                      time_t.
//   date_modified      The date on which this profile was last modified, in
//                      time_t.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//   label              A user-chosen and user-visible label for the profile to
//                      help identifying the semantics of the profile. The user
//                      can choose an arbitrary string in principle, but the
//                      values '$HOME$' and '$WORK$' indicate a special meaning.
//   initial_creator_id The application that initially created the profile.
//                      Represented as an integer. See AutofillProfile.
//   last_modifier_id   The application that performed the last non-metadata
//                      modification of the profile.
//                      Represented as an integer. See AutofillProfile.
//
// contact_info_type_tokens
//                      Contains the values for all relevant FieldTyps of a
//                      contact_info entry. At most one entry per (guid, type)
//                      pair exists.
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
  virtual bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Multi-value aware.
  virtual bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes the Autofill profile with the given `guid`. `profile_source`
  // indicates where the profile was synced from and thus whether it is stored
  // in `kAutofillProfilesTable` or `kContactInfoTable`.
  virtual bool RemoveAutofillProfile(const std::string& guid,
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
  virtual bool GetAutofillProfiles(
      AutofillProfile::Source profile_source,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  // Deletes all data from the local_addresses tables. Returns true if any data
  // was deleted, false if not (so false means "commit not needed" rather than
  // "error").
  bool ClearAllLocalData();

  // Removes rows from local_addresses tables if they were created on or after
  // `delete_begin` and strictly before `delete_end`. Returns the list of
  // of deleted profiles in `profiles`. Return value is true if all rows were
  // successfully removed. Returns false on database error. In that case, the
  // output vector state is undefined, and may be partially filled.
  // TODO(crbug.com/1135188): This function is solely used to remove browsing
  // data. Once explicit save dialogs are fully launched, it can be removed. For
  // this reason profiles in the `contact_info` table are not considered.
  bool RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles);

  // Table migration functions. NB: These do not and should not rely on other
  // functions in this class. The implementation of a function such as
  // GetCreditCard may change over time, but MigrateToVersionXX should never
  // change.
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

 private:
  // Reads profiles from the deprecated autofill_profiles table.
  std::unique_ptr<AutofillProfile> GetAutofillProfileFromLegacyTable(
      const std::string& guid) const;
  bool GetAutofillProfilesFromLegacyTable(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  bool InitLegacyProfilesTable();
  bool InitLegacyProfileAddressesTable();
  bool InitLegacyProfileNamesTable();
  bool InitLegacyProfileEmailsTable();
  bool InitLegacyProfilePhonesTable();
  bool InitLegacyProfileBirthdatesTable();
  bool InitProfileMetadataTable(AutofillProfile::Source source);
  bool InitProfileTypeTokensTable(AutofillProfile::Source source);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_ADDRESS_AUTOFILL_TABLE_H_
