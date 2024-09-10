// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace autofill {

namespace {

constexpr std::string_view kAddressesTable = "addresses";
constexpr std::string_view kGuid = "guid";
constexpr std::string_view kRecordType = "record_type";
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kUseDate = "use_date";
constexpr std::string_view kUseDate2 = "use_date2";
constexpr std::string_view kUseDate3 = "use_date3";
constexpr std::string_view kDateModified = "date_modified";
constexpr std::string_view kLanguageCode = "language_code";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kInitialCreatorId = "initial_creator_id";
constexpr std::string_view kLastModifierId = "last_modifier_id";

constexpr std::string_view kAddressTypeTokensTable = "address_type_tokens";
// kGuid = "guid"
constexpr std::string_view kType = "type";
constexpr std::string_view kValue = "value";
constexpr std::string_view kVerificationStatus = "verification_status";
constexpr std::string_view kObservations = "observations";

// Before the `kAddressesTable` and `kAddressTypeTokensTable` tables, local and
// account addresses were stored separately.
constexpr std::string_view kContactInfoTable = "contact_info";
constexpr std::string_view kLocalAddressesTable = "local_addresses";
constexpr std::string_view kContactInfoTypeTokensTable =
    "contact_info_type_tokens";
constexpr std::string_view kLocalAddressesTypeTokensTable =
    "local_addresses_type_tokens";

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

// In an older version of the schema, local and account addresses were stored in
// different tables with the same layout. This function was used to get the
// correct table based on the record type. It shouldn't be used anymore, except
// in migration logic.
std::string_view GetLegacyProfileMetadataTable(
    AutofillProfile::RecordType record_type) {
  switch (record_type) {
    case AutofillProfile::RecordType::kLocalOrSyncable:
      return kLocalAddressesTable;
    case AutofillProfile::RecordType::kAccount:
    case AutofillProfile::RecordType::kAccountHome:
    case AutofillProfile::RecordType::kAccountWork:
      return kContactInfoTable;
  }
  NOTREACHED();
}
std::string_view GetLegacyProfileTypeTokensTable(
    AutofillProfile::RecordType record_type) {
  switch (record_type) {
    case AutofillProfile::RecordType::kLocalOrSyncable:
      return kLocalAddressesTypeTokensTable;
    case AutofillProfile::RecordType::kAccount:
    case AutofillProfile::RecordType::kAccountHome:
    case AutofillProfile::RecordType::kAccountWork:
      return kContactInfoTypeTokensTable;
  }
  NOTREACHED();
}

// Insert the `profile`'s metadata into `kAddressesTable`, returning true if the
// write succeeded.
bool AddProfileMetadataToTable(sql::Database* db,
                               const AutofillProfile& profile) {
  sql::Statement s;
  InsertBuilder(db, s, kAddressesTable,
                {kGuid, kRecordType, kUseCount, kUseDate, kUseDate2, kUseDate3,
                 kDateModified, kLanguageCode, kLabel, kInitialCreatorId,
                 kLastModifierId});
  auto bind_optional_time = [&s](int index, std::optional<base::Time> time) {
    if (time) {
      s.BindInt64(index, time->ToTimeT());
    } else {
      s.BindNull(index);
    }
  };
  int index = 0;
  s.BindString(index++, profile.guid());
  s.BindInt(index++, static_cast<int>(profile.record_type()));
  s.BindInt64(index++, profile.use_count());
  s.BindInt64(index++, profile.use_date().ToTimeT());
  if (base::FeatureList::IsEnabled(features::kAutofillTrackMultipleUseDates)) {
    bind_optional_time(index++, profile.use_date(2));
    bind_optional_time(index++, profile.use_date(3));
  } else {
    s.BindNull(index++);
    s.BindNull(index++);
  }
  s.BindInt64(index++, profile.modification_date().ToTimeT());
  s.BindString(index++, profile.language_code());
  s.BindString(index++, profile.profile_label());
  s.BindInt(index++, profile.initial_creator_id());
  s.BindInt(index++, profile.last_modifier_id());
  return s.Run();
}

// Insert the `profile`'s values into `kAddressTypeTokensTable`, returning true
// if the write succeeded.
bool AddProfileTypeTokensToTable(sql::Database* db,
                                 const AutofillProfile& profile) {
  for (FieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    if (!base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel) &&
        type == ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY) {
      continue;
    }
    sql::Statement s;
    InsertBuilder(db, s, kAddressTypeTokensTable,
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
  InsertBuilder(db, s, GetLegacyProfileMetadataTable(profile.record_type()),
                {kGuid, kUseCount, kUseDate, kDateModified, kLanguageCode,
                 kLabel, kInitialCreatorId, kLastModifierId});
  int index = 0;
  s.BindString(index++, profile.guid());
  s.BindInt64(index++, profile.use_count());
  s.BindInt64(index++, profile.use_date().ToTimeT());
  s.BindInt64(index++, profile.modification_date().ToTimeT());
  s.BindString(index++, profile.language_code());
  s.BindString(index++, profile.profile_label());
  s.BindInt(index++, profile.initial_creator_id());
  s.BindInt(index++, profile.last_modifier_id());
  if (!s.Run()) {
    return false;
  }
  // Note that `GetDatabaseStoredTypesOfAutofillProfile()` might change in
  // future versions. Due to the flexible layout of the type tokens table, this
  // is not a problem.
  for (FieldType type : GetDatabaseStoredTypesOfAutofillProfile()) {
    InsertBuilder(db, s, GetLegacyProfileTypeTokensTable(profile.record_type()),
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

// Represents a row in `kAddressTypeTokensTable`.
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

// Reads all rows from `kAddressTypeTokensTable` by `guid` and returns them as a
// result. Returns `std::nullopt` if reading failed.
std::optional<std::vector<FieldTypeData>> ReadProfileTypeTokens(
    const std::string& guid,
    sql::Database* db) {
  sql::Statement s;
  if (!SelectByGuid(db, s, kAddressTypeTokensTable,
                    {kType, kValue, kVerificationStatus, kObservations},
                    guid)) {
    return std::nullopt;
  }
  std::vector<FieldTypeData> field_type_data;
  // As `SelectByGuid()` already calls `s.Step()`, do-while is used here.
  do {
    FieldType type = ToSafeFieldType(s.ColumnInt(0), UNKNOWN_TYPE);
    if (!GetDatabaseStoredTypesOfAutofillProfile().contains(type)) {
      // This is possible in two cases:
      // - The database was tampered with by external means.
      // - The type corresponding to `s.ColumnInt(0)` was deprecated. In this
      //   case, due to the structure of `kAddressTypeTokensTable` , it is not
      //   necessary to add database migration logic or drop a column. Instead,
      //   during the next update, the data will be dropped.
      continue;
    }
    base::span<const uint8_t> observations_data = s.ColumnBlob(3);
    field_type_data.emplace_back(type, s.ColumnString16(1), s.ColumnInt(2),
                                 std::vector<uint8_t>(observations_data.begin(),
                                                      observations_data.end()));
  } while (s.Step());
  return field_type_data;
}

// Reads all rows of `kAddresses` by `guid` and constructs a profile from this
// information. Since `kAddresses` only contains metadata, the resulting profile
// won't have values for and types. For the same reason, because type related
// information is not stored in `kAddresses`, `country_code` is provided as an
// input parameter.
// Returns `std::nullopt` if reading fails.
std::optional<AutofillProfile> GetProfileFromMetadataTable(
    const std::string& guid,
    const AddressCountryCode& country_code,
    sql::Database* db) {
  sql::Statement s;
  if (!SelectByGuid(db, s, kAddressesTable,
                    {kRecordType, kUseCount, kUseDate, kUseDate2, kUseDate3,
                     kDateModified, kLanguageCode, kLabel, kInitialCreatorId,
                     kLastModifierId},
                    guid)) {
    return std::nullopt;
  }

  int index = 0;
  int raw_record_type = s.ColumnInt(index++);
  if (raw_record_type < 0 ||
      raw_record_type >
          static_cast<int>(AutofillProfile::RecordType::kMaxValue)) {
    // Corrupt data read from the disk.
    return std::nullopt;
  }
  AutofillProfile profile(
      guid, static_cast<AutofillProfile::RecordType>(raw_record_type),
      country_code);

  // Populate the `profile` with metadata.
  auto as_optional_time = [&s](size_t index) -> std::optional<base::Time> {
    if (s.GetColumnType(index) == sql::ColumnType::kNull) {
      return std::nullopt;
    }
    return base::Time::FromTimeT(s.ColumnInt64(index));
  };
  profile.set_use_count(s.ColumnInt64(index++));
  profile.set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)), 1);
  if (base::FeatureList::IsEnabled(features::kAutofillTrackMultipleUseDates)) {
    profile.set_use_date(as_optional_time(index++), 2);
    profile.set_use_date(as_optional_time(index++), 3);
  } else {
    index += 2;
  }
  profile.set_modification_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile.set_language_code(s.ColumnString(index++));
  profile.set_profile_label(s.ColumnString(index++));
  profile.set_initial_creator_id(s.ColumnInt(index++));
  profile.set_last_modifier_id(s.ColumnInt(index++));
  return profile;
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
  return InitAddressesTable() && InitAddressTypeTokensTable();
}

bool AddressAutofillTable::MigrateToVersion(int version,
                                            bool* update_compatible_version) {
  if (!db()->is_open()) {
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
    case 132:
      *update_compatible_version = false;
      return MigrateToVersion132AddAdditionalLastUseDateColumns();
    case 134:
      *update_compatible_version = true;
      return MigrateToVersion134UnifyLocalAndAccountAddressStorage();
  }
  return true;
}

bool AddressAutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  sql::Transaction transaction(db());
  return transaction.Begin() && AddProfileMetadataToTable(db(), profile) &&
         AddProfileTypeTokensToTable(db(), profile) && transaction.Commit();
}

bool AddressAutofillTable::UpdateAutofillProfile(
    const AutofillProfile& profile) {
  DCHECK(base::Uuid::ParseCaseInsensitive(profile.guid()).is_valid());

  if (!GetAutofillProfile(profile.guid())) {
    return false;
  }

  // Implementing an update as remove + add has multiple advantages:
  // - Prevents outdated (FieldType, value) pairs from remaining in the
  //   `kAddressTypeTokensTable`, in case field types are removed.
  // - Simpler code.
  // The possible downside is performance. This is not an issue, as updates
  // happen rarely and asynchronously.
  // Note that this doesn't reuse `AddAutofillProfile()` to avoid nested
  // transactions.
  sql::Transaction transaction(db());
  return transaction.Begin() && RemoveAutofillProfile(profile.guid()) &&
         AddProfileMetadataToTable(db(), profile) &&
         AddProfileTypeTokensToTable(db(), profile) && transaction.Commit();
}

bool AddressAutofillTable::RemoveAutofillProfile(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DeleteWhereColumnEq(db(), kAddressesTable, kGuid, guid) &&
         DeleteWhereColumnEq(db(), kAddressTypeTokensTable, kGuid, guid) &&
         transaction.Commit();
}

bool AddressAutofillTable::RemoveAllAutofillProfiles(
    DenseSet<AutofillProfile::RecordType> record_types) {
  sql::Transaction transaction(db());
  std::vector<AutofillProfile> profiles;
  // TODO(crbug.com/40100455): Since the `kAddressTypeTokensTable` doesn't have
  // a `kRecordType` column, it's non-trivial to remove the correct entries from
  // that table. For simplicity, the current implementation fetches all profiles
  // to remove and removes them manually. Rewrite to a DELETE SQL query.
  if (!GetAutofillProfiles(record_types, profiles)) {
    return false;
  }
  return transaction.Begin() &&
         std::ranges::all_of(profiles,
                             [&](const AutofillProfile& p) {
                               return RemoveAutofillProfile(p.guid());
                             }) &&
         transaction.Commit();
}

std::optional<AutofillProfile> AddressAutofillTable::GetAutofillProfile(
    const std::string& guid) const {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  // Constructing a profile requires its record type, stored in the metadata
  // table, and its country, stored in the type tokens tables. For this reason,
  // the logic works as follows:
  // - Reading all rows by `guid` from the type tokens table.
  // - Extracting the country from it.
  // - Reading all rows by `guid` from the metadata table and constructing a
  //   profile using the extracted country information.
  // - Populating the profile with the remaining type token rows.
  std::optional<std::vector<FieldTypeData>> field_type_data =
      ReadProfileTypeTokens(guid, db());
  if (!field_type_data) {
    return std::nullopt;
  }
  auto country = std::ranges::find(*field_type_data, ADDRESS_HOME_COUNTRY,
                                   &FieldTypeData::type);
  std::optional<AutofillProfile> profile = GetProfileFromMetadataTable(
      guid,
      AddressCountryCode(base::UTF16ToUTF8(
          country != field_type_data->end() ? country->value : u"")),
      db());
  if (!profile) {
    return std::nullopt;
  }
  for (const FieldTypeData& data : *field_type_data) {
    profile->SetRawInfoWithVerificationStatusInt(data.type, data.value,
                                                 data.status);
    profile->token_quality().LoadSerializedObservationsForStoredType(
        data.type, data.serialized_data);
  }
  profile->FinalizeAfterImport();
  return profile;
}

bool AddressAutofillTable::GetAutofillProfiles(
    DenseSet<AutofillProfile::RecordType> record_types,
    std::vector<AutofillProfile>& profiles) const {
  profiles.clear();

  sql::Statement s;
  const std::string placeholders =
      base::JoinString(std::vector<std::string>(record_types.size(), "?"), ",");
  SelectBuilder(
      db(), s, kAddressesTable, {kGuid},
      base::StrCat({"WHERE ", kRecordType, " IN (", placeholders, ")"}));
  size_t index = 0;
  for (AutofillProfile::RecordType record_type : record_types) {
    s.BindInt(index++, static_cast<int>(record_type));
  }

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::optional<AutofillProfile> profile = GetAutofillProfile(guid);
    if (!profile) {
      continue;
    }
    profiles.push_back(std::move(*profile));
  }

