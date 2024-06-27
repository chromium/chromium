// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace autofill {

namespace {

constexpr std::string_view kContactInfoTable = "contact_info";
constexpr std::string_view kLocalAddressesTable = "local_addresses";
constexpr std::string_view kGuid = "guid";
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kUseDate = "use_date";
constexpr std::string_view kDateModified = "date_modified";
constexpr std::string_view kLanguageCode = "language_code";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kInitialCreatorId = "initial_creator_id";
constexpr std::string_view kLastModifierId = "last_modifier_id";

constexpr std::string_view kContactInfoTypeTokensTable =
    "contact_info_type_tokens";
constexpr std::string_view kLocalAddressesTypeTokensTable =
    "local_addresses_type_tokens";
// kGuid = "guid"
constexpr std::string_view kType = "type";
constexpr std::string_view kValue = "value";
constexpr std::string_view kVerificationStatus = "verification_status";
constexpr std::string_view kObservations = "observations";

// Historically, a different schema was used and addresses were stored in a set
// of tables named autofill_profiles*. These tables are no longer used in
// production and only referenced in the migration logic. Do not add to them.
// Use the contact_info* and local_addresses* tables instead.
constexpr std::string_view kAutofillProfilesTable = "autofill_profiles";
// kGuid = "guid"
// kLabel = "label"
constexpr std::string_view kCompanyName = "company_name";
constexpr std::string_view kStreetAddress = "street_address";
constexpr std::string_view kDependentLocality = "dependent_locality";
constexpr std::string_view kCity = "city";
constexpr std::string_view kState = "state";
constexpr std::string_view kZipcode = "zipcode";
constexpr std::string_view kSortingCode = "sorting_code";
constexpr std::string_view kCountryCode = "country_code";
// kUseCount = "use_count"
// kUseDate = "use_date"
// kDateModified = "date_modified"
// kLanguageCode = "language_code"
constexpr std::string_view kDisallowSettingsVisibleUpdates =
    "disallow_settings_visible_updates";

constexpr std::string_view kAutofillProfileAddressesTable =
    "autofill_profile_addresses";
// kGuid = "guid"
// kStreetAddress = "street_address"
constexpr std::string_view kStreetName = "street_name";
constexpr std::string_view kDependentStreetName = "dependent_street_name";
constexpr std::string_view kHouseNumber = "house_number";
constexpr std::string_view kSubpremise = "subpremise";
// kDependentLocality = "dependent_locality"
// kCity = "city"
// kState = "state"
constexpr std::string_view kZipCode = "zip_code";
// kCountryCode = "country_code"
// kSortingCode = "sorting_code"
constexpr std::string_view kApartmentNumber = "apartment_number";
constexpr std::string_view kFloor = "floor";
constexpr std::string_view kStreetAddressStatus = "street_address_status";
constexpr std::string_view kStreetNameStatus = "street_name_status";
constexpr std::string_view kDependentStreetNameStatus =
    "dependent_street_name_status";
constexpr std::string_view kHouseNumberStatus = "house_number_status";
constexpr std::string_view kSubpremiseStatus = "subpremise_status";
constexpr std::string_view kDependentLocalityStatus =
    "dependent_locality_status";
constexpr std::string_view kCityStatus = "city_status";
constexpr std::string_view kStateStatus = "state_status";
constexpr std::string_view kZipCodeStatus = "zip_code_status";
constexpr std::string_view kCountryCodeStatus = "country_code_status";
constexpr std::string_view kSortingCodeStatus = "sorting_code_status";
constexpr std::string_view kApartmentNumberStatus = "apartment_number_status";
constexpr std::string_view kFloorStatus = "floor_status";

constexpr std::string_view kAutofillProfileNamesTable =
    "autofill_profile_names";
// kGuid = "guid"
constexpr std::string_view kFirstName = "first_name";
constexpr std::string_view kMiddleName = "middle_name";
constexpr std::string_view kLastName = "last_name";
constexpr std::string_view kFirstLastName = "first_last_name";
constexpr std::string_view kConjunctionLastName = "conjunction_last_name";
constexpr std::string_view kSecondLastName = "second_last_name";
constexpr std::string_view kFullName = "full_name";
constexpr std::string_view kFirstNameStatus = "first_name_status";
constexpr std::string_view kMiddleNameStatus = "middle_name_status";
constexpr std::string_view kLastNameStatus = "last_name_status";
constexpr std::string_view kFirstLastNameStatus = "first_last_name_status";
constexpr std::string_view kConjunctionLastNameStatus =
    "conjunction_last_name_status";
constexpr std::string_view kSecondLastNameStatus = "second_last_name_status";
constexpr std::string_view kFullNameStatus = "full_name_status";

constexpr std::string_view kAutofillProfileEmailsTable =
    "autofill_profile_emails";
// kGuid = "guid"
constexpr std::string_view kEmail = "email";

constexpr std::string_view kAutofillProfilePhonesTable =
    "autofill_profile_phones";
// kGuid = "guid"
constexpr std::string_view kNumber = "number";

constexpr std::string_view kAutofillProfileBirthdatesTable =
    "autofill_profile_birthdates";
// kGuid = "guid"
constexpr std::string_view kDay = "day";
constexpr std::string_view kMonth = "month";
constexpr std::string_view kYear = "year";

void AddLegacyAutofillProfileDetailsFromStatement(sql::Statement& s,
                                                  AutofillProfile* profile) {
  int index = 0;
  for (FieldType type :
       {COMPANY_NAME, ADDRESS_HOME_STREET_ADDRESS,
        ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
        ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE, ADDRESS_HOME_COUNTRY}) {
    profile->SetRawInfo(type, s.ColumnString16(index++));
  }
  profile->set_use_count(s.ColumnInt64(index++));
  profile->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_modification_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_language_code(s.ColumnString(index++));
  profile->set_profile_label(s.ColumnString(index++));
}

bool AddLegacyAutofillProfileNamesToProfile(sql::Database* db,
                                            AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfileNamesTable)) {
    return false;
  }
  sql::Statement s;
  if (SelectByGuid(
          db, s, kAutofillProfileNamesTable,
          {kGuid, kFirstName, kFirstNameStatus, kMiddleName, kMiddleNameStatus,
           kFirstLastName, kFirstLastNameStatus, kConjunctionLastName,
           kConjunctionLastNameStatus, kSecondLastName, kSecondLastNameStatus,
           kLastName, kLastNameStatus, kFullName, kFullNameStatus},
          profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));

    int index = 1;
    for (FieldType type :
         {NAME_FIRST, NAME_MIDDLE, NAME_LAST_FIRST, NAME_LAST_CONJUNCTION,
          NAME_LAST_SECOND, NAME_LAST, NAME_FULL}) {
      profile->SetRawInfoWithVerificationStatusInt(
          type, s.ColumnString16(index), s.ColumnInt(index + 1));
      index += 2;
    }
  }
  return s.Succeeded();
}

bool AddLegacyAutofillProfileAddressesToProfile(sql::Database* db,
                                                AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfileAddressesTable)) {
    return false;
  }
  sql::Statement s;
  if (SelectByGuid(db, s, kAutofillProfileAddressesTable,
                   {kGuid,
                    kStreetAddress,
                    kStreetAddressStatus,
                    kStreetName,
                    kStreetNameStatus,
                    kHouseNumber,
                    kHouseNumberStatus,
                    kSubpremise,
                    kSubpremiseStatus,
                    kDependentLocality,
                    kDependentLocalityStatus,
                    kCity,
                    kCityStatus,
                    kState,
                    kStateStatus,
                    kZipCode,
                    kZipCodeStatus,
                    kSortingCode,
                    kSortingCodeStatus,
                    kCountryCode,
                    kCountryCodeStatus,
                    kApartmentNumber,
                    kApartmentNumberStatus,
                    kFloor,
                    kFloorStatus},
                   profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    std::u16string street_address = s.ColumnString16(1);
    std::u16string dependent_locality = s.ColumnString16(13);
    std::u16string city = s.ColumnString16(15);
    std::u16string state = s.ColumnString16(17);
    std::u16string zip_code = s.ColumnString16(19);
    std::u16string sorting_code = s.ColumnString16(21);
    std::u16string country = s.ColumnString16(23);

    std::u16string street_address_legacy =
        profile->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS);
    std::u16string dependent_locality_legacy =
        profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY);
    std::u16string city_legacy = profile->GetRawInfo(ADDRESS_HOME_CITY);
    std::u16string state_legacy = profile->GetRawInfo(ADDRESS_HOME_STATE);
    std::u16string zip_code_legacy = profile->GetRawInfo(ADDRESS_HOME_ZIP);
    std::u16string sorting_code_legacy =
        profile->GetRawInfo(ADDRESS_HOME_SORTING_CODE);
    std::u16string country_legacy = profile->GetRawInfo(ADDRESS_HOME_COUNTRY);

    // At this stage, the unstructured address was already written to
    // the profile. If the address was changed by a legacy client, the
    // information diverged from the one in this table that is only written by
    // new clients. In this case remove the corresponding row from this table.
    // Otherwise, read the new structured tokens and set the verification
    // statuses for all tokens.
    if (street_address == street_address_legacy &&
        dependent_locality == dependent_locality_legacy &&
        city == city_legacy && state == state_legacy &&
        zip_code == zip_code_legacy && sorting_code == sorting_code_legacy &&
        country == country_legacy) {
      int index = 1;
      for (FieldType type :
           {ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_NAME,
            ADDRESS_HOME_HOUSE_NUMBER, ADDRESS_HOME_SUBPREMISE,
            ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY,
            ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE,
            ADDRESS_HOME_COUNTRY, ADDRESS_HOME_APT_NUM, ADDRESS_HOME_FLOOR}) {
        profile->SetRawInfoWithVerificationStatusInt(
            type, s.ColumnString16(index), s.ColumnInt(index + 1));
        index += 2;
      }
    } else {
      // Remove the structured information from the table for
      // eventual deletion consistency.
      DeleteWhereColumnEq(db, kAutofillProfileAddressesTable, kGuid,
                          profile->guid());
    }
  }
  return s.Succeeded();
}