  return s.Succeeded();
}

std::optional<AutofillProfile>
AddressAutofillTable::GetAutofillProfileFromLegacyTable(
    const std::string& guid) const {
  sql::Statement s;
  if (!SelectByGuid(db(), s, kAutofillProfilesTable,
                    {kCompanyName, kStreetAddress, kDependentLocality, kCity,
                     kState, kZipcode, kSortingCode, kCountryCode, kUseCount,
                     kUseDate, kDateModified, kLanguageCode, kLabel},
                    guid)) {
    return std::nullopt;
  }

  AutofillProfile profile(guid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);

  DCHECK(base::Uuid::ParseCaseInsensitive(profile.guid()).is_valid());

  // Get associated name info using guid.
  AddLegacyAutofillProfileNamesToProfile(db(), &profile);

  // Get associated email info using guid.
  AddLegacyAutofillProfileEmailsToProfile(db(), &profile);

  // Get associated phone info using guid.
  AddLegacyAutofillProfilePhonesToProfile(db(), &profile);

  // The details should be added after the other info to make sure they don't
  // change when we change the names/emails/phones.
  AddLegacyAutofillProfileDetailsFromStatement(s, &profile);

  // The structured address information should be added after the street_address
  // from the query above was  written because this information is used to
  // detect changes by a legacy client.
  AddLegacyAutofillProfileAddressesToProfile(db(), &profile);

  // For more-structured profiles, the profile must be finalized to fully
  // populate the name fields.
  profile.FinalizeAfterImport();

  return profile;
}