bool AddLegacyAutofillProfileEmailsToProfile(sql::Database* db,
                                             AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfileEmailsTable)) {
    return false;
  }
  // TODO(estade): update schema so that multiple emails are not associated
  // per unique profile guid. Please refer https://crbug.com/497934.
  sql::Statement s;
  if (SelectByGuid(db, s, kAutofillProfileEmailsTable, {kGuid, kEmail},
                   profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfo(EMAIL_ADDRESS, s.ColumnString16(1));
  }
  return s.Succeeded();
}

bool AddLegacyAutofillProfilePhonesToProfile(sql::Database* db,
                                             AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfilePhonesTable)) {
    return false;
  }
  // TODO(estade): update schema so that multiple phone numbers are not
  // associated per unique profile guid. Please refer
  // https://crbug.com/497934.
  sql::Statement s;
  if (SelectByGuid(db, s, kAutofillProfilePhonesTable, {kGuid, kNumber},
                   profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, s.ColumnString16(1));
  }
  return s.Succeeded();
}

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

time_t GetEndTime(base::Time end) {
  if (end.is_null() || end == base::Time::Max()) {
    return std::numeric_limits<time_t>::max();
  }

  return end.ToTimeT();
}

// This helper function binds the `profile`s properties to the placeholders in
// `s`, in the order the columns are defined in the header file.
void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    sql::Statement& s) {
  int index = 0;
  s.BindString(index++, profile.guid());
  s.BindInt64(index++, profile.use_count());
  s.BindInt64(index++, profile.use_date().ToTimeT());
  s.BindInt64(index++, profile.modification_date().ToTimeT());
  s.BindString(index++, profile.language_code());
  s.BindString(index++, profile.profile_label());
  s.BindInt(index++, profile.initial_creator_id());
  s.BindInt(index++, profile.last_modifier_id());
}

// Local and account profiles are stored in different tables with the same
// layout. One table contains profile-level metadata, while another table
// contains the values for every relevant FieldType. The following two
// functions are used to map from a profile's `source` to the correct table.
std::string_view GetProfileMetadataTable(AutofillProfile::Source source) {
  switch (source) {
    case AutofillProfile::Source::kLocalOrSyncable:
      return kLocalAddressesTable;
    case AutofillProfile::Source::kAccount:
      return kContactInfoTable;
  }
  NOTREACHED_NORETURN();
}
std::string_view GetProfileTypeTokensTable(AutofillProfile::Source source) {
  switch (source) {
    case AutofillProfile::Source::kLocalOrSyncable:
      return kLocalAddressesTypeTokensTable;
    case AutofillProfile::Source::kAccount:
      return kContactInfoTypeTokensTable;
  }
  NOTREACHED_NORETURN();
}

// Inserts `profile` into `GetProfileMetadataTable()` and
// `GetProfileTypeTokensTable()`, depending on the profile's source.
bool AddAutofillProfileToTable(sql::Database* db,
                               const AutofillProfile& profile) {
  sql::Statement s;
  InsertBuilder(db, s, GetProfileMetadataTable(profile.source()),
                {kGuid, kUseCount, kUseDate, kDateModified, kLanguageCode,
                 kLabel, kInitialCreatorId, kLastModifierId});
  BindAutofillProfileToStatement(profile, s);
  if (!s.Run()) {
    return false;
  }
  for (FieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForAddressOverflowAndLandmark) &&
        type == ADDRESS_HOME_OVERFLOW_AND_LANDMARK) {
      continue;
    }
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForBetweenStreetsOrLandmark) &&
        type == ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK) {
      continue;
    }

    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForAddressOverflow) &&
        type == ADDRESS_HOME_OVERFLOW) {
      continue;
    }
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForLandmark) &&
        type == ADDRESS_HOME_LANDMARK) {
      continue;
    }
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForBetweenStreets) &&
        (type == ADDRESS_HOME_BETWEEN_STREETS ||
         type == ADDRESS_HOME_BETWEEN_STREETS_1 ||
         type == ADDRESS_HOME_BETWEEN_STREETS_2)) {
      continue;
    }
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForAdminLevel2) &&
        type == ADDRESS_HOME_ADMIN_LEVEL2) {
      continue;
    }
    if (!base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel) &&
        type == ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY) {
      continue;
    }
    InsertBuilder(db, s, GetProfileTypeTokensTable(profile.source()),
                  {kGuid, kType, kValue, kVerificationStatus, kObservations});
    s.BindString(0, profile.guid());
    s.BindInt(1, type);
    s.BindString16(2, Truncate(profile.GetRawInfo(type)));
    s.BindInt(3, profile.GetVerificationStatusInt(type));
    s.BindBlob(
        4, profile.token_quality().SerializeObservationsForStoredType(type));
    if (!s.Run()) {
      return false;
    }
  }
  return true;
}

// `MigrateToVersion113MigrateLocalAddressProfilesToNewTable()` migrates
// profiles from one table layout to another. This function inserts the given
// `profile` into the `GetProfileMetadataTable()` of schema version 113.
// `AddAutofillProfileToTable()` can't be reused, since the schema can change in
// future database versions in ways incompatible with version 113 (e.g. adding
// a column).
// The code was copied from `AddAutofillProfileToTable()` in version 113. Like
// the migration logic, it shouldn't be changed.
bool AddAutofillProfileToTableVersion113(sql::Database* db,
                                         const AutofillProfile& profile) {
  sql::Statement s;
  InsertBuilder(db, s, GetProfileMetadataTable(profile.source()),
                {kGuid, kUseCount, kUseDate, kDateModified, kLanguageCode,
                 kLabel, kInitialCreatorId, kLastModifierId});
  BindAutofillProfileToStatement(profile, s);
  if (!s.Run()) {
    return false;
  }
  // Note that `GetDatabaseStoredTypesOfAutofillProfile()` might change in
  // future versions. Due to the flexible layout of the type tokens table, this
  // is not a problem.
  for (FieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    InsertBuilder(db, s, GetProfileTypeTokensTable(profile.source()),
                  {kGuid, kType, kValue, kVerificationStatus});
    s.BindString(0, profile.guid());
    s.BindInt(1, type);
    s.BindString16(2, Truncate(profile.GetRawInfo(type)));
    s.BindInt(3, profile.GetVerificationStatusInt(type));
    if (!s.Run()) {
      return false;
    }
  }
  return true;
}

}  // namespace

AddressAutofillTable::AddressAutofillTable() = default;

AddressAutofillTable::~AddressAutofillTable() = default;

// static
AddressAutofillTable* AddressAutofillTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<AddressAutofillTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey AddressAutofillTable::GetTypeKey() const {
  return GetKey();
}

bool AddressAutofillTable::CreateTablesIfNecessary() {
  return InitProfileMetadataTable(AutofillProfile::Source::kAccount) &&
         InitProfileTypeTokensTable(AutofillProfile::Source::kAccount) &&
         InitProfileMetadataTable(AutofillProfile::Source::kLocalOrSyncable) &&
         InitProfileTypeTokensTable(AutofillProfile::Source::kLocalOrSyncable);
}

bool AddressAutofillTable::MigrateToVersion(int version,
                                            bool* update_compatible_version) {
  if (!db_->is_open()) {
    return false;
  }
  // Migrate if necessary.
  switch (version) {
    case 88:
      *update_compatible_version = false;
      return MigrateToVersion88AddNewNameColumns();
    case 90:
      *update_compatible_version = false;
      return MigrateToVersion90AddNewStructuredAddressColumns();
    case 91:
      *update_compatible_version = false;
      return MigrateToVersion91AddMoreStructuredAddressColumns();
    case 92:
      *update_compatible_version = false;
      return MigrateToVersion92AddNewPrefixedNameColumn();
    case 93:
      *update_compatible_version = false;
      return MigrateToVersion93AddAutofillProfileLabelColumn();
    case 96:
      *update_compatible_version = false;
      return MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn();
    case 99:
      *update_compatible_version = true;
      return MigrateToVersion99RemoveAutofillProfilesTrashTable();
    case 100:
      *update_compatible_version = true;
      return MigrateToVersion100RemoveProfileValidityBitfieldColumn();
    case 102:
      *update_compatible_version = false;
      return MigrateToVersion102AddAutofillBirthdatesTable();
    case 107:
      *update_compatible_version = false;
      return MigrateToVersion107AddContactInfoTables();
    case 110:
      *update_compatible_version = false;
      return MigrateToVersion110AddInitialCreatorIdAndLastModifierId();
    case 113:
      *update_compatible_version = false;
      return MigrateToVersion113MigrateLocalAddressProfilesToNewTable();
    case 114:
      *update_compatible_version = true;
      return MigrateToVersion114DropLegacyAddressTables();
    case 117:
      *update_compatible_version = false;
      return MigrateToVersion117AddProfileObservationColumn();
    case 121:
      *update_compatible_version = true;
      return MigrateToVersion121DropServerAddressTables();
  }
  return true;
}