// TODO(crbug.com/40267335): This function's implementation is very similar to
// `GetAutofillProfiles()`. Simplify somehow.
bool AddressAutofillTable::GetAutofillProfilesFromLegacyTable(
    std::vector<AutofillProfile>& profiles) const {
  profiles.clear();

  sql::Statement s;
  SelectBuilder(db(), s, kAutofillProfilesTable, {kGuid});

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::optional<AutofillProfile> profile =
        GetAutofillProfileFromLegacyTable(guid);
    if (!profile) {
      continue;
    }
    profiles.push_back(std::move(*profile));
  }

  return s.Succeeded();
}

bool AddressAutofillTable::MigrateToVersion88AddNewNameColumns() {
  for (std::string_view column :
       std::vector<std::string_view>{"honorific_prefix", kFirstLastName,
                                     kConjunctionLastName, kSecondLastName}) {
    if (!AddColumnIfNotExists(db(), kAutofillProfileNamesTable, column,
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
    if (!AddColumnIfNotExists(db(), kAutofillProfileNamesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion92AddNewPrefixedNameColumn() {
  return AddColumnIfNotExists(db(), kAutofillProfileNamesTable,
                              "full_name_with_honorific_prefix", "VARCHAR") &&
         AddColumnIfNotExists(db(), kAutofillProfileNamesTable,
                              "full_name_with_honorific_prefix_status",
                              "INTEGER DEFAULT 0");
}

bool AddressAutofillTable::MigrateToVersion90AddNewStructuredAddressColumns() {
  if (!db()->DoesTableExist(kAutofillProfileAddressesTable)) {
    InitLegacyProfileAddressesTable();
  }

  for (std::string_view column : {kDependentLocality, kCity, kState, kZipCode,
                                  kSortingCode, kCountryCode}) {
    if (!AddColumnIfNotExists(db(), kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column :
       {kDependentLocalityStatus, kCityStatus, kStateStatus, kZipCodeStatus,
        kSortingCodeStatus, kCountryCodeStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db(), kAutofillProfileAddressesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion91AddMoreStructuredAddressColumns() {
  if (!db()->DoesTableExist(kAutofillProfileAddressesTable)) {
    InitLegacyProfileAddressesTable();
  }

  for (std::string_view column : {kApartmentNumber, kFloor}) {
    if (!AddColumnIfNotExists(db(), kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column : {kApartmentNumberStatus, kFloorStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db(), kAutofillProfileAddressesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AddressAutofillTable::MigrateToVersion93AddAutofillProfileLabelColumn() {
  if (!db()->DoesTableExist(kAutofillProfilesTable)) {
    InitLegacyProfileAddressesTable();
  }

  return AddColumnIfNotExists(db(), kAutofillProfilesTable, kLabel, "VARCHAR");
}

bool AddressAutofillTable::
    MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn() {
  if (!db()->DoesTableExist(kAutofillProfilesTable)) {
    InitLegacyProfileAddressesTable();
  }

  return AddColumnIfNotExists(db(), kAutofillProfilesTable,
                              kDisallowSettingsVisibleUpdates,
                              "INTEGER NOT NULL DEFAULT 0");
}

bool AddressAutofillTable::
    MigrateToVersion99RemoveAutofillProfilesTrashTable() {
  return DropTableIfExists(db(), "autofill_profiles_trash");
}

bool AddressAutofillTable::
    MigrateToVersion100RemoveProfileValidityBitfieldColumn() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DropColumn(db(), kAutofillProfilesTable, "validity_bitfield") &&
         DropColumn(db(), kAutofillProfilesTable,
                    "is_client_validity_states_updated") &&
         transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion102AddAutofillBirthdatesTable() {
  return CreateTable(db(), kAutofillProfileBirthdatesTable,
                     {{kGuid, "VARCHAR"},
                      {kDay, "INTEGER DEFAULT 0"},
                      {kMonth, "INTEGER DEFAULT 0"},
                      {kYear, "INTEGER DEFAULT 0"}});
}

bool AddressAutofillTable::MigrateToVersion107AddContactInfoTables() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         CreateTable(db(), kContactInfoTable,
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                      {kLanguageCode, "VARCHAR"},
                      {kLabel, "VARCHAR"}}) &&
         CreateTable(db(), kContactInfoTypeTokensTable,
                     {{kGuid, "VARCHAR"},
                      {kType, "INTEGER"},
                      {kValue, "VARCHAR"},
                      {kVerificationStatus, "INTEGER DEFAULT 0"}},
                     /*composite_primary_key=*/{kGuid, kType}) &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion110AddInitialCreatorIdAndLastModifierId() {
  if (!db()->DoesTableExist(kContactInfoTable)) {
    return false;
  }
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         AddColumnIfNotExists(db(), kContactInfoTable, kInitialCreatorId,
                              "INTEGER DEFAULT 0") &&
         AddColumnIfNotExists(db(), kContactInfoTable, kLastModifierId,
                              "INTEGER DEFAULT 0") &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion113MigrateLocalAddressProfilesToNewTable() {
  sql::Transaction transaction(db());
  if (!transaction.Begin() ||
      !CreateTableIfNotExists(db(), kLocalAddressesTable,
                              {{kGuid, "VARCHAR PRIMARY KEY"},
                               {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                               {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                               {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                               {kLanguageCode, "VARCHAR"},
                               {kLabel, "VARCHAR"},
                               {kInitialCreatorId, "INTEGER DEFAULT 0"},
                               {kLastModifierId, "INTEGER DEFAULT 0"}}) ||
      !CreateTableIfNotExists(db(), kLocalAddressesTypeTokensTable,
                              {{kGuid, "VARCHAR"},
                               {kType, "INTEGER"},
                               {kValue, "VARCHAR"},
                               {kVerificationStatus, "INTEGER DEFAULT 0"}},
                              /*composite_primary_key=*/{kGuid, kType})) {
    return false;
  }
  bool success = true;
  if (db()->DoesTableExist(kAutofillProfilesTable)) {
    std::vector<AutofillProfile> profiles;
    success = GetAutofillProfilesFromLegacyTable(profiles);
    // Migrate profiles to the new tables.
    for (const AutofillProfile& profile : profiles) {
      success = success && AddAutofillProfileToTableVersion113(db(), profile);
    }
  }
  // Delete all profiles from the legacy tables. The tables are dropped in
  // version 114.
  for (std::string_view deprecated_table :
       {kAutofillProfilesTable, kAutofillProfileAddressesTable,
        kAutofillProfileNamesTable, kAutofillProfileEmailsTable,
        kAutofillProfilePhonesTable, kAutofillProfileBirthdatesTable}) {
    success = success && (!db()->DoesTableExist(deprecated_table) ||
                          Delete(db(), deprecated_table));
  }
  return success && transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion114DropLegacyAddressTables() {
  sql::Transaction transaction(db());
  bool success = transaction.Begin();
  for (std::string_view deprecated_table :
       {kAutofillProfilesTable, kAutofillProfileAddressesTable,
        kAutofillProfileNamesTable, kAutofillProfileEmailsTable,
        kAutofillProfilePhonesTable, kAutofillProfileBirthdatesTable}) {
    success = success && DropTableIfExists(db(), deprecated_table);
  }
  return success && transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion117AddProfileObservationColumn() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         AddColumn(db(), kContactInfoTypeTokensTable, kObservations, "BLOB") &&
         AddColumn(db(), kLocalAddressesTypeTokensTable, kObservations,
                   "BLOB") &&
         transaction.Commit();
}

bool AddressAutofillTable::MigrateToVersion121DropServerAddressTables() {
  sql::Transaction transaction(db());
  return transaction.Begin() && DropTableIfExists(db(), "server_addresses") &&
         DropTableIfExists(db(), "server_address_metadata") &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion132AddAdditionalLastUseDateColumns() {
  auto migrate_table = [&](AutofillProfile::RecordType record_type) {
    std::string_view table = GetLegacyProfileMetadataTable(record_type);
    return AddColumn(db(), table, kUseDate2, "INTEGER") &&
           AddColumn(db(), table, kUseDate3, "INTEGER");
  };
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         migrate_table(AutofillProfile::RecordType::kLocalOrSyncable) &&
         migrate_table(AutofillProfile::RecordType::kAccount) &&
         transaction.Commit();
}

bool AddressAutofillTable::
    MigrateToVersion134UnifyLocalAndAccountAddressStorage() {
  // In version 133, local and account profiles were stored in two separate sets
  // of tables with the same structure (named `GetLegacyProfileMetadataTable()`
  // and `GetLegacyProfileTypeTokensTable()`). This migration logic merges the
  // two tables into `kAddressesTable` and `kAddressTypeTokensTable`. In order
  // to be able to distinguish profiles, a `kRecordType` column is added to the
  // metadata table. The migration logic works by:
  // - Adding a `kRecordType` column to the existing metadata tables and setting
  //   a value for the existing rows.
  // - Renaming the account address tables to the new names.
  // - Bulk inserting local addresses into the (renamed) new tables.
  // - Dropping the local address tables.
  sql::Transaction transaction(db());
  // Adds a `kRecordType` column to `GetLegacyProfileMetadataTable(record_type)`
  // and sets a value for existing rows.
  auto migrate_metadata_table = [&](AutofillProfile::RecordType record_type) {
    std::string_view table = GetLegacyProfileMetadataTable(record_type);
    if (!AddColumn(db(), table, kRecordType, "INTEGER")) {
      return false;
    }
    sql::Statement update_stmt;
    UpdateBuilder(db(), update_stmt, table, {kRecordType});
    update_stmt.BindInt(0, static_cast<int>(record_type));
    return update_stmt.Run();
  };
  // Bulk inserts the data from `from` into `to` and drops `from`.
  auto merge_into = [&](std::string_view from, std::string_view to) {
    return db()->Execute(
               base::StrCat({"INSERT INTO ", to, " SELECT * FROM ", from})) &&
           DropTableIfExists(db(), from);
  };
  return transaction.Begin() &&
         migrate_metadata_table(AutofillProfile::RecordType::kAccount) &&
         migrate_metadata_table(
             AutofillProfile::RecordType::kLocalOrSyncable) &&
         RenameTable(db(),
                     GetLegacyProfileMetadataTable(
                         AutofillProfile::RecordType::kAccount),
                     kAddressesTable) &&
         RenameTable(db(),
                     GetLegacyProfileTypeTokensTable(
                         AutofillProfile::RecordType::kAccount),
                     kAddressTypeTokensTable) &&
         merge_into(GetLegacyProfileMetadataTable(
                        AutofillProfile::RecordType::kLocalOrSyncable),
                    kAddressesTable) &&
         merge_into(GetLegacyProfileTypeTokensTable(
                        AutofillProfile::RecordType::kLocalOrSyncable),
                    kAddressTypeTokensTable) &&
         transaction.Commit();
}

bool AddressAutofillTable::InitLegacyProfileAddressesTable() {
  // The default value of 0 corresponds to the verification status
  // |kNoStatus|.
  return CreateTableIfNotExists(
      db(), kAutofillProfileAddressesTable,
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

bool AddressAutofillTable::InitAddressesTable() {
  return CreateTableIfNotExists(db(), kAddressesTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                                 {kLanguageCode, "VARCHAR"},
                                 {kLabel, "VARCHAR"},
                                 {kInitialCreatorId, "INTEGER DEFAULT 0"},
                                 {kLastModifierId, "INTEGER DEFAULT 0"},
                                 // The second and third last use date are null
                                 // if the profile wasn't used often enough.
                                 {kUseDate2, "INTEGER"},
                                 {kUseDate3, "INTEGER"},
                                 {kRecordType, "INTEGER"}});
}

bool AddressAutofillTable::InitAddressTypeTokensTable() {
  return CreateTableIfNotExists(db(), kAddressTypeTokensTable,
                                {{kGuid, "VARCHAR"},
                                 {kType, "INTEGER"},
                                 {kValue, "VARCHAR"},
                                 {kVerificationStatus, "INTEGER DEFAULT 0"},
                                 {kObservations, "BLOB"}},
                                /*composite_primary_key=*/{kGuid, kType});
}

}  // namespace autofill