bool AddressAutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  sql::Transaction transaction(db_);
  return transaction.Begin() && AddAutofillProfileToTable(db_, profile) &&
         transaction.Commit();
}

bool AddressAutofillTable::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  DCHECK(base::Uuid::ParseCaseInsensitive(profile.guid()).is_valid());

  std::unique_ptr<AutofillProfile> old_profile =
      GetAutofillProfile(profile.guid(), profile.source());
  if (!old_profile) {
    return false;
  }

  // Implementing an update as remove + add has multiple advantages:
  // - Prevents outdated (FieldType, value) pairs from remaining in the
  //   `GetProfileTypeTokensTable(profile)`, in case field types are removed.
  // - Simpler code.
  // The possible downside is performance. This is not an issue, as updates
  // happen rarely and asynchronously.
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         RemoveAutofillProfile(profile.guid(), profile.source()) &&
         AddAutofillProfileToTable(db_, profile) && transaction.Commit();
}

bool AddressAutofillTable::RemoveAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DeleteWhereColumnEq(db_, GetProfileMetadataTable(profile_source),
                             kGuid, guid) &&
         DeleteWhereColumnEq(db_, GetProfileTypeTokensTable(profile_source),
                             kGuid, guid) &&
         transaction.Commit();
}

bool AddressAutofillTable::RemoveAllAutofillProfiles(
    AutofillProfile::Source profile_source) {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         Delete(db_, GetProfileMetadataTable(profile_source)) &&
         Delete(db_, GetProfileTypeTokensTable(profile_source)) &&
         transaction.Commit();
}

std::unique_ptr<AutofillProfile> AddressAutofillTable::GetAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source) const {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Statement s;
  if (!SelectByGuid(db_, s, GetProfileMetadataTable(profile_source),
                    {kUseCount, kUseDate, kDateModified, kLanguageCode, kLabel,
                     kInitialCreatorId, kLastModifierId},
                    guid)) {
    return nullptr;
  }

  int index = 0;
  const int64_t use_count = s.ColumnInt64(index++);
  const base::Time use_date = base::Time::FromTimeT(s.ColumnInt64(index++));
  const base::Time modification_date =
      base::Time::FromTimeT(s.ColumnInt64(index++));
  const std::string language_code = s.ColumnString(index++);
  const std::string profile_label = s.ColumnString(index++);
  const int creator_id = s.ColumnInt(index++);
  const int modifier_id = s.ColumnInt(index++);

  if (!SelectByGuid(db_, s, GetProfileTypeTokensTable(profile_source),
                    {kType, kValue, kVerificationStatus, kObservations},
                    guid)) {
    return nullptr;
  }

  struct FieldTypeData {
    // Type corresponding to the data entry.
    FieldType type;
    // Value corresponding to the entry type.
    std::u16string value;
    // VerificationStatus of the data entry's `value`.
    int status;
    // Serialized observations for the stored type.
    std::vector<uint8_t> serialized_data;
  };

  std::vector<FieldTypeData> field_type_values;
  std::string country_code;
  // As `SelectByGuid()` already calls `s.Step()`, do-while is used here.
  do {
    FieldType type = ToSafeFieldType(s.ColumnInt(0), UNKNOWN_TYPE);
    if (!GetDatabaseStoredTypesOfAutofillProfile().contains(type)) {
      // This is possible in two cases:
      // - The database was tampered with by external means.
      // - The type corresponding to `s.ColumnInt(0)` was deprecated. In this
      //   case, due to the structure of
      //   `GetProfileTypeTokensTable(profile_source)`, it is not necessary to
      //   add database migration logic or drop a column. Instead, during the
      //   next update, the data will be dropped.
      continue;
    }

    base::span<const uint8_t> observations_data = s.ColumnBlob(3);
    field_type_values.emplace_back(
        type, s.ColumnString16(1), s.ColumnInt(2),
        std::vector<uint8_t>(observations_data.begin(),
                             observations_data.end()));

    if (type == ADDRESS_HOME_COUNTRY) {
      country_code = base::UTF16ToUTF8(s.ColumnString16(1));
    }

  } while (s.Step());

  // TODO(crbug.com/40275657): Define a proper migration strategy from stored
  // legacy profiles into i18n ones.
  auto profile = std::make_unique<AutofillProfile>(
      guid, profile_source, AddressCountryCode(country_code));
  profile->set_use_count(use_count);
  profile->set_use_date(use_date);
  profile->set_modification_date(modification_date);
  profile->set_language_code(language_code);
  profile->set_profile_label(profile_label);
  profile->set_initial_creator_id(creator_id);
  profile->set_last_modifier_id(modifier_id);

  for (const auto& data : field_type_values) {
    profile->SetRawInfoWithVerificationStatusInt(data.type, data.value,
                                                 data.status);
    profile->token_quality().LoadSerializedObservationsForStoredType(
        data.type, data.serialized_data);
  }

  profile->FinalizeAfterImport();
  return profile;
}

bool AddressAutofillTable::GetAutofillProfiles(
    AutofillProfile::Source profile_source,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) const {
  CHECK(profiles);
  profiles->clear();

  sql::Statement s;
  SelectBuilder(db_, s, GetProfileMetadataTable(profile_source), {kGuid});
  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfile(guid, profile_source);
    if (!profile) {
      continue;
    }
    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

std::unique_ptr<AutofillProfile>
AddressAutofillTable::GetAutofillProfileFromLegacyTable(
    const std::string& guid) const {
  sql::Statement s;
  if (!SelectByGuid(db_, s, kAutofillProfilesTable,
                    {kCompanyName, kStreetAddress, kDependentLocality, kCity,
                     kState, kZipcode, kSortingCode, kCountryCode, kUseCount,
                     kUseDate, kDateModified, kLanguageCode, kLabel},
                    guid)) {
    return nullptr;
  }

  auto profile = std::make_unique<AutofillProfile>(
      guid, AutofillProfile::Source::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);

  DCHECK(base::Uuid::ParseCaseInsensitive(profile->guid()).is_valid());

  // Get associated name info using guid.
  AddLegacyAutofillProfileNamesToProfile(db_, profile.get());

  // Get associated email info using guid.
  AddLegacyAutofillProfileEmailsToProfile(db_, profile.get());

  // Get associated phone info using guid.
  AddLegacyAutofillProfilePhonesToProfile(db_, profile.get());

  // The details should be added after the other info to make sure they don't
  // change when we change the names/emails/phones.
  AddLegacyAutofillProfileDetailsFromStatement(s, profile.get());

  // The structured address information should be added after the street_address
  // from the query above was  written because this information is used to
  // detect changes by a legacy client.
  AddLegacyAutofillProfileAddressesToProfile(db_, profile.get());

  // For more-structured profiles, the profile must be finalized to fully
  // populate the name fields.
  profile->FinalizeAfterImport();

  return profile;
}

// TODO(crbug.com/40267335): This function's implementation is very similar to
// `GetAutofillProfiles()`. Simplify somehow.
bool AddressAutofillTable::GetAutofillProfilesFromLegacyTable(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) const {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillProfilesTable, {kGuid});

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfileFromLegacyTable(guid);
    if (!profile) {
      continue;
    }
    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

bool AddressAutofillTable::RemoveAutofillDataModifiedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

  // Remember Autofill profiles in the time range.
  sql::Statement s_profiles_get;
  SelectBetween(
      db_, s_profiles_get,
      GetProfileMetadataTable(AutofillProfile::Source::kLocalOrSyncable),
      {kGuid}, kDateModified, delete_begin_t, delete_end_t);

  profiles->clear();
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfile(guid, AutofillProfile::Source::kLocalOrSyncable);
    if (!profile) {
      return false;
    }
    profiles->push_back(std::move(profile));
  }
  if (!s_profiles_get.Succeeded()) {
    return false;
  }

  // Remove Autofill profiles in the time range.
  for (const std::unique_ptr<AutofillProfile>& profile : *profiles) {
    if (!RemoveAutofillProfile(profile->guid(),
                               AutofillProfile::Source::kLocalOrSyncable)) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion88AddNewNameColumns() {
  for (std::string_view column :
       std::vector<std::string_view>{"honorific_prefix", kFirstLastName,
                                     kConjunctionLastName, kSecondLastName}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileNamesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column : std::vector<std::string_view>{
           "honorific_prefix_status", kFirstNameStatus, kMiddleNameStatus,
           kLastNameStatus, kFirstLastNameStatus, kConjunctionLastNameStatus,
           kSecondLastNameStatus, kFullNameStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db_, kAutofillProfileNamesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion92AddNewPrefixedNameColumn() {
  return AddColumnIfNotExists(db_, kAutofillProfileNamesTable,
                              "full_name_with_honorific_prefix", "VARCHAR") &&
         AddColumnIfNotExists(db_, kAutofillProfileNamesTable,
                              "full_name_with_honorific_prefix_status",
                              "INTEGER DEFAULT 0");
}

bool AddressAutofillTable::MigrateToVersion90AddNewStructuredAddressColumns() {
  if (!db_->DoesTableExist(kAutofillProfileAddressesTable)) {
    InitLegacyProfileAddressesTable();
  }

  for (std::string_view column : {kDependentLocality, kCity, kState, kZipCode,
                                  kSortingCode, kCountryCode}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column :
       {kDependentLocalityStatus, kCityStatus, kStateStatus, kZipCodeStatus,
        kSortingCodeStatus, kCountryCodeStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion91AddMoreStructuredAddressColumns() {
  if (!db_->DoesTableExist(kAutofillProfileAddressesTable)) {
    InitLegacyProfileAddressesTable();
  }

  for (std::string_view column : {kApartmentNumber, kFloor}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column : {kApartmentNumberStatus, kFloorStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion93AddAutofillProfileLabelColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable)) {
    InitLegacyProfileAddressesTable();
  }

  return AddColumnIfNotExists(db_, kAutofillProfilesTable, kLabel, "VARCHAR");
}

bool AddressAutofillTable::
    MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable)) {
    InitLegacyProfileAddressesTable();
  }

  return AddColumnIfNotExists(db_, kAutofillProfilesTable,
                              kDisallowSettingsVisibleUpdates,
                              "INTEGER NOT NULL DEFAULT 0");
}

bool AddressAutofillTable::
    MigrateToVersion99RemoveAutofillProfilesTrashTable() {
  return DropTableIfExists(db_, "autofill_profiles_trash");
}

bool AddressAutofillTable::
    MigrateToVersion100RemoveProfileValidityBitfieldColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kAutofillProfilesTable, "validity_bitfield") &&
         DropColumn(db_, kAutofillProfilesTable,
                    "is_client_validity_states_updated") &&
         transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion102AddAutofillBirthdatesTable() {
  return CreateTable(db_, kAutofillProfileBirthdatesTable,
                     {{kGuid, "VARCHAR"},
                      {kDay, "INTEGER DEFAULT 0"},
                      {kMonth, "INTEGER DEFAULT 0"},
                      {kYear, "INTEGER DEFAULT 0"}});
}

bool AddressAutofillTable::MigrateToVersion107AddContactInfoTables() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kContactInfoTable,
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                      {kLanguageCode, "VARCHAR"},
                      {kLabel, "VARCHAR"}}) &&
         CreateTable(db_, kContactInfoTypeTokensTable,
                     {{kGuid, "VARCHAR"},
                      {kType, "INTEGER"},
                      {kValue, "VARCHAR"},
                      {kVerificationStatus, "INTEGER DEFAULT 0"}},
                     /*composite_primary_key=*/{kGuid, kType}) &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion110AddInitialCreatorIdAndLastModifierId() {
  if (!db_->DoesTableExist(kContactInfoTable)) {
    return false;
  }
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         AddColumnIfNotExists(db_, kContactInfoTable, kInitialCreatorId,
                              "INTEGER DEFAULT 0") &&
         AddColumnIfNotExists(db_, kContactInfoTable, kLastModifierId,
                              "INTEGER DEFAULT 0") &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion113MigrateLocalAddressProfilesToNewTable() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin() ||
      !CreateTableIfNotExists(db_, kLocalAddressesTable,
                              {{kGuid, "VARCHAR PRIMARY KEY"},
                               {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                               {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                               {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                               {kLanguageCode, "VARCHAR"},
                               {kLabel, "VARCHAR"},
                               {kInitialCreatorId, "INTEGER DEFAULT 0"},
                               {kLastModifierId, "INTEGER DEFAULT 0"}}) ||
      !CreateTableIfNotExists(db_, kLocalAddressesTypeTokensTable,
                              {{kGuid, "VARCHAR"},
                               {kType, "INTEGER"},
                               {kValue, "VARCHAR"},
                               {kVerificationStatus, "INTEGER DEFAULT 0"}},
                              /*composite_primary_key=*/{kGuid, kType})) {
    return false;
  }
  bool success = true;
  if (db_->DoesTableExist(kAutofillProfilesTable)) {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    success = GetAutofillProfilesFromLegacyTable(&profiles);
    // Migrate profiles to the new tables. Preserve the modification dates.
    for (const std::unique_ptr<AutofillProfile>& profile : profiles) {
      success = success && AddAutofillProfileToTableVersion113(db_, *profile);
    }
  }
  // Delete all profiles from the legacy tables. The tables are dropped in
  // version 114.
  for (std::string_view deprecated_table :
       {kAutofillProfilesTable, kAutofillProfileAddressesTable,
        kAutofillProfileNamesTable, kAutofillProfileEmailsTable,
        kAutofillProfilePhonesTable, kAutofillProfileBirthdatesTable}) {
    success = success && (!db_->DoesTableExist(deprecated_table) ||
                          Delete(db_, deprecated_table));
  }
  return success && transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion114DropLegacyAddressTables() {
  sql::Transaction transaction(db_);
  bool success = transaction.Begin();
  for (std::string_view deprecated_table :
       {kAutofillProfilesTable, kAutofillProfileAddressesTable,
        kAutofillProfileNamesTable, kAutofillProfileEmailsTable,
        kAutofillProfilePhonesTable, kAutofillProfileBirthdatesTable}) {
    success = success && DropTableIfExists(db_, deprecated_table);
  }
  return success && transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion117AddProfileObservationColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         AddColumn(db_, kContactInfoTypeTokensTable, kObservations, "BLOB") &&
         AddColumn(db_, kLocalAddressesTypeTokensTable, kObservations,
                   "BLOB") &&
         transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion121DropServerAddressTables() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTableIfExists(db_, "server_addresses") &&
         DropTableIfExists(db_, "server_address_metadata") &&
         transaction.Commit();
}

bool AddressAutofillTable::InitLegacyProfileAddressesTable() {
  // The default value of 0 corresponds to the verification status
  // |kNoStatus|.
  return CreateTableIfNotExists(
      db_, kAutofillProfileAddressesTable,
      {{kGuid, "VARCHAR"},
       {kStreetAddress, "VARCHAR"},
       {kStreetName, "VARCHAR"},
       {kDependentStreetName, "VARCHAR"},
       {kHouseNumber, "VARCHAR"},
       {kSubpremise, "VARCHAR"},
       {"premise_name", "VARCHAR"},
       {kStreetAddressStatus, "INTEGER DEFAULT 0"},
       {kStreetNameStatus, "INTEGER DEFAULT 0"},
       {kDependentStreetNameStatus, "INTEGER DEFAULT 0"},
       {kHouseNumberStatus, "INTEGER DEFAULT 0"},
       {kSubpremiseStatus, "INTEGER DEFAULT 0"},
       {"premise_name_status", "INTEGER DEFAULT 0"},
       {kDependentLocality, "VARCHAR"},
       {kCity, "VARCHAR"},
       {kState, "VARCHAR"},
       {kZipCode, "VARCHAR"},
       {kSortingCode, "VARCHAR"},
       {kCountryCode, "VARCHAR"},
       {kDependentLocalityStatus, "INTEGER DEFAULT 0"},
       {kCityStatus, "INTEGER DEFAULT 0"},
       {kStateStatus, "INTEGER DEFAULT 0"},
       {kZipCodeStatus, "INTEGER DEFAULT 0"},
       {kSortingCodeStatus, "INTEGER DEFAULT 0"},
       {kCountryCodeStatus, "INTEGER DEFAULT 0"},
       {kApartmentNumber, "VARCHAR"},
       {kFloor, "VARCHAR"},
       {kApartmentNumberStatus, "INTEGER DEFAULT 0"},
       {kFloorStatus, "INTEGER DEFAULT 0"}});
}

bool AddressAutofillTable::InitProfileMetadataTable(
    AutofillProfile::Source source) {
  return CreateTableIfNotExists(db_, GetProfileMetadataTable(source),
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                                 {kLanguageCode, "VARCHAR"},
                                 {kLabel, "VARCHAR"},
                                 {kInitialCreatorId, "INTEGER DEFAULT 0"},
                                 {kLastModifierId, "INTEGER DEFAULT 0"}});
}

bool AddressAutofillTable::InitProfileTypeTokensTable(
    AutofillProfile::Source source) {
  return CreateTableIfNotExists(db_, GetProfileTypeTokensTable(source),
                                {{kGuid, "VARCHAR"},
                                 {kType, "INTEGER"},
                                 {kValue, "VARCHAR"},
                                 {kVerificationStatus, "INTEGER DEFAULT 0"},
                                 {kObservations, "BLOB"}},
                                /*composite_primary_key=*/{kGuid, kType});
}

}  // namespace autofill
