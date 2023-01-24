// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table.h"

#include <stdint.h>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor_factory.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

namespace {

constexpr base::StringPiece kAutofillTable = "autofill";
constexpr base::StringPiece kName = "name";
constexpr base::StringPiece kValue = "value";
constexpr base::StringPiece kValueLower = "value_lower";
constexpr base::StringPiece kDateCreated = "date_created";
constexpr base::StringPiece kDateLastUsed = "date_last_used";
constexpr base::StringPiece kCount = "count";

constexpr base::StringPiece kAutofillProfilesTable = "autofill_profiles";
constexpr base::StringPiece kGuid = "guid";
constexpr base::StringPiece kLabel = "label";
constexpr base::StringPiece kCompanyName = "company_name";
constexpr base::StringPiece kStreetAddress = "street_address";
constexpr base::StringPiece kDependentLocality = "dependent_locality";
constexpr base::StringPiece kCity = "city";
constexpr base::StringPiece kState = "state";
constexpr base::StringPiece kZipcode = "zipcode";
constexpr base::StringPiece kSortingCode = "sorting_code";
constexpr base::StringPiece kCountryCode = "country_code";
constexpr base::StringPiece kUseCount = "use_count";
constexpr base::StringPiece kUseDate = "use_date";
constexpr base::StringPiece kDateModified = "date_modified";
constexpr base::StringPiece kOrigin = "origin";
constexpr base::StringPiece kLanguageCode = "language_code";
constexpr base::StringPiece kDisallowSettingsVisibleUpdates =
    "disallow_settings_visible_updates";

constexpr base::StringPiece kAutofillProfileAddressesTable =
    "autofill_profile_addresses";
// kGuid = "guid"
// kStreetAddress = "street_address"
constexpr base::StringPiece kStreetName = "street_name";
constexpr base::StringPiece kDependentStreetName = "dependent_street_name";
constexpr base::StringPiece kHouseNumber = "house_number";
constexpr base::StringPiece kSubpremise = "subpremise";
// kDependentLocality = "dependent_locality"
// kCity = "city"
// kState = "state"
constexpr base::StringPiece kZipCode = "zip_code";
// kCountryCode = "country_code"
// kSortingCode = "sorting_code"
constexpr base::StringPiece kPremiseName = "premise_name";
constexpr base::StringPiece kApartmentNumber = "apartment_number";
constexpr base::StringPiece kFloor = "floor";
constexpr base::StringPiece kStreetAddressStatus = "street_address_status";
constexpr base::StringPiece kStreetNameStatus = "street_name_status";
constexpr base::StringPiece kDependentStreetNameStatus =
    "dependent_street_name_status";
constexpr base::StringPiece kHouseNumberStatus = "house_number_status";
constexpr base::StringPiece kSubpremiseStatus = "subpremise_status";
constexpr base::StringPiece kPremiseNameStatus = "premise_name_status";
constexpr base::StringPiece kDependentLocalityStatus =
    "dependent_locality_status";
constexpr base::StringPiece kCityStatus = "city_status";
constexpr base::StringPiece kStateStatus = "state_status";
constexpr base::StringPiece kZipCodeStatus = "zip_code_status";
constexpr base::StringPiece kCountryCodeStatus = "country_code_status";
constexpr base::StringPiece kSortingCodeStatus = "sorting_code_status";
constexpr base::StringPiece kApartmentNumberStatus = "apartment_number_status";
constexpr base::StringPiece kFloorStatus = "floor_status";

constexpr base::StringPiece kAutofillProfileNamesTable =
    "autofill_profile_names";
// kGuid = "guid"
constexpr base::StringPiece kHonorificPrefix = "honorific_prefix";
constexpr base::StringPiece kFirstName = "first_name";
constexpr base::StringPiece kMiddleName = "middle_name";
constexpr base::StringPiece kLastName = "last_name";
constexpr base::StringPiece kFirstLastName = "first_last_name";
constexpr base::StringPiece kConjunctionLastName = "conjunction_last_name";
constexpr base::StringPiece kSecondLastName = "second_last_name";
constexpr base::StringPiece kFullName = "full_name";
constexpr base::StringPiece kFullNameWithHonorificPrefix =
    "full_name_with_honorific_prefix";
constexpr base::StringPiece kHonorificPrefixStatus = "honorific_prefix_status";
constexpr base::StringPiece kFirstNameStatus = "first_name_status";
constexpr base::StringPiece kMiddleNameStatus = "middle_name_status";
constexpr base::StringPiece kLastNameStatus = "last_name_status";
constexpr base::StringPiece kFirstLastNameStatus = "first_last_name_status";
constexpr base::StringPiece kConjunctionLastNameStatus =
    "conjunction_last_name_status";
constexpr base::StringPiece kSecondLastNameStatus = "second_last_name_status";
constexpr base::StringPiece kFullNameStatus = "full_name_status";
constexpr base::StringPiece kFullNameWithHonorificPrefixStatus =
    "full_name_with_honorific_prefix_status";

constexpr base::StringPiece kAutofillProfileEmailsTable =
    "autofill_profile_emails";
// kGuid = "guid"
constexpr base::StringPiece kEmail = "email";

constexpr base::StringPiece kAutofillProfilePhonesTable =
    "autofill_profile_phones";
// kGuid = "guid"
constexpr base::StringPiece kNumber = "number";

constexpr base::StringPiece kAutofillProfileBirthdatesTable =
    "autofill_profile_birthdates";
// kGuid = "guid"
constexpr base::StringPiece kDay = "day";
constexpr base::StringPiece kMonth = "month";
constexpr base::StringPiece kYear = "year";

constexpr base::StringPiece kCreditCardsTable = "credit_cards";
// kGuid = "guid"
constexpr base::StringPiece kNameOnCard = "name_on_card";
constexpr base::StringPiece kExpirationMonth = "expiration_month";
constexpr base::StringPiece kExpirationYear = "expiration_year";
constexpr base::StringPiece kCardNumberEncrypted = "card_number_encrypted";
// kUseCount = "use_count"
// kUseDate = "use_date"
// kDateModified = "date_modified"
// kOrigin = "origin"
constexpr base::StringPiece kBillingAddressId = "billing_address_id";
constexpr base::StringPiece kNickname = "nickname";

constexpr base::StringPiece kMaskedCreditCardsTable = "masked_credit_cards";
constexpr base::StringPiece kId = "id";
constexpr base::StringPiece kStatus = "status";
// kNameOnCard = "name_on_card"
constexpr base::StringPiece kNetwork = "network";
constexpr base::StringPiece kLastFour = "last_four";
constexpr base::StringPiece kExpMonth = "exp_month";
constexpr base::StringPiece kExpYear = "exp_year";
constexpr base::StringPiece kBankName = "bank_name";
// kNickname = "nickname"
constexpr base::StringPiece kCardIssuer = "card_issuer";
constexpr base::StringPiece kCardIssuerId = "card_issuer_id";
constexpr base::StringPiece kInstrumentId = "instrument_id";
constexpr base::StringPiece kVirtualCardEnrollmentState =
    "virtual_card_enrollment_state";
constexpr base::StringPiece kCardArtUrl = "card_art_url";
constexpr base::StringPiece kProductDescription = "product_description";

constexpr base::StringPiece kUnmaskedCreditCardsTable = "unmasked_credit_cards";
// kId = "id"
// kCardNumberEncrypted = "card_number_encrypted"
constexpr base::StringPiece kUnmaskDate = "unmask_date";

constexpr base::StringPiece kServerCardCloudTokenDataTable =
    "server_card_cloud_token_data";
// kId = "id"
constexpr base::StringPiece kSuffix = "suffix";
// kExpMonth = "exp_month"
// kExpYear = "exp_year"
// kCardArtUrl = "card_art_url"
constexpr base::StringPiece kInstrumentToken = "instrument_token";

constexpr base::StringPiece kServerCardMetadataTable = "server_card_metadata";
// kId = "id"
// kUseCount = "use_count"
// kUseDate = "use_date"
// kBillingAddressId = "billing_address_id"

constexpr base::StringPiece kIBANsTable = "ibans";
// kGuid = "guid"
// kUseCount = "use_count"
// kUseDate = "use_date"
// kValue = "value"
// kNickname = "nickname"

constexpr base::StringPiece kServerAddressesTable = "server_addresses";
// kId = "id"
constexpr base::StringPiece kRecipientName = "recipient_name";
// kCompanyName = "company_name"
// kStreetAddress = "street_address"
constexpr base::StringPiece kAddress1 = "address_1";
constexpr base::StringPiece kAddress2 = "address_2";
constexpr base::StringPiece kAddress3 = "address_3";
constexpr base::StringPiece kAddress4 = "address_4";
constexpr base::StringPiece kPostalCode = "postal_code";
// kSortingCode = "sorting_code"
// kCountryCode = "country_code"
// kLanguageCode = "language_code"
constexpr base::StringPiece kPhoneNumber = "phone_number";

constexpr base::StringPiece kServerAddressMetadataTable =
    "server_address_metadata";
// kId = "id"
// kUseCount = "use_count"
// kUseDate = "use_date"
constexpr base::StringPiece kHasConverted = "has_converted";

constexpr base::StringPiece kAutofillSyncMetadataTable =
    "autofill_sync_metadata";
constexpr base::StringPiece kModelType = "model_type";
constexpr base::StringPiece kStorageKey = "storage_key";
// kValue = "value"

constexpr base::StringPiece kAutofillModelTypeStateTable =
    "autofill_model_type_state";
// kModelType = "model_type"
// kValue = "value"

constexpr base::StringPiece kPaymentsCustomerDataTable =
    "payments_customer_data";
constexpr base::StringPiece kCustomerId = "customer_id";

constexpr base::StringPiece kPaymentsUpiVpaTable = "payments_upi_vpa";
constexpr base::StringPiece kVpa = "vpa";

constexpr base::StringPiece kOfferDataTable = "offer_data";
constexpr base::StringPiece kOfferId = "offer_id";
constexpr base::StringPiece kOfferRewardAmount = "offer_reward_amount";
constexpr base::StringPiece kExpiry = "expiry";
constexpr base::StringPiece kOfferDetailsUrl = "offer_details_url";
constexpr base::StringPiece kPromoCode = "promo_code";
constexpr base::StringPiece kValuePropText = "value_prop_text";
constexpr base::StringPiece kSeeDetailsText = "see_details_text";
constexpr base::StringPiece kUsageInstructionsText = "usage_instructions_text";

constexpr base::StringPiece kOfferEligibleInstrumentTable =
    "offer_eligible_instrument";
// kOfferId = "offer_id"
// kInstrumentId = "instrument_id"

constexpr base::StringPiece kOfferMerchantDomainTable = "offer_merchant_domain";
// kOfferId = "offer_id"
constexpr base::StringPiece kMerchantDomain = "merchant_domain";

constexpr base::StringPiece kContactInfoTable = "contact_info";
// kGuid = "guid"
// kUseCount = "use_count"
// kUseDate = "use_date"
// kDateModified = "date_modified"
// kLanguageCode = "language_code"
// kLabel = "label"
constexpr base::StringPiece kInitialCreatorId = "initial_creator_id";
constexpr base::StringPiece kLastModifierId = "last_modifier_id";

constexpr base::StringPiece kContactInfoTypeTokensTable =
    "contact_info_type_tokens";
// kGuid = "guid"
constexpr base::StringPiece kType = "type";
// kValue = "value"
constexpr base::StringPiece kVerificationStatus = "verification_status";

constexpr base::StringPiece kVirtualCardUsageDataTable =
    "virtual_card_usage_data";
// kId = "id"
// kInstrumentId = "instrument_id"
// kMerchantDomain = "merchant_domain"
// kLastFour = "last_four"

// Helper functions to construct SQL statements from string constants.
// - Functions with names corresponding to SQL keywords execute the statement
//   directly and return if it was successful.
// - Builder functions only assign the statement, which enables binding
//   values to placeholders before running it.

// Executes a CREATE TABLE statement on `db` which the provided
// `table_name`. The columns are described in `column_names_and_types` as
// pairs of (name, type), where type can include modifiers such as NOT NULL.
// By specifying `compositive_primary_key`, a PRIMARY KEY (col1, col2, ..)
// clause is generated.
// Returns true if successful.
bool CreateTable(
    sql::Database* db,
    base::StringPiece table_name,
    std::initializer_list<std::pair<base::StringPiece, base::StringPiece>>
        column_names_and_types,
    std::initializer_list<base::StringPiece> composite_primary_key = {}) {
  DCHECK(composite_primary_key.size() == 0 ||
         composite_primary_key.size() >= 2);

  std::vector<std::string> combined_names_and_types;
  combined_names_and_types.reserve(column_names_and_types.size());
  for (const auto& [name, type] : column_names_and_types)
    combined_names_and_types.push_back(base::StrCat({name, " ", type}));

  auto primary_key_clause =
      composite_primary_key.size() == 0
          ? ""
          : base::StrCat({", PRIMARY KEY (",
                          base::JoinString(composite_primary_key, ", "), ")"});

  return db->Execute(
      base::StrCat({"CREATE TABLE ", table_name, " (",
                    base::JoinString(combined_names_and_types, ", "),
                    primary_key_clause, ")"})
          .c_str());
}

// Wrapper around `CreateTable()` that condition the creation on the
// `table_name` not existing.
// Returns true if the table now exists.
bool CreateTableIfNotExists(
    sql::Database* db,
    base::StringPiece table_name,
    std::initializer_list<std::pair<base::StringPiece, base::StringPiece>>
        column_names_and_types,
    std::initializer_list<base::StringPiece> composite_primary_key = {}) {
  return db->DoesTableExist(table_name) ||
         CreateTable(db, table_name, column_names_and_types,
                     composite_primary_key);
}

// Creates and index on `table_name` for the provided `columns`.
// The index is named after the table and columns, separated by '_'.
// Returns true if successful.
bool CreateIndex(sql::Database* db,
                 base::StringPiece table_name,
                 std::initializer_list<base::StringPiece> columns) {
  auto index_name =
      base::StrCat({table_name, "_", base::JoinString(columns, "_")});
  return db->Execute(
      base::StrCat({"CREATE INDEX ", index_name, " ON ", table_name, "(",
                    base::JoinString(columns, ", "), ")"})
          .c_str());
}

// Initializes `statement` with INSERT INTO `table_name`, with placeholders for
// all `column_names`.
// By setting `or_replace`, INSERT OR REPLACE INTO is used instead.
void InsertBuilder(sql::Database* db,
                   sql::Statement& statement,
                   base::StringPiece table_name,
                   std::initializer_list<base::StringPiece> column_names,
                   bool or_replace = false) {
  auto insert_or_replace =
      base::StrCat({"INSERT ", or_replace ? "OR REPLACE " : ""});
  auto placeholders = base::JoinString(
      std::vector<std::string>(column_names.size(), "?"), ", ");
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({insert_or_replace, "INTO ", table_name, " (",
                    base::JoinString(column_names, ", "), ") VALUES (",
                    placeholders, ")"})
          .c_str()));
}

// Renames the table `from` into `to` and returns true if successful.
bool RenameTable(sql::Database* db,
                 base::StringPiece from,
                 base::StringPiece to) {
  return db->Execute(
      base::StrCat({"ALTER TABLE ", from, " RENAME TO ", to}).c_str());
}

// Wrapper around `sql::Database::DoesColumnExist()`, because that function
// only accepts const char* parameters.
bool DoesColumnExist(sql::Database* db,
                     base::StringPiece table_name,
                     base::StringPiece column_name) {
  return db->DoesColumnExist(std::string(table_name).c_str(),
                             std::string(column_name).c_str());
}

// Adds a column named `column_name` of `type` to `table_name` and returns true
// if successful.
bool AddColumn(sql::Database* db,
               base::StringPiece table_name,
               base::StringPiece column_name,
               base::StringPiece type) {
  return db->Execute(base::StrCat({"ALTER TABLE ", table_name, " ADD COLUMN ",
                                   column_name, " ", type})
                         .c_str());
}

// Like `AddColumn()`, but conditioned on `column` not existing in `table_name`.
// Returns true if the column is now part of the table
bool AddColumnIfNotExists(sql::Database* db,
                          base::StringPiece table_name,
                          base::StringPiece column_name,
                          base::StringPiece type) {
  return DoesColumnExist(db, table_name, column_name) ||
         AddColumn(db, table_name, column_name, type);
}

// Drops `table_name` and returns true if successful.
bool DropTable(sql::Database* db, base::StringPiece table_name) {
  return db->Execute(base::StrCat({"DROP TABLE ", table_name}).c_str());
}

// Initializes `statement` with DELETE FROM `table_name`. A WHERE clause
// can optionally be specified in `where_clause`.
void DeleteBuilder(sql::Database* db,
                   sql::Statement& statement,
                   base::StringPiece table_name,
                   base::StringPiece where_clause = "") {
  auto where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", table_name, where}).c_str()));
}

// Like `DeleteBuilder()`, but runs the statement and returns true if it was
// successful.
bool Delete(sql::Database* db,
            base::StringPiece table_name,
            base::StringPiece where_clause = "") {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, where_clause);
  return statement.Run();
}

// Wrapper around `DeleteBuilder()`, which initializes the where clause as
// `column` = `value`.
// Runs the statement and returns true if it was successful.
bool DeleteWhereColumnEq(sql::Database* db,
                         base::StringPiece table_name,
                         base::StringPiece column,
                         base::StringPiece value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindString(0, value);
  return statement.Run();
}

// Initializes `statement` with UPDATE `table_name` SET `column_names` = ?, with
// a placeholder for every `column_names`. A WHERE clause can optionally be
// specified in `where_clause`.
void UpdateBuilder(sql::Database* db,
                   sql::Statement& statement,
                   base::StringPiece table_name,
                   std::initializer_list<base::StringPiece> column_names,
                   base::StringPiece where_clause = "") {
  auto columns_with_placeholders =
      base::JoinString(column_names, " = ?, ") + " = ?";
  auto where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(
      db->GetUniqueStatement(base::StrCat({"UPDATE ", table_name, " SET ",
                                           columns_with_placeholders, where})
                                 .c_str()));
}

// Initializes `statement` with SELECT `columns` FROM `table_name` and
// optionally further `modifiers`, such as WHERE, ORDER BY, etc.
void SelectBuilder(sql::Database* db,
                   sql::Statement& statement,
                   base::StringPiece table_name,
                   std::initializer_list<base::StringPiece> columns,
                   base::StringPiece modifiers = "") {
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"SELECT ", base::JoinString(columns, ", "), " FROM ",
                    table_name, " ", modifiers})
          .c_str()));
}

// Wrapper around `SelectBuilder()` that restricts the it to the provided
// `guid`. Returns `statement.is_valid() && statement.Step()`.
bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  base::StringPiece table_name,
                  std::initializer_list<base::StringPiece> columns,
                  base::StringPiece guid) {
  SelectBuilder(db, statement, table_name, columns, "WHERE guid=?");
  statement.BindString(0, guid);
  return statement.is_valid() && statement.Step();
}

// Wrapper around `SelectBuilder()` that restricts it to the half-open interval
// [low, high[ of `column_between`.
void SelectBetween(sql::Database* db,
                   sql::Statement& statement,
                   base::StringPiece table_name,
                   std::initializer_list<base::StringPiece> columns,
                   base::StringPiece column_between,
                   int64_t low,
                   int64_t high) {
  auto between_selector = base::StrCat(
      {"WHERE ", column_between, " >= ? AND ", column_between, " < ?"});
  SelectBuilder(db, statement, table_name, columns, between_selector);
  statement.BindInt64(0, low);
  statement.BindInt64(1, high);
}

// Helper struct for AutofillTable::RemoveFormElementsAddedBetween().
// Contains all the necessary fields to update a row in the 'autofill' table.
struct AutofillUpdate {
  std::u16string name;
  std::u16string value;
  time_t date_created;
  time_t date_last_used;
  int count;
};

// Truncates `data` to the maximum length that can be stored in a column of the
// Autofill database. Shorter strings are left as-is.
std::u16string Truncate(const std::u16string& data) {
  return data.substr(0, AutofillTable::kMaxDataLength);
}

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    const base::Time& modification_date,
                                    sql::Statement* s) {
  DCHECK(base::IsValidGUID(profile.guid()));
  int index = 0;
  s->BindString(index++, profile.guid());

  for (ServerFieldType type :
       {COMPANY_NAME, ADDRESS_HOME_STREET_ADDRESS,
        ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
        ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE, ADDRESS_HOME_COUNTRY}) {
    s->BindString16(index++, Truncate(profile.GetRawInfo(type)));
  }
  s->BindInt64(index++, profile.use_count());
  s->BindInt64(index++, profile.use_date().ToTimeT());
  s->BindInt64(index++, modification_date.ToTimeT());
  s->BindString(index++, profile.origin());
  s->BindString(index++, profile.language_code());
  s->BindString(index++, profile.profile_label());
  s->BindBool(index++, profile.disallow_settings_visible_updates());
}

void AddAutofillProfileDetailsFromStatement(sql::Statement& s,
                                            AutofillProfile* profile) {
  int index = 1;  // 0 is for the origin.
  for (ServerFieldType type :
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
  profile->set_disallow_settings_visible_updates(s.ColumnBool(index++));
}

void BindEncryptedCardToColumn(sql::Statement* s,
                               int column_index,
                               const std::u16string& number,
                               const AutofillTableEncryptor& encryptor) {
  std::string encrypted_data;
  encryptor.EncryptString16(number, &encrypted_data);
  s->BindBlob(column_index, encrypted_data);
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               const base::Time& modification_date,
                               sql::Statement* s,
                               const AutofillTableEncryptor& encryptor) {
  DCHECK(base::IsValidGUID(credit_card.guid()));
  int index = 0;
  s->BindString(index++, credit_card.guid());

  for (ServerFieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                               CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    s->BindString16(index++, Truncate(credit_card.GetRawInfo(type)));
  }
  BindEncryptedCardToColumn(
      s, index++, credit_card.GetRawInfo(CREDIT_CARD_NUMBER), encryptor);

  s->BindInt64(index++, credit_card.use_count());
  s->BindInt64(index++, credit_card.use_date().ToTimeT());
  s->BindInt64(index++, modification_date.ToTimeT());
  s->BindString(index++, credit_card.origin());
  s->BindString(index++, credit_card.billing_address_id());
  s->BindString16(index++, credit_card.nickname());
}

void BindIBANToStatement(const IBAN& iban,
                         sql::Statement* s,
                         const AutofillTableEncryptor& encryptor) {
  DCHECK(base::IsValidGUID(iban.guid()));
  int index = 0;
  s->BindString(index++, iban.guid());

  s->BindInt64(index++, iban.use_count());
  s->BindInt64(index++, iban.use_date().ToTimeT());

  s->BindString16(index++, iban.value());
  s->BindString16(index++, iban.nickname());
}

std::u16string UnencryptedCardFromColumn(
    sql::Statement& s,
    int column_index,
    const AutofillTableEncryptor& encryptor) {
  std::u16string credit_card_number;
  std::string encrypted_number;
  s.ColumnBlobAsString(column_index, &encrypted_number);
  if (!encrypted_number.empty())
    encryptor.DecryptString16(encrypted_number, &credit_card_number);
  return credit_card_number;
}

std::unique_ptr<CreditCard> CreditCardFromStatement(
    sql::Statement& s,
    const AutofillTableEncryptor& encryptor) {
  auto credit_card = std::make_unique<CreditCard>();

  int index = 0;
  credit_card->set_guid(s.ColumnString(index++));
  DCHECK(base::IsValidGUID(credit_card->guid()));

  for (ServerFieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                               CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    credit_card->SetRawInfo(type, s.ColumnString16(index++));
  }
  credit_card->SetRawInfo(CREDIT_CARD_NUMBER,
                          UnencryptedCardFromColumn(s, index++, encryptor));
  credit_card->set_use_count(s.ColumnInt64(index++));
  credit_card->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  credit_card->set_modification_date(
      base::Time::FromTimeT(s.ColumnInt64(index++)));
  credit_card->set_origin(s.ColumnString(index++));
  credit_card->set_billing_address_id(s.ColumnString(index++));
  credit_card->SetNickname(s.ColumnString16(index++));
  return credit_card;
}

std::unique_ptr<IBAN> IBANFromStatement(
    sql::Statement& s,
    const AutofillTableEncryptor& encryptor) {
  auto iban = std::make_unique<IBAN>();

  int index = 0;
  iban->set_guid(s.ColumnString(index++));
  DCHECK(base::IsValidGUID(iban->guid()));
  iban->set_use_count(s.ColumnInt64(index++));
  iban->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));

  iban->SetRawInfo(IBAN_VALUE, s.ColumnString16(index++));
  iban->set_nickname(s.ColumnString16(index++));
  return iban;
}

bool AddAutofillProfileNames(const AutofillProfile& profile,
                             sql::Database* db) {
  sql::Statement s;
  InsertBuilder(
      db, s, kAutofillProfileNamesTable,
      {kGuid, kHonorificPrefix, kHonorificPrefixStatus, kFirstName,
       kFirstNameStatus, kMiddleName, kMiddleNameStatus, kFirstLastName,
       kFirstLastNameStatus, kConjunctionLastName, kConjunctionLastNameStatus,
       kSecondLastName, kSecondLastNameStatus, kLastName, kLastNameStatus,
       kFullName, kFullNameStatus, kFullNameWithHonorificPrefix,
       kFullNameWithHonorificPrefixStatus});
  s.BindString(0, profile.guid());
  int index = 1;
  for (ServerFieldType type :
       {NAME_HONORIFIC_PREFIX, NAME_FIRST, NAME_MIDDLE, NAME_LAST_FIRST,
        NAME_LAST_CONJUNCTION, NAME_LAST_SECOND, NAME_LAST, NAME_FULL,
        NAME_FULL_WITH_HONORIFIC_PREFIX}) {
    s.BindString16(index++, profile.GetRawInfo(type));
    s.BindInt(index++, profile.GetVerificationStatusInt(type));
  }
  return s.Run();
}

bool AddAutofillProfileAddresses(const AutofillProfile& profile,
                                 sql::Database* db) {
  sql::Statement s;
  InsertBuilder(db, s, kAutofillProfileAddressesTable,
                {kGuid,
                 kStreetAddress,
                 kStreetAddressStatus,
                 kStreetName,
                 kStreetNameStatus,
                 kDependentStreetName,
                 kDependentStreetNameStatus,
                 kHouseNumber,
                 kHouseNumberStatus,
                 kSubpremise,
                 kSubpremiseStatus,
                 kPremiseName,
                 kPremiseNameStatus,
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
                 kFloorStatus});

  s.BindString(0, profile.guid());
  int index = 1;
  for (ServerFieldType type :
       {ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_NAME,
        ADDRESS_HOME_DEPENDENT_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER,
        ADDRESS_HOME_SUBPREMISE, ADDRESS_HOME_PREMISE_NAME,
        ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
        ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE, ADDRESS_HOME_COUNTRY,
        ADDRESS_HOME_APT_NUM, ADDRESS_HOME_FLOOR}) {
    s.BindString16(index++, profile.GetRawInfo(type));
    s.BindInt(index++, profile.GetVerificationStatusInt(type));
  }
  return s.Run();
}

bool AddAutofillProfileNamesToProfile(sql::Database* db,
                                      AutofillProfile* profile) {
  sql::Statement s;
  if (SelectByGuid(
          db, s, kAutofillProfileNamesTable,
          {kGuid, kHonorificPrefix, kHonorificPrefixStatus, kFirstName,
           kFirstNameStatus, kMiddleName, kMiddleNameStatus, kFirstLastName,
           kFirstLastNameStatus, kConjunctionLastName,
           kConjunctionLastNameStatus, kSecondLastName, kSecondLastNameStatus,
           kLastName, kLastNameStatus, kFullName, kFullNameStatus,
           kFullNameWithHonorificPrefix, kFullNameWithHonorificPrefixStatus},
          profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));

    int index = 1;
    for (ServerFieldType type :
         {NAME_HONORIFIC_PREFIX, NAME_FIRST, NAME_MIDDLE, NAME_LAST_FIRST,
          NAME_LAST_CONJUNCTION, NAME_LAST_SECOND, NAME_LAST, NAME_FULL,
          NAME_FULL_WITH_HONORIFIC_PREFIX}) {
      profile->SetRawInfoWithVerificationStatusInt(
          type, s.ColumnString16(index), s.ColumnInt(index + 1));
      index += 2;
    }
  }
  return s.Succeeded();
}

bool AddAutofillProfileAddressesToProfile(sql::Database* db,
                                          AutofillProfile* profile) {
  sql::Statement s;
  if (SelectByGuid(db, s, kAutofillProfileAddressesTable,
                   {kGuid,
                    kStreetAddress,
                    kStreetAddressStatus,
                    kStreetName,
                    kStreetNameStatus,
                    kDependentStreetName,
                    kDependentStreetNameStatus,
                    kHouseNumber,
                    kHouseNumberStatus,
                    kSubpremise,
                    kSubpremiseStatus,
                    kPremiseName,
                    kPremiseNameStatus,
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
      for (ServerFieldType type :
           {ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_NAME,
            ADDRESS_HOME_DEPENDENT_STREET_NAME, ADDRESS_HOME_HOUSE_NUMBER,
            ADDRESS_HOME_SUBPREMISE, ADDRESS_HOME_PREMISE_NAME,
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

bool AddAutofillProfileEmailsToProfile(sql::Database* db,
                                       AutofillProfile* profile) {
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

bool AddAutofillProfilePhonesToProfile(sql::Database* db,
                                       AutofillProfile* profile) {
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

bool AddAutofillProfileBirthdateToProfile(sql::Database* db,
                                          AutofillProfile* profile) {
  sql::Statement s;
  if (SelectByGuid(db, s, kAutofillProfileBirthdatesTable,
                   {kGuid, kDay, kMonth, kYear}, profile->guid())) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfoAsInt(BIRTHDATE_DAY, s.ColumnInt(1));
    profile->SetRawInfoAsInt(BIRTHDATE_MONTH, s.ColumnInt(2));
    profile->SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, s.ColumnInt(3));
  }
  return s.Succeeded();
}

bool AddAutofillProfileEmails(const AutofillProfile& profile,
                              sql::Database* db) {
  // Add the new email.
  sql::Statement s;
  InsertBuilder(db, s, kAutofillProfileEmailsTable, {kGuid, kEmail});
  s.BindString(0, profile.guid());
  s.BindString16(1, profile.GetRawInfo(EMAIL_ADDRESS));

  return s.Run();
}

bool AddAutofillProfilePhones(const AutofillProfile& profile,
                              sql::Database* db) {
  // Add the new number.
  sql::Statement s;
  InsertBuilder(db, s, kAutofillProfilePhonesTable, {kGuid, kNumber});
  s.BindString(0, profile.guid());
  s.BindString16(1, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  return s.Run();
}

bool AddAutofillProfileBirthdate(const AutofillProfile& profile,
                                 sql::Database* db) {
  // Add the new birthdate.
  sql::Statement s;
  InsertBuilder(db, s, kAutofillProfileBirthdatesTable,
                {kGuid, kDay, kMonth, kYear});
  s.BindString(0, profile.guid());
  s.BindInt(1, profile.GetRawInfoAsInt(BIRTHDATE_DAY));
  s.BindInt(2, profile.GetRawInfoAsInt(BIRTHDATE_MONTH));
  s.BindInt(3, profile.GetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR));

  return s.Run();
}

bool AddAutofillProfilePieces(const AutofillProfile& profile,
                              sql::Database* db) {
  return AddAutofillProfileNames(profile, db) &&
         AddAutofillProfileEmails(profile, db) &&
         AddAutofillProfilePhones(profile, db) &&
         AddAutofillProfileAddresses(profile, db) &&
         AddAutofillProfileBirthdate(profile, db);
}

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Database* db) {
  return DeleteWhereColumnEq(db, kAutofillProfileNamesTable, kGuid, guid) &&
         DeleteWhereColumnEq(db, kAutofillProfileEmailsTable, kGuid, guid) &&
         DeleteWhereColumnEq(db, kAutofillProfilePhonesTable, kGuid, guid) &&
         DeleteWhereColumnEq(db, kAutofillProfileAddressesTable, kGuid, guid) &&
         DeleteWhereColumnEq(db, kAutofillProfileBirthdatesTable, kGuid, guid);
}

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

time_t GetEndTime(const base::Time& end) {
  if (end.is_null() || end == base::Time::Max())
    return std::numeric_limits<time_t>::max();

  return end.ToTimeT();
}

// Returns |s| with |escaper| in front of each of occurrence of a character
// from |special_chars|. Any occurrence of |escaper| in |s| is doubled. For
// example, Substitute("hello_world!", "_%", '!'') returns "hello!_world!!".
std::u16string Substitute(const std::u16string& s,
                          const std::u16string& special_chars,
                          const char16_t& escaper) {
  // Prepend |escaper| to the list of |special_chars|.
  std::u16string escape_wildcards(special_chars);
  escape_wildcards.insert(escape_wildcards.begin(), escaper);

  // Prepend the |escaper| just before |special_chars| in |s|.
  std::u16string result(s);
  for (char16_t c : escape_wildcards) {
    for (size_t pos = 0; (pos = result.find(c, pos)) != std::u16string::npos;
         pos += 2) {
      result.insert(result.begin() + pos, escaper);
    }
  }

  return result;
}

// All ServerFieldTypes stored for an AutofillProfile in `kContactInfoTable`.
// When introducing a new field type, it suffices to add it here. When removing
// a field type, removing it from the list suffices (no additional clean-up in
// the table necessary).
// This is not reusing `AutofillProfile::SupportedTypes()` for three reasons:
// - Due to the table design, the stored types are already ambiguous, so we
//   prefer the explicitness here.
// - Some supported types (like PHONE_HOME_CITY_CODE) are not stored.
// - Some non-supported types are stored (usually types that don't have filling
//   support yet).
std::vector<ServerFieldType> GetStoredContactInfoTypes() {
  return {COMPANY_NAME,
          NAME_HONORIFIC_PREFIX,
          NAME_FIRST,
          NAME_MIDDLE,
          NAME_LAST_FIRST,
          NAME_LAST_CONJUNCTION,
          NAME_LAST_SECOND,
          NAME_LAST,
          NAME_FULL,
          NAME_FULL_WITH_HONORIFIC_PREFIX,
          ADDRESS_HOME_STREET_ADDRESS,
          ADDRESS_HOME_STREET_NAME,
          ADDRESS_HOME_DEPENDENT_STREET_NAME,
          ADDRESS_HOME_HOUSE_NUMBER,
          ADDRESS_HOME_SUBPREMISE,
          ADDRESS_HOME_PREMISE_NAME,
          ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,
          ADDRESS_HOME_SORTING_CODE,
          ADDRESS_HOME_COUNTRY,
          ADDRESS_HOME_APT_NUM,
          ADDRESS_HOME_FLOOR,
          EMAIL_ADDRESS,
          PHONE_HOME_WHOLE_NUMBER,
          BIRTHDATE_DAY,
          BIRTHDATE_MONTH,
          BIRTHDATE_4_DIGIT_YEAR};
}

// This helper function binds the `profile`s properties to the placeholders in
// `s`, in the order the columns are defined in the header file.
// Instead of `profile.modification_date()`, `modification_date` is used. This
// makes the function useful for updates as well.
void BindAutofillProfileToContactInfoStatement(
    const AutofillProfile& profile,
    const base::Time& modification_date,
    sql::Statement& s) {
  int index = 0;
  s.BindString(index++, profile.guid());
  s.BindInt64(index++, profile.use_count());
  s.BindInt64(index++, profile.use_date().ToTimeT());
  s.BindInt64(index++, modification_date.ToTimeT());
  s.BindString(index++, profile.language_code());
  s.BindString(index++, profile.profile_label());
  s.BindInt(index++, profile.initial_creator_id());
  s.BindInt(index++, profile.last_modifier_id());
}

// Inserts `profile` into `kContactInfoTable` and `kContactInfoTypeTokensTable`.
bool AddAutofillProfileToContactInfoTable(sql::Database* db,
                                          const AutofillProfile& profile,
                                          const base::Time& modification_date) {
  sql::Statement s;
  InsertBuilder(db, s, kContactInfoTable,
                {kGuid, kUseCount, kUseDate, kDateModified, kLanguageCode,
                 kLabel, kInitialCreatorId, kLastModifierId});
  BindAutofillProfileToContactInfoStatement(profile, modification_date, s);
  if (!s.Run())
    return false;
  for (ServerFieldType type : GetStoredContactInfoTypes()) {
    InsertBuilder(db, s, kContactInfoTypeTokensTable,
                  {kGuid, kType, kValue, kVerificationStatus});
    s.BindString(0, profile.guid());
    s.BindInt(1, type);
    s.BindString16(2, Truncate(profile.GetRawInfo(type)));
    s.BindInt(3, profile.GetVerificationStatusInt(type));
    if (!s.Run())
      return false;
  }
  return true;
}

// Reads the profile with `guid` from `kContactInfoTable`. The profile's source
// is set to `kAccount`.
std::unique_ptr<AutofillProfile> GetAutofillProfileFromContactInfoTable(
    sql::Database* db,
    const std::string& guid) {
  sql::Statement s;
  if (!SelectByGuid(db, s, kContactInfoTable,
                    {kUseCount, kUseDate, kDateModified, kLanguageCode, kLabel,
                     kInitialCreatorId, kLastModifierId},
                    guid)) {
    return nullptr;
  }
  auto profile = std::make_unique<AutofillProfile>(
      guid, /*origin=*/"", AutofillProfile::Source::kAccount);
  int index = 0;
  profile->set_use_count(s.ColumnInt64(index++));
  profile->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_modification_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_language_code(s.ColumnString(index++));
  profile->set_profile_label(s.ColumnString(index++));
  profile->set_initial_creator_id(s.ColumnInt(index++));
  profile->set_last_modifier_id(s.ColumnInt(index++));

  if (!SelectByGuid(db, s, kContactInfoTypeTokensTable,
                    {kType, kValue, kVerificationStatus}, guid)) {
    return nullptr;
  }
  // As `SelectByGuid()` already calls `s.Step()`, do-while is used here.
  do {
    ServerFieldType type = ToSafeServerFieldType(s.ColumnInt(0), UNKNOWN_TYPE);
    DCHECK(type != UNKNOWN_TYPE);
    profile->SetRawInfoWithVerificationStatusInt(type, s.ColumnString16(1),
                                                 s.ColumnInt(2));
  } while (s.Step());

  profile->FinalizeAfterImport();
  return profile;
}

}  // namespace

// static
const size_t AutofillTable::kMaxDataLength = 1024;

AutofillTable::AutofillTable()
    : autofill_table_encryptor_(
          AutofillTableEncryptorFactory::GetInstance()->Create()) {
  DCHECK(autofill_table_encryptor_);
}

AutofillTable::~AutofillTable() = default;

AutofillTable* AutofillTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<AutofillTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey AutofillTable::GetTypeKey() const {
  return GetKey();
}

bool AutofillTable::CreateTablesIfNecessary() {
  return InitMainTable() && InitCreditCardsTable() && InitIBANsTable() &&
         InitProfilesTable() && InitProfileAddressesTable() &&
         InitProfileNamesTable() && InitProfileEmailsTable() &&
         InitProfilePhonesTable() && InitProfileBirthdatesTable() &&
         InitMaskedCreditCardsTable() && InitUnmaskedCreditCardsTable() &&
         InitServerCardMetadataTable() && InitServerAddressesTable() &&
         InitServerAddressMetadataTable() && InitAutofillSyncMetadataTable() &&
         InitModelTypeStateTable() && InitPaymentsCustomerDataTable() &&
         InitPaymentsUPIVPATable() &&
         InitServerCreditCardCloudTokenDataTable() && InitOfferDataTable() &&
         InitOfferEligibleInstrumentTable() && InitOfferMerchantDomainTable() &&
         InitContactInfoTable() && InitContactInfoTypeTokensTable() &&
         InitVirtualCardUsageDataTable();
}

bool AutofillTable::IsSyncable() {
  return true;
}

bool AutofillTable::MigrateToVersion(int version,
                                     bool* update_compatible_version) {
  // Migrate if necessary.
  switch (version) {
    case 83:
      *update_compatible_version = true;
      return MigrateToVersion83RemoveServerCardTypeColumn();
    case 84:
      *update_compatible_version = false;
      return MigrateToVersion84AddNicknameColumn();
    case 85:
      *update_compatible_version = false;
      return MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard();
    case 86:
      *update_compatible_version = false;
      return MigrateToVersion86RemoveUnmaskedCreditCardsUseColumns();
    case 87:
      *update_compatible_version = false;
      return MigrateToVersion87AddCreditCardNicknameColumn();
    case 88:
      *update_compatible_version = false;
      return MigrateToVersion88AddNewNameColumns();
    case 89:
      *update_compatible_version = false;
      return MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard();
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
    case 94:
      *update_compatible_version = false;
      return MigrateToVersion94AddPromoCodeColumnsToOfferData();
    case 95:
      *update_compatible_version = false;
      return MigrateToVersion95AddVirtualCardMetadata();
    case 96:
      *update_compatible_version = false;
      return MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn();
    case 98:
      *update_compatible_version = true;
      return MigrateToVersion98RemoveStatusColumnMaskedCreditCards();
    case 99:
      *update_compatible_version = true;
      return MigrateToVersion99RemoveAutofillProfilesTrashTable();
    case 100:
      *update_compatible_version = true;
      return MigrateToVersion100RemoveProfileValidityBitfieldColumn();
    case 101:
      // update_compatible_version is set to false because this table is not
      // used since M99.
      *update_compatible_version = false;
      return MigrateToVersion101RemoveCreditCardArtImageTable();
    case 102:
      *update_compatible_version = false;
      return MigrateToVersion102AddAutofillBirthdatesTable();
    case 104:
      *update_compatible_version = false;
      return MigrateToVersion104AddProductDescriptionColumn();
    case 105:
      *update_compatible_version = false;
      return MigrateToVersion105AddAutofillIBANTable();
    case 106:
      *update_compatible_version = true;
      return MigrateToVersion106RecreateAutofillIBANTable();
    case 107:
      *update_compatible_version = false;
      return MigrateToVersion107AddContactInfoTables();
    case 108:
      *update_compatible_version = false;
      return MigrateToVersion108AddCardIssuerIdColumn();
    case 109:
      *update_compatible_version = false;
      return MigrateToVersion109AddVirtualCardUsageDataTable();
    case 110:
      *update_compatible_version = false;
      return MigrateToVersion110AddInitialCreatorIdAndLastModifierId();
  }
  return true;
}

bool AutofillTable::AddFormFieldValues(
    const std::vector<FormFieldData>& elements,
    std::vector<AutofillChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, AutofillClock::Now());
}

bool AutofillTable::AddFormFieldValue(const FormFieldData& element,
                                      std::vector<AutofillChange>* changes) {
  return AddFormFieldValueTime(element, changes, AutofillClock::Now());
}

bool AutofillTable::GetFormValuesForElementName(
    const std::u16string& name,
    const std::u16string& prefix,
    std::vector<AutofillEntry>* entries,
    int limit) {
  DCHECK(entries);
  bool succeeded = false;

  if (prefix.empty()) {
    sql::Statement s;
    SelectBuilder(db_, s, kAutofillTable,
                  {kName, kValue, kDateCreated, kDateLastUsed},
                  "WHERE name = ? ORDER BY count DESC LIMIT ?");
    s.BindString16(0, name);
    s.BindInt(1, limit);

    entries->clear();
    while (s.Step()) {
      entries->push_back(AutofillEntry(
          AutofillKey(/*name=*/s.ColumnString16(0),
                      /*value=*/s.ColumnString16(1)),
          /*date_created=*/base::Time::FromTimeT(s.ColumnInt64(2)),
          /*date_last_used=*/base::Time::FromTimeT(s.ColumnInt64(3))));
    }

    succeeded = s.Succeeded();
  } else {
    std::u16string prefix_lower = base::i18n::ToLower(prefix);
    std::u16string next_prefix = prefix_lower;
    next_prefix.back()++;

    sql::Statement s1;
    SelectBuilder(db_, s1, kAutofillTable,
                  {kName, kValue, kDateCreated, kDateLastUsed},
                  "WHERE name = ? AND "
                  "value_lower >= ? AND "
                  "value_lower < ? "
                  "ORDER BY count DESC "
                  "LIMIT ?");
    s1.BindString16(0, name);
    s1.BindString16(1, prefix_lower);
    s1.BindString16(2, next_prefix);
    s1.BindInt(3, limit);

    entries->clear();
    while (s1.Step()) {
      entries->push_back(AutofillEntry(
          AutofillKey(/*name=*/s1.ColumnString16(0),
                      /*value=*/s1.ColumnString16(1)),
          /*date_created=*/base::Time::FromTimeT(s1.ColumnInt64(2)),
          /*date_last_used=*/base::Time::FromTimeT(s1.ColumnInt64(3))));
    }

    succeeded = s1.Succeeded();

    if (IsFeatureSubstringMatchEnabled()) {
      sql::Statement s2;
      SelectBuilder(db_, s2, kAutofillTable,
                    {kName, kValue, kDateCreated, kDateLastUsed},
                    "WHERE name = ? AND ("
                    " value LIKE '% ' || :prefix || '%' ESCAPE '!' OR "
                    " value LIKE '%.' || :prefix || '%' ESCAPE '!' OR "
                    " value LIKE '%,' || :prefix || '%' ESCAPE '!' OR "
                    " value LIKE '%-' || :prefix || '%' ESCAPE '!' OR "
                    " value LIKE '%@' || :prefix || '%' ESCAPE '!' OR "
                    " value LIKE '%!_' || :prefix || '%' ESCAPE '!' ) "
                    "ORDER BY count DESC "
                    "LIMIT ?");

      s2.BindString16(0, name);
      // escaper as L'!' -> 0x21.
      s2.BindString16(1, Substitute(prefix_lower, u"_%", 0x21));
      s2.BindInt(2, limit);
      while (s2.Step()) {
        entries->push_back(AutofillEntry(
            AutofillKey(/*name=*/s2.ColumnString16(0),
                        /*value=*/s2.ColumnString16(1)),
            /*date_created=*/base::Time::FromTimeT(s2.ColumnInt64(2)),
            /*date_last_used=*/base::Time::FromTimeT(s2.ColumnInt64(3))));
      }

      succeeded &= s2.Succeeded();
    }
  }

  return succeeded;
}

bool AutofillTable::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<AutofillChange>* changes) {
  const time_t delete_begin_time_t = delete_begin.ToTimeT();
  const time_t delete_end_time_t = GetEndTime(delete_end);

  // Query for the name, value, count, and access dates of all form elements
  // that were used between the given times.
  sql::Statement s;
  SelectBuilder(db_, s, kAutofillTable,
                {kName, kValue, kCount, kDateCreated, kDateLastUsed},
                "WHERE (date_created >= ? AND date_created < ?) OR "
                "      (date_last_used >= ? AND date_last_used < ?)");
  s.BindInt64(0, delete_begin_time_t);
  s.BindInt64(1, delete_end_time_t);
  s.BindInt64(2, delete_begin_time_t);
  s.BindInt64(3, delete_end_time_t);

  std::vector<AutofillUpdate> updates;
  std::vector<AutofillChange> tentative_changes;
  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    int count = s.ColumnInt(2);
    time_t date_created_time_t = s.ColumnInt64(3);
    time_t date_last_used_time_t = s.ColumnInt64(4);

    // If *all* uses of the element were between |delete_begin| and
    // |delete_end|, then delete the element.  Otherwise, update the use
    // timestamps and use count.
    AutofillChange::Type change_type;
    if (date_created_time_t >= delete_begin_time_t &&
        date_last_used_time_t < delete_end_time_t) {
      change_type = AutofillChange::REMOVE;
    } else {
      change_type = AutofillChange::UPDATE;

      // For all updated elements, set either date_created or date_last_used so
      // that the range [date_created, date_last_used] no longer overlaps with
      // [delete_begin, delete_end). Update the count by interpolating.
      // Precisely, compute the average amount of time between increments to the
      // count in the original range [date_created, date_last_used]:
      //   avg_delta = (date_last_used_orig - date_created_orig) / (count - 1)
      // The count can be expressed as
      //   count = 1 + (date_last_used - date_created) / avg_delta
      // Hence, update the count to
      //   count_new = 1 + (date_last_used_new - date_created_new) / avg_delta
      //             = 1 + ((count - 1) *
      //                    (date_last_used_new - date_created_new) /
      //                    (date_last_used_orig - date_created_orig))
      // Interpolating might not give a result that completely accurately
      // reflects the user's history, but it's the best that can be done given
      // the information in the database.
      AutofillUpdate updated_entry;
      updated_entry.name = name;
      updated_entry.value = value;
      updated_entry.date_created = date_created_time_t < delete_begin_time_t
                                       ? date_created_time_t
                                       : delete_end_time_t;
      updated_entry.date_last_used = date_last_used_time_t >= delete_end_time_t
                                         ? date_last_used_time_t
                                         : delete_begin_time_t - 1;
      updated_entry.count =
          1 + base::ClampRound(
                  1.0 * (count - 1) *
                  (updated_entry.date_last_used - updated_entry.date_created) /
                  (date_last_used_time_t - date_created_time_t));
      updates.push_back(updated_entry);
    }

    tentative_changes.emplace_back(change_type, AutofillKey(name, value));
  }
  if (!s.Succeeded())
    return false;

  // As a single transaction, remove or update the elements appropriately.
  sql::Statement s_delete;
  DeleteBuilder(db_, s_delete, kAutofillTable,
                "date_created >= ? AND date_last_used < ?");
  s_delete.BindInt64(0, delete_begin_time_t);
  s_delete.BindInt64(1, delete_end_time_t);
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;
  if (!s_delete.Run())
    return false;
  for (const auto& update : updates) {
    sql::Statement s_update;
    UpdateBuilder(db_, s_update, kAutofillTable,
                  {kDateCreated, kDateLastUsed, kCount},
                  "name = ? AND value = ?");
    s_update.BindInt64(0, update.date_created);
    s_update.BindInt64(1, update.date_last_used);
    s_update.BindInt(2, update.count);
    s_update.BindString16(3, update.name);
    s_update.BindString16(4, update.value);
    if (!s_update.Run())
      return false;
  }
  if (!transaction.Commit())
    return false;

  *changes = tentative_changes;
  return true;
}

bool AutofillTable::RemoveExpiredFormElements(
    std::vector<AutofillChange>* changes) {
  const auto change_type = AutofillChange::EXPIRE;

  base::Time expiration_time =
      AutofillClock::Now() - kAutocompleteRetentionPolicyPeriod;

  // Query for the name and value of all form elements that were last used
  // before the |expiration_time|.
  sql::Statement select_for_delete;
  SelectBuilder(db_, select_for_delete, kAutofillTable, {kName, kValue},
                "WHERE date_last_used < ?");
  select_for_delete.BindInt64(0, expiration_time.ToTimeT());
  std::vector<AutofillChange> tentative_changes;
  while (select_for_delete.Step()) {
    std::u16string name = select_for_delete.ColumnString16(0);
    std::u16string value = select_for_delete.ColumnString16(1);
    tentative_changes.emplace_back(change_type, AutofillKey(name, value));
  }

  if (!select_for_delete.Succeeded())
    return false;

  sql::Statement delete_data_statement;
  DeleteBuilder(db_, delete_data_statement, kAutofillTable,
                "date_last_used < ?");
  delete_data_statement.BindInt64(0, expiration_time.ToTimeT());
  if (!delete_data_statement.Run())
    return false;

  *changes = tentative_changes;
  return true;
}

bool AutofillTable::RemoveFormElement(const std::u16string& name,
                                      const std::u16string& value) {
  sql::Statement s;
  DeleteBuilder(db_, s, kAutofillTable, "name = ? AND value= ?");
  s.BindString16(0, name);
  s.BindString16(1, value);
  return s.Run();
}

int AutofillTable::GetCountOfValuesContainedBetween(const base::Time& begin,
                                                    const base::Time& end) {
  const time_t begin_time_t = begin.ToTimeT();
  const time_t end_time_t = GetEndTime(end);

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT COUNT(DISTINCT(value1)) FROM ( "
      "  SELECT value AS value1 FROM autofill "
      "  WHERE NOT EXISTS ( "
      "    SELECT value AS value2, date_created, date_last_used FROM autofill "
      "    WHERE value1 = value2 AND "
      "          (date_created < ? OR date_last_used >= ?)))"));
  s.BindInt64(0, begin_time_t);
  s.BindInt64(1, end_time_t);

  if (!s.Step()) {
    NOTREACHED();
    return false;
  }
  return s.ColumnInt(0);
}

bool AutofillTable::GetAllAutofillEntries(std::vector<AutofillEntry>* entries) {
  sql::Statement s;
  SelectBuilder(db_, s, kAutofillTable,
                {kName, kValue, kDateCreated, kDateLastUsed});

  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    base::Time date_created = base::Time::FromTimeT(s.ColumnInt64(2));
    base::Time date_last_used = base::Time::FromTimeT(s.ColumnInt64(3));
    entries->push_back(
        AutofillEntry(AutofillKey(name, value), date_created, date_last_used));
  }

  return s.Succeeded();
}

bool AutofillTable::GetAutofillTimestamps(const std::u16string& name,
                                          const std::u16string& value,
                                          base::Time* date_created,
                                          base::Time* date_last_used) {
  sql::Statement s;
  SelectBuilder(db_, s, kAutofillTable, {kDateCreated, kDateLastUsed},
                "WHERE name = ? AND value = ?");
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step())
    return false;

  *date_created = base::Time::FromTimeT(s.ColumnInt64(0));
  *date_last_used = base::Time::FromTimeT(s.ColumnInt64(1));

  DCHECK(!s.Step());
  return true;
}

bool AutofillTable::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& entries) {
  if (entries.empty())
    return true;

  // Remove all existing entries.
  for (const auto& entry : entries) {
    sql::Statement s;
    DeleteBuilder(db_, s, kAutofillTable, "name = ? AND value = ?");
    s.BindString16(0, entry.key().name());
    s.BindString16(1, entry.key().value());
    if (!s.Run())
      return false;
  }

  // Insert all the supplied autofill entries.
  for (const auto& entry : entries) {
    if (!InsertAutofillEntry(entry))
      return false;
  }

  return true;
}

bool AutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  if (profile.source() == AutofillProfile::Source::kAccount) {
    sql::Transaction transaction(db_);
    return transaction.Begin() &&
           AddAutofillProfileToContactInfoTable(
               db_, profile, /*modification_date=*/AutofillClock::Now()) &&
           transaction.Commit();
  }

  DCHECK(profile.source() == AutofillProfile::Source::kLocalOrSyncable);
  sql::Statement s;
  InsertBuilder(
      db_, s, kAutofillProfilesTable,
      {kGuid, kCompanyName, kStreetAddress, kDependentLocality, kCity, kState,
       kZipcode, kSortingCode, kCountryCode, kUseCount, kUseDate, kDateModified,
       kOrigin, kLanguageCode, kLabel, kDisallowSettingsVisibleUpdates});
  BindAutofillProfileToStatement(profile, AutofillClock::Now(), &s);

  if (!s.Run())
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::UpdateAutofillProfile(const AutofillProfile& profile) {
  DCHECK(base::IsValidGUID(profile.guid()));

  std::unique_ptr<AutofillProfile> old_profile =
      GetAutofillProfile(profile.guid(), profile.source());
  if (!old_profile)
    return false;

  base::Time new_modification_date = *old_profile != profile
                                         ? AutofillClock::Now()
                                         : old_profile->modification_date();

  if (profile.source() == AutofillProfile::Source::kAccount) {
    // Implementing an update as remove + add has multiple advantages:
    // - Prevents outdated (ServerFieldType, value) pairs from remaining in the
    //   `kContactInfoTypeTokensTables`, in case field types are removed.
    // - Simpler code.
    // The possible downside is performance. This is not an issue, as updates
    // happen rarely and asynchronously.
    sql::Transaction transaction(db_);
    return transaction.Begin() &&
           RemoveAutofillProfile(profile.guid(), profile.source()) &&
           AddAutofillProfileToContactInfoTable(db_, profile,
                                                new_modification_date) &&
           transaction.Commit();
  }

  DCHECK(profile.source() == AutofillProfile::Source::kLocalOrSyncable);
  sql::Statement s;
  UpdateBuilder(
      db_, s, kAutofillProfilesTable,
      {kGuid, kCompanyName, kStreetAddress, kDependentLocality, kCity, kState,
       kZipcode, kSortingCode, kCountryCode, kUseCount, kUseDate, kDateModified,
       kOrigin, kLanguageCode, kLabel, kDisallowSettingsVisibleUpdates},
      "guid = ?1");
  BindAutofillProfileToStatement(profile, new_modification_date, &s);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  if (!result)
    return result;

  // Remove the old names, emails, and phone numbers.
  if (!RemoveAutofillProfilePieces(profile.guid(), db_))
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::RemoveAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source) {
  DCHECK(base::IsValidGUID(guid));
  if (profile_source == AutofillProfile::Source::kAccount) {
    sql::Transaction transaction(db_);
    return transaction.Begin() &&
           DeleteWhereColumnEq(db_, kContactInfoTable, kGuid, guid) &&
           DeleteWhereColumnEq(db_, kContactInfoTypeTokensTable, kGuid, guid) &&
           transaction.Commit();
  }
  DCHECK(profile_source == AutofillProfile::Source::kLocalOrSyncable);
  return DeleteWhereColumnEq(db_, kAutofillProfilesTable, kGuid, guid) &&
         RemoveAutofillProfilePieces(guid, db_);
}

bool AutofillTable::RemoveAllAutofillProfiles(
    AutofillProfile::Source profile_source) {
  DCHECK(profile_source == AutofillProfile::Source::kAccount);
  sql::Transaction transaction(db_);
  return transaction.Begin() && Delete(db_, kContactInfoTable) &&
         Delete(db_, kContactInfoTypeTokensTable) && transaction.Commit();
}

std::unique_ptr<AutofillProfile> AutofillTable::GetAutofillProfile(
    const std::string& guid,
    AutofillProfile::Source profile_source) {
  DCHECK(base::IsValidGUID(guid));
  if (profile_source == AutofillProfile::Source::kAccount)
    return GetAutofillProfileFromContactInfoTable(db_, guid);

  DCHECK(profile_source == AutofillProfile::Source::kLocalOrSyncable);
  sql::Statement s;
  if (!SelectByGuid(db_, s, kAutofillProfilesTable,
                    {kOrigin, kCompanyName, kStreetAddress, kDependentLocality,
                     kCity, kState, kZipcode, kSortingCode, kCountryCode,
                     kUseCount, kUseDate, kDateModified, kLanguageCode, kLabel,
                     kDisallowSettingsVisibleUpdates},
                    guid)) {
    return nullptr;
  }

  auto profile = std::make_unique<AutofillProfile>(
      guid, /*origin=*/s.ColumnString(0),
      AutofillProfile::Source::kLocalOrSyncable);
  DCHECK(base::IsValidGUID(profile->guid()));

  // Get associated name info using guid.
  AddAutofillProfileNamesToProfile(db_, profile.get());

  // Get associated email info using guid.
  AddAutofillProfileEmailsToProfile(db_, profile.get());

  // Get associated phone info using guid.
  AddAutofillProfilePhonesToProfile(db_, profile.get());

  // Get associated birthdate info using guid.
  AddAutofillProfileBirthdateToProfile(db_, profile.get());

  // The details should be added after the other info to make sure they don't
  // change when we change the names/emails/phones.
  AddAutofillProfileDetailsFromStatement(s, profile.get());

  // The structured address information should be added after the street_address
  // from the query above was  written because this information is used to
  // detect changes by a legacy client.
  AddAutofillProfileAddressesToProfile(db_, profile.get());

  // For more-structured profiles, the profile must be finalized to fully
  // populate the name fields.
  profile->FinalizeAfterImport();

  return profile;
}

bool AutofillTable::GetAutofillProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles,
    AutofillProfile::Source profile_source) {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s;
  SelectBuilder(db_, s,
                profile_source == AutofillProfile::Source::kAccount
                    ? kContactInfoTable
                    : kAutofillProfilesTable,
                {kGuid}, "ORDER BY date_modified DESC, guid");

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfile(guid, profile_source);
    if (!profile)
      continue;
    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

bool AutofillTable::GetServerProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) const {
  profiles->clear();

  sql::Statement s;
  SelectBuilder(
      db_, s, kServerAddressesTable,
      {kId, kUseCount, kUseDate, kRecipientName, kCompanyName, kStreetAddress,
       kAddress1,     // ADDRESS_HOME_STATE
       kAddress2,     // ADDRESS_HOME_CITY
       kAddress3,     // ADDRESS_HOME_DEPENDENT_LOCALITY
       kAddress4,     // Not supported in AutofillProfile yet.
       kPostalCode,   // ADDRESS_HOME_ZIP
       kSortingCode,  // ADDRESS_HOME_SORTING_CODE
       kCountryCode,  // ADDRESS_HOME_COUNTRY
       kPhoneNumber,  // PHONE_HOME_WHOLE_NUMBER
       kLanguageCode, kHasConverted},
      "LEFT OUTER JOIN server_address_metadata USING (id)");

  while (s.Step()) {
    int index = 0;
    std::unique_ptr<AutofillProfile> profile =
        std::make_unique<AutofillProfile>(AutofillProfile::SERVER_PROFILE,
                                          s.ColumnString(index++));
    profile->set_use_count(s.ColumnInt64(index++));
    profile->set_use_date(
        base::Time::FromInternalValue(s.ColumnInt64(index++)));
    // Modification date is not tracked for server profiles. Explicitly set it
    // here to override the default value of AutofillClock::Now().
    profile->set_modification_date(base::Time());

    std::u16string recipient_name = s.ColumnString16(index++);
    for (ServerFieldType type :
         {COMPANY_NAME, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STATE,
          ADDRESS_HOME_CITY, ADDRESS_HOME_DEPENDENT_LOCALITY}) {
      profile->SetRawInfo(type, s.ColumnString16(index++));
    }
    index++;  // Skip address_4 which we haven't added to AutofillProfile yet.
    for (ServerFieldType type :
         {ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE, ADDRESS_HOME_COUNTRY}) {
      profile->SetRawInfo(type, s.ColumnString16(index++));
    }
    std::u16string phone_number = s.ColumnString16(index++);
    profile->set_language_code(s.ColumnString(index++));
    profile->set_has_converted(s.ColumnBool(index++));

    // SetInfo instead of SetRawInfo so the constituent pieces will be parsed
    // for these data types.
    profile->SetInfo(NAME_FULL, recipient_name, profile->language_code());
    profile->SetInfo(PHONE_HOME_WHOLE_NUMBER, phone_number,
                     profile->language_code());

    // For more-structured profiles, the profile must be finalized to fully
    // populate the name fields.
    profile->FinalizeAfterImport();

    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

void AutofillTable::SetServerProfilesAndMetadata(
    const std::vector<AutofillProfile>& profiles,
    bool update_metadata) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old ones first.
  Delete(db_, kServerAddressesTable);

  sql::Statement insert;
  InsertBuilder(db_, insert, kServerAddressesTable,
                {kId, kRecipientName, kCompanyName, kStreetAddress,
                 kAddress1,     // ADDRESS_HOME_STATE
                 kAddress2,     // ADDRESS_HOME_CITY
                 kAddress3,     // ADDRESS_HOME_DEPENDENT_LOCALITY
                 kAddress4,     // Not supported in AutofillProfile yet.
                 kPostalCode,   // ADDRESS_HOME_ZIP
                 kSortingCode,  // ADDRESS_HOME_SORTING_CODE
                 kCountryCode,  // ADDRESS_HOME_COUNTRY
                 kPhoneNumber,  // PHONE_HOME_WHOLE_NUMBER
                 kLanguageCode});
  for (const auto& profile : profiles) {
    DCHECK(profile.record_type() == AutofillProfile::SERVER_PROFILE);

    int index = 0;
    insert.BindString(index++, profile.server_id());
    for (ServerFieldType type :
         {NAME_FULL, COMPANY_NAME, ADDRESS_HOME_STREET_ADDRESS,
          ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
          ADDRESS_HOME_DEPENDENT_LOCALITY}) {
      insert.BindString16(index++, profile.GetRawInfo(type));
    }
    index++;  // Skip address_4 which we haven't added to AutofillProfile yet.
    for (ServerFieldType type :
         {ADDRESS_HOME_ZIP, ADDRESS_HOME_SORTING_CODE, ADDRESS_HOME_COUNTRY,
          PHONE_HOME_WHOLE_NUMBER}) {
      insert.BindString16(index++, profile.GetRawInfo(type));
    }
    insert.BindString(index++, profile.language_code());

    insert.Run();
    insert.Reset(true);

    if (update_metadata) {
      // Save the use count and use date of the profile.
      UpdateServerAddressMetadata(profile);
    }
  }

  if (update_metadata) {
    // Delete metadata that's no longer relevant.
    Delete(db_, kServerAddressMetadataTable,
           "id NOT IN (SELECT id FROM server_addresses)");
  }

  transaction.Commit();
}

void AutofillTable::SetServerProfiles(
    const std::vector<AutofillProfile>& profiles) {
  SetServerProfilesAndMetadata(profiles, /*update_metadata=*/true);
}

bool AutofillTable::AddIBAN(const IBAN& iban) {
  sql::Statement s;
  InsertBuilder(db_, s, kIBANsTable,
                {kGuid, kUseCount, kUseDate, kValue, kNickname});
  BindIBANToStatement(iban, &s, *autofill_table_encryptor_);
  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return true;
}

bool AutofillTable::UpdateIBAN(const IBAN& iban) {
  DCHECK(base::IsValidGUID(iban.guid()));

  std::unique_ptr<IBAN> old_iban = GetIBAN(iban.guid());
  if (!old_iban) {
    return false;
  }

  if (*old_iban == iban) {
    return true;
  }

  sql::Statement s;
  UpdateBuilder(db_, s, kIBANsTable,
                {kGuid, kUseCount, kUseDate, kValue, kNickname}, "guid=?1");
  BindIBANToStatement(iban, &s, *autofill_table_encryptor_);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveIBAN(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  return DeleteWhereColumnEq(db_, kIBANsTable, kGuid, guid);
}

std::unique_ptr<IBAN> AutofillTable::GetIBAN(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s;
  SelectBuilder(db_, s, kIBANsTable,
                {kGuid, kUseCount, kUseDate, kValue, kNickname},
                "WHERE guid = ?");
  s.BindString(0, guid);

  if (!s.Step())
    return nullptr;

  return IBANFromStatement(s, *autofill_table_encryptor_);
}

bool AutofillTable::GetIBANs(std::vector<std::unique_ptr<IBAN>>* ibans) {
  DCHECK(ibans);
  ibans->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kIBANsTable, {kGuid}, "ORDER BY use_date DESC, guid");

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<IBAN> iban = GetIBAN(guid);
    if (!iban)
      return false;
    ibans->push_back(std::move(iban));
  }

  return s.Succeeded();
}

bool AutofillTable::AddCreditCard(const CreditCard& credit_card) {
  sql::Statement s;
  InsertBuilder(db_, s, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname});
  BindCreditCardToStatement(credit_card, AutofillClock::Now(), &s,
                            *autofill_table_encryptor_);

  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return true;
}

bool AutofillTable::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK(base::IsValidGUID(credit_card.guid()));

  std::unique_ptr<CreditCard> old_credit_card =
      GetCreditCard(credit_card.guid());
  if (!old_credit_card)
    return false;

  bool update_modification_date = *old_credit_card != credit_card;

  sql::Statement s;
  UpdateBuilder(db_, s, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname},
                "guid=?1");
  BindCreditCardToStatement(credit_card,
                            update_modification_date
                                ? AutofillClock::Now()
                                : old_credit_card->modification_date(),
                            &s, *autofill_table_encryptor_);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  return DeleteWhereColumnEq(db_, kCreditCardsTable, kGuid, guid);
}

bool AutofillTable::AddFullServerCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::FULL_SERVER_CARD, credit_card.record_type());
  DCHECK(!credit_card.number().empty());
  DCHECK(!credit_card.server_id().empty());

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromUnmaskedCreditCards(credit_card.server_id());
  DeleteFromMaskedCreditCards(credit_card.server_id());

  CreditCard masked(credit_card);
  masked.set_record_type(CreditCard::MASKED_SERVER_CARD);
  masked.SetNumber(credit_card.LastFourDigits());
  masked.RecordAndLogUse();
  DCHECK(!masked.network().empty());
  AddMaskedCreditCards({masked});

  AddUnmaskedCreditCard(credit_card.server_id(), credit_card.number());

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

std::unique_ptr<CreditCard> AutofillTable::GetCreditCard(
    const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s;
  SelectBuilder(db_, s, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname},
                "WHERE guid = ?");
  s.BindString(0, guid);

  if (!s.Step())
    return nullptr;

  return CreditCardFromStatement(s, *autofill_table_encryptor_);
}

bool AutofillTable::GetCreditCards(
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kCreditCardsTable, {kGuid},
                "ORDER BY date_modified DESC, guid");

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<CreditCard> credit_card = GetCreditCard(guid);
    if (!credit_card)
      return false;
    credit_cards->push_back(std::move(credit_card));
  }

  return s.Succeeded();
}

bool AutofillTable::GetServerCreditCards(
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) const {
  credit_cards->clear();

  sql::Statement s;
  SelectBuilder(
      db_, s, base::StrCat({kMaskedCreditCardsTable, " AS masked"}),
      {kCardNumberEncrypted, kLastFour, base::StrCat({"masked.", kId}),
       base::StrCat({"metadata.", kUseCount}),
       base::StrCat({"metadata.", kUseDate}), kNetwork, kNameOnCard, kExpMonth,
       kExpYear, base::StrCat({"metadata.", kBillingAddressId}), kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kCardArtUrl, kProductDescription},
      "LEFT OUTER JOIN unmasked_credit_cards USING (id) "
      "LEFT OUTER JOIN server_card_metadata AS metadata USING (id)");
  while (s.Step()) {
    int index = 0;

    // If the card_number_encrypted field is nonempty, we can assume this card
    // is a full card, otherwise it's masked.
    std::u16string full_card_number =
        UnencryptedCardFromColumn(s, index++, *autofill_table_encryptor_);
    std::u16string last_four = s.ColumnString16(index++);
    CreditCard::RecordType record_type = full_card_number.empty()
                                             ? CreditCard::MASKED_SERVER_CARD
                                             : CreditCard::FULL_SERVER_CARD;
    std::string server_id = s.ColumnString(index++);
    std::unique_ptr<CreditCard> card =
        std::make_unique<CreditCard>(record_type, server_id);
    card->SetRawInfo(CREDIT_CARD_NUMBER,
                     record_type == CreditCard::MASKED_SERVER_CARD
                         ? last_four
                         : full_card_number);
    card->set_use_count(s.ColumnInt64(index++));
    card->set_use_date(base::Time::FromInternalValue(s.ColumnInt64(index++)));
    // Modification date is not tracked for server cards. Explicitly set it here
    // to override the default value of AutofillClock::Now().
    card->set_modification_date(base::Time());

    std::string card_network = s.ColumnString(index++);
    if (record_type == CreditCard::MASKED_SERVER_CARD) {
      // The issuer network must be set after setting the number to override the
      // autodetected issuer network.
      card->SetNetworkForMaskedCard(card_network.c_str());
    } else {
      DCHECK_EQ(CreditCard::GetCardNetwork(full_card_number), card_network);
    }

    card->SetRawInfo(CREDIT_CARD_NAME_FULL, s.ColumnString16(index++));
    card->SetRawInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(index++));
    card->SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, s.ColumnString16(index++));
    card->set_billing_address_id(s.ColumnString(index++));
    card->set_bank_name(s.ColumnString(index++));
    card->SetNickname(s.ColumnString16(index++));
    card->set_card_issuer(
        static_cast<CreditCard::Issuer>(s.ColumnInt(index++)));
    card->set_issuer_id(s.ColumnString(index++));
    card->set_instrument_id(s.ColumnInt64(index++));
    card->set_virtual_card_enrollment_state(
        static_cast<CreditCard::VirtualCardEnrollmentState>(
            s.ColumnInt(index++)));
    card->set_card_art_url(GURL(s.ColumnString(index++)));
    card->set_product_description(s.ColumnString16(index++));
    credit_cards->push_back(std::move(card));
  }
  return s.Succeeded();
}

void AutofillTable::SetServerCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db_, kMaskedCreditCardsTable);

  AddMaskedCreditCards(credit_cards);

  // Delete all items in the unmasked table that aren't in the new set.
  Delete(db_, kUnmaskedCreditCardsTable,
         "id NOT IN (SELECT id FROM masked_credit_cards)");
  // Do the same for metadata.
  Delete(db_, kServerCardMetadataTable,
         "id NOT IN (SELECT id FROM masked_credit_cards)");

  transaction.Commit();
}

bool AutofillTable::UnmaskServerCreditCard(const CreditCard& masked,
                                           const std::u16string& full_number) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromUnmaskedCreditCards(masked.server_id());

  AddUnmaskedCreditCard(masked.server_id(), full_number);

  CreditCard unmasked = masked;
  unmasked.set_record_type(CreditCard::FULL_SERVER_CARD);
  unmasked.SetNumber(full_number);
  unmasked.RecordAndLogUse();
  UpdateServerCardMetadata(unmasked);

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::MaskServerCreditCard(const std::string& id) {
  return DeleteFromUnmaskedCreditCards(id);
}

bool AutofillTable::AddServerCardMetadata(
    const AutofillMetadata& card_metadata) {
  sql::Statement s;
  InsertBuilder(db_, s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, card_metadata.use_count);
  s.BindTime(1, card_metadata.use_date);
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerCardMetadata(const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  DeleteWhereColumnEq(db_, kServerCardMetadataTable, kId,
                      credit_card.server_id());

  sql::Statement s;
  InsertBuilder(db_, s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, credit_card.use_count());
  s.BindTime(1, credit_card.use_date());
  s.BindString(2, credit_card.billing_address_id());
  s.BindString(3, credit_card.server_id());
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerCardMetadata(
    const AutofillMetadata& card_metadata) {
  // Do not check if there was a record that got deleted. Inserting a new one is
  // also fine.
  RemoveServerCardMetadata(card_metadata.id);
  sql::Statement s;
  InsertBuilder(db_, s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, card_metadata.use_count);
  s.BindTime(1, card_metadata.use_date);
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::RemoveServerCardMetadata(const std::string& id) {
  DeleteWhereColumnEq(db_, kServerCardMetadataTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::GetServerCardsMetadata(
    std::map<std::string, AutofillMetadata>* cards_metadata) const {
  cards_metadata->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kServerCardMetadataTable,
                {kId, kUseCount, kUseDate, kBillingAddressId});

  while (s.Step()) {
    int index = 0;

    AutofillMetadata card_metadata;
    card_metadata.id = s.ColumnString(index++);
    card_metadata.use_count = s.ColumnInt64(index++);
    card_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    card_metadata.billing_address_id = s.ColumnString(index++);
    (*cards_metadata)[card_metadata.id] = card_metadata;
  }
  return s.Succeeded();
}

bool AutofillTable::AddServerAddressMetadata(
    const AutofillMetadata& address_metadata) {
  sql::Statement s;
  InsertBuilder(db_, s, kServerAddressMetadataTable,
                {kUseCount, kUseDate, kHasConverted, kId});
  s.BindInt64(0, address_metadata.use_count);
  s.BindInt64(1, address_metadata.use_date.ToInternalValue());
  s.BindBool(2, address_metadata.has_converted);
  s.BindString(3, address_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

// TODO(crbug.com/680182): Record the address conversion status when a server
// address gets converted.
bool AutofillTable::UpdateServerAddressMetadata(
    const AutofillProfile& profile) {
  DCHECK_EQ(AutofillProfile::SERVER_PROFILE, profile.record_type());

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  DeleteWhereColumnEq(db_, kServerAddressMetadataTable, kId,
                      profile.server_id());

  sql::Statement s;
  InsertBuilder(db_, s, kServerAddressMetadataTable,
                {kUseCount, kUseDate, kHasConverted, kId});

  s.BindInt64(0, profile.use_count());
  s.BindInt64(1, profile.use_date().ToInternalValue());
  s.BindBool(2, profile.has_converted());
  s.BindString(3, profile.server_id());
  s.Run();

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerAddressMetadata(
    const AutofillMetadata& address_metadata) {
  // Do not check if there was a record that got deleted. Inserting a new one is
  // also fine.
  RemoveServerAddressMetadata(address_metadata.id);
  sql::Statement s;
  InsertBuilder(db_, s, kServerAddressMetadataTable,
                {kUseCount, kUseDate, kHasConverted, kId});
  s.BindInt64(0, address_metadata.use_count);
  s.BindInt64(1, address_metadata.use_date.ToInternalValue());
  s.BindBool(2, address_metadata.has_converted);
  s.BindString(3, address_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::RemoveServerAddressMetadata(const std::string& id) {
  DeleteWhereColumnEq(db_, kServerAddressMetadataTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::GetServerAddressesMetadata(
    std::map<std::string, AutofillMetadata>* addresses_metadata) const {
  addresses_metadata->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kServerAddressMetadataTable,
                {kId, kUseCount, kUseDate, kHasConverted});
  while (s.Step()) {
    int index = 0;

    AutofillMetadata address_metadata;
    address_metadata.id = s.ColumnString(index++);
    address_metadata.use_count = s.ColumnInt64(index++);
    address_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    address_metadata.has_converted = s.ColumnBool(index++);
    (*addresses_metadata)[address_metadata.id] = address_metadata;
  }
  return s.Succeeded();
}

void AutofillTable::SetServerCardsData(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db_, kMaskedCreditCardsTable);

  // Add all the masked cards.
  sql::Statement masked_insert;
  InsertBuilder(
      db_, masked_insert, kMaskedCreditCardsTable,
      {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear, kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kCardArtUrl, kProductDescription});

  int index;
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
    index = 0;
    masked_insert.BindString(index++, card.server_id());
    masked_insert.BindString(index++, card.network());
    masked_insert.BindString16(index++, card.GetRawInfo(CREDIT_CARD_NAME_FULL));
    masked_insert.BindString16(index++, card.LastFourDigits());
    masked_insert.BindString16(index++, card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
    masked_insert.BindString16(index++,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
    masked_insert.BindString(index++, card.bank_name());
    masked_insert.BindString16(index++, card.nickname());
    masked_insert.BindInt(index++, static_cast<int>(card.card_issuer()));
    masked_insert.BindString(index++, card.issuer_id());
    masked_insert.BindInt64(index++, card.instrument_id());
    masked_insert.BindInt(
        index++, static_cast<int>(card.virtual_card_enrollment_state()));
    masked_insert.BindString(index++, card.card_art_url().spec());
    masked_insert.BindString16(index++, card.product_description());
    masked_insert.Run();
    masked_insert.Reset(true);
  }

  // Delete all items in the unmasked table that aren't in the new set.
  Delete(db_, kUnmaskedCreditCardsTable,
         "id NOT IN (SELECT id FROM masked_credit_cards)");

  transaction.Commit();
}

void AutofillTable::SetServerAddressesData(
    const std::vector<AutofillProfile>& profiles) {
  SetServerProfilesAndMetadata(profiles, /*update_metadata=*/false);
}

void AutofillTable::SetCreditCardCloudTokenData(
    const std::vector<CreditCardCloudTokenData>& credit_card_cloud_token_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Deletes all old values.
  Delete(db_, kServerCardCloudTokenDataTable);

  // Inserts new values.
  sql::Statement insert_cloud_token;
  InsertBuilder(
      db_, insert_cloud_token, kServerCardCloudTokenDataTable,
      {kId, kSuffix, kExpMonth, kExpYear, kCardArtUrl, kInstrumentToken});

  for (const CreditCardCloudTokenData& data : credit_card_cloud_token_data) {
    insert_cloud_token.BindString(0, data.masked_card_id);
    insert_cloud_token.BindString16(1, data.suffix);
    insert_cloud_token.BindString16(2, data.ExpirationMonthAsString());
    insert_cloud_token.BindString16(3, data.Expiration4DigitYearAsString());
    insert_cloud_token.BindString(4, data.card_art_url);
    insert_cloud_token.BindString(5, data.instrument_token);
    insert_cloud_token.Run();
    insert_cloud_token.Reset(true);
  }
  transaction.Commit();
}

bool AutofillTable::GetCreditCardCloudTokenData(
    std::vector<std::unique_ptr<CreditCardCloudTokenData>>*
        credit_card_cloud_token_data) {
  credit_card_cloud_token_data->clear();

  sql::Statement s;
  SelectBuilder(
      db_, s, kServerCardCloudTokenDataTable,
      {kId, kSuffix, kExpMonth, kExpYear, kCardArtUrl, kInstrumentToken});

  while (s.Step()) {
    int index = 0;
    std::unique_ptr<CreditCardCloudTokenData> data =
        std::make_unique<CreditCardCloudTokenData>();
    data->masked_card_id = s.ColumnString(index++);
    data->suffix = s.ColumnString16(index++);
    data->SetExpirationMonthFromString(s.ColumnString16(index++));
    data->SetExpirationYearFromString(s.ColumnString16(index++));
    data->card_art_url = s.ColumnString(index++);
    data->instrument_token = s.ColumnString(index++);
    credit_card_cloud_token_data->push_back(std::move(data));
  }

  return s.Succeeded();
}

void AutofillTable::SetPaymentsCustomerData(
    const PaymentsCustomerData* customer_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db_, kPaymentsCustomerDataTable);

  if (customer_data) {
    sql::Statement insert_customer_data;
    InsertBuilder(db_, insert_customer_data, kPaymentsCustomerDataTable,
                  {kCustomerId});
    insert_customer_data.BindString(0, customer_data->customer_id);
    insert_customer_data.Run();
  }

  transaction.Commit();
}

bool AutofillTable::GetPaymentsCustomerData(
    std::unique_ptr<PaymentsCustomerData>* customer_data) const {
  sql::Statement s;
  SelectBuilder(db_, s, kPaymentsCustomerDataTable, {kCustomerId});
  if (s.Step()) {
    *customer_data = std::make_unique<PaymentsCustomerData>(
        /*customer_id=*/s.ColumnString(0));
  }

  return s.Succeeded();
}

void AutofillTable::SetAutofillOffers(
    const std::vector<AutofillOfferData>& autofill_offer_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db_, kOfferDataTable);
  Delete(db_, kOfferEligibleInstrumentTable);
  Delete(db_, kOfferMerchantDomainTable);

  // Insert new values.
  sql::Statement insert_offers;
  InsertBuilder(
      db_, insert_offers, kOfferDataTable,
      {kOfferId, kOfferRewardAmount, kExpiry, kOfferDetailsUrl, kPromoCode,
       kValuePropText, kSeeDetailsText, kUsageInstructionsText});

  for (const AutofillOfferData& data : autofill_offer_data) {
    insert_offers.BindInt64(0, data.GetOfferId());
    insert_offers.BindString(1, data.GetOfferRewardAmount());
    insert_offers.BindInt64(
        2, data.GetExpiry().ToDeltaSinceWindowsEpoch().InMilliseconds());
    insert_offers.BindString(3, data.GetOfferDetailsUrl().spec());
    insert_offers.BindString(4, data.GetPromoCode());
    insert_offers.BindString(5, data.GetDisplayStrings().value_prop_text);
    insert_offers.BindString(6, data.GetDisplayStrings().see_details_text);
    insert_offers.BindString(7,
                             data.GetDisplayStrings().usage_instructions_text);
    insert_offers.Run();
    insert_offers.Reset(true);

    for (const int64_t instrument_id : data.GetEligibleInstrumentIds()) {
      // Insert new offer_eligible_instrument values.
      sql::Statement insert_offer_eligible_instruments;
      InsertBuilder(db_, insert_offer_eligible_instruments,
                    kOfferEligibleInstrumentTable, {kOfferId, kInstrumentId});
      insert_offer_eligible_instruments.BindInt64(0, data.GetOfferId());
      insert_offer_eligible_instruments.BindInt64(1, instrument_id);
      insert_offer_eligible_instruments.Run();
    }

    for (const GURL& merchant_origin : data.GetMerchantOrigins()) {
      // Insert new offer_merchant_domain values.
      sql::Statement insert_offer_merchant_domains;
      InsertBuilder(db_, insert_offer_merchant_domains,
                    kOfferMerchantDomainTable, {kOfferId, kMerchantDomain});
      insert_offer_merchant_domains.BindInt64(0, data.GetOfferId());
      insert_offer_merchant_domains.BindString(1, merchant_origin.spec());
      insert_offer_merchant_domains.Run();
    }
  }
  transaction.Commit();
}

bool AutofillTable::GetAutofillOffers(
    std::vector<std::unique_ptr<AutofillOfferData>>* autofill_offer_data) {
  autofill_offer_data->clear();

  sql::Statement s;
  SelectBuilder(
      db_, s, kOfferDataTable,
      {kOfferId, kOfferRewardAmount, kExpiry, kOfferDetailsUrl, kPromoCode,
       kValuePropText, kSeeDetailsText, kUsageInstructionsText});

  while (s.Step()) {
    int index = 0;
    int64_t offer_id = s.ColumnInt64(index++);
    std::string offer_reward_amount = s.ColumnString(index++);
    base::Time expiry = base::Time::FromDeltaSinceWindowsEpoch(
        base::Milliseconds(s.ColumnInt64(index++)));
    GURL offer_details_url = GURL(s.ColumnString(index++));
    std::string promo_code = s.ColumnString(index++);
    std::string value_prop_text = s.ColumnString(index++);
    std::string see_details_text = s.ColumnString(index++);
    std::string usage_instructions_text = s.ColumnString(index++);
    DisplayStrings display_strings = {value_prop_text, see_details_text,
                                      usage_instructions_text};
    std::vector<int64_t> eligible_instrument_id;
    std::vector<GURL> merchant_origins;

    sql::Statement s_offer_eligible_instrument;
    SelectBuilder(db_, s_offer_eligible_instrument,
                  kOfferEligibleInstrumentTable, {kOfferId, kInstrumentId},
                  "WHERE offer_id = ?");
    s_offer_eligible_instrument.BindInt64(0, offer_id);
    while (s_offer_eligible_instrument.Step()) {
      const int64_t instrument_id = s_offer_eligible_instrument.ColumnInt64(1);
      if (instrument_id != 0) {
        eligible_instrument_id.push_back(instrument_id);
      }
    }

    sql::Statement s_offer_merchant_domain;
    SelectBuilder(db_, s_offer_merchant_domain, kOfferMerchantDomainTable,
                  {kOfferId, kMerchantDomain}, "WHERE offer_id = ?");
    s_offer_merchant_domain.BindInt64(0, offer_id);
    while (s_offer_merchant_domain.Step()) {
      const std::string merchant_domain =
          s_offer_merchant_domain.ColumnString(1);
      if (!merchant_domain.empty()) {
        merchant_origins.emplace_back(merchant_domain);
      }
    }
    if (promo_code.empty()) {
      auto data = std::make_unique<AutofillOfferData>(
          AutofillOfferData::GPayCardLinkedOffer(
              offer_id, expiry, merchant_origins, offer_details_url,
              display_strings, eligible_instrument_id, offer_reward_amount));
      autofill_offer_data->emplace_back(std::move(data));
    } else {
      auto data = std::make_unique<AutofillOfferData>(
          AutofillOfferData::GPayPromoCodeOffer(
              offer_id, expiry, merchant_origins, offer_details_url,
              display_strings, promo_code));
      autofill_offer_data->emplace_back(std::move(data));
    }
  }

  return s.Succeeded();
}
void AutofillTable::SetVirtualCardUsageData(
    const std::vector<VirtualCardUsageData>& virtual_card_usage_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return;
  }

  // Delete old table.
  Delete(db_, kVirtualCardUsageDataTable);

  // Insert new values.
  sql::Statement insert_data;
  InsertBuilder(db_, insert_data, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});

  for (const VirtualCardUsageData& data : virtual_card_usage_data) {
    // usage_data_id should be consistent with the sync server logic.
    std::string usage_data_id = base::JoinString(
        {"VirtualCardUsageData",
         base::NumberToString(data.instrument_id.value()),
         data.merchant_app_package, data.merchant_origin.Serialize()},
        "|");
    insert_data.BindString(0, usage_data_id);
    insert_data.BindInt64(1, data.instrument_id.value());
    insert_data.BindString(2, data.merchant_origin.Serialize());
    insert_data.BindString(3, data.virtual_card_last_four.value());
    insert_data.Run();
    insert_data.Reset(true);
  }
  transaction.Commit();
}

bool AutofillTable::GetVirtualCardUsageData(
    std::vector<std::unique_ptr<VirtualCardUsageData>>*
        virtual_card_usage_data) {
  virtual_card_usage_data->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});

  while (s.Step()) {
    int index = 1;  // UsageDataId is unused.
    int64_t instrument_id = s.ColumnInt64(index++);
    std::string merchant_domain = s.ColumnString(index++);
    std::string last_four = s.ColumnString(index++);

    auto data = std::make_unique<VirtualCardUsageData>();
    data->instrument_id = VirtualCardUsageData::InstrumentId(instrument_id);
    data->virtual_card_last_four =
        VirtualCardUsageData::VirtualCardLastFour(last_four);
    data->merchant_origin = url::Origin::Create(GURL(merchant_domain));

    virtual_card_usage_data->push_back(std::move(data));
  }

  return s.Succeeded();
}

bool AutofillTable::InsertUpiId(const std::string& upi_id) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement insert_upi_id_statement;
  InsertBuilder(db_, insert_upi_id_statement, kPaymentsUpiVpaTable, {kVpa});
  insert_upi_id_statement.BindString(0, upi_id);
  insert_upi_id_statement.Run();

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

std::vector<std::string> AutofillTable::GetAllUpiIds() {
  sql::Statement select_upi_id_statement;
  SelectBuilder(db_, select_upi_id_statement, kPaymentsUpiVpaTable, {kVpa});

  std::vector<std::string> upi_ids;
  while (select_upi_id_statement.Step()) {
    upi_ids.push_back(select_upi_id_statement.ColumnString(0));
  }
  return upi_ids;
}

bool AutofillTable::ClearAllServerData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  bool changed = false;
  for (base::StringPiece table_name :
       {kMaskedCreditCardsTable, kUnmaskedCreditCardsTable,
        kServerAddressesTable, kServerCardMetadataTable,
        kServerAddressMetadataTable, kPaymentsCustomerDataTable,
        kServerCardCloudTokenDataTable, kOfferDataTable,
        kOfferEligibleInstrumentTable, kOfferMerchantDomainTable}) {
    Delete(db_, table_name);
    changed |= db_->GetLastChangeCount() > 0;
  }

  transaction.Commit();
  return changed;
}

bool AutofillTable::ClearAllLocalData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  ClearAutofillProfiles();
  bool changed = db_->GetLastChangeCount() > 0;
  ClearCreditCards();
  changed |= db_->GetLastChangeCount() > 0;

  transaction.Commit();
  return changed;
}

bool AutofillTable::RemoveAutofillDataModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles,
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

  // Remember Autofill profiles in the time range.
  sql::Statement s_profiles_get;
  SelectBetween(db_, s_profiles_get, kAutofillProfilesTable, {kGuid},
                kDateModified, delete_begin_t, delete_end_t);

  profiles->clear();
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfile(guid, AutofillProfile::Source::kLocalOrSyncable);
    if (!profile)
      return false;
    profiles->push_back(std::move(profile));
  }
  if (!s_profiles_get.Succeeded())
    return false;

  // Remove the profile pieces.
  for (const std::unique_ptr<AutofillProfile>& profile : *profiles) {
    if (!RemoveAutofillProfilePieces(profile->guid(), db_))
      return false;
  }

  // Remove Autofill profiles in the time range.
  sql::Statement s_profiles;
  DeleteBuilder(db_, s_profiles, kAutofillProfilesTable,
                "date_modified >= ? AND date_modified < ?");
  s_profiles.BindInt64(0, delete_begin_t);
  s_profiles.BindInt64(1, delete_end_t);

  if (!s_profiles.Run())
    return false;

  // Remember Autofill credit cards in the time range.
  sql::Statement s_credit_cards_get;
  SelectBetween(db_, s_credit_cards_get, kCreditCardsTable, {kGuid},
                kDateModified, delete_begin_t, delete_end_t);

  credit_cards->clear();
  while (s_credit_cards_get.Step()) {
    std::string guid = s_credit_cards_get.ColumnString(0);
    std::unique_ptr<CreditCard> credit_card = GetCreditCard(guid);
    if (!credit_card)
      return false;
    credit_cards->push_back(std::move(credit_card));
  }
  if (!s_credit_cards_get.Succeeded())
    return false;

  // Remove Autofill credit cards in the time range.
  sql::Statement s_credit_cards;
  DeleteBuilder(db_, s_credit_cards, kCreditCardsTable,
                "date_modified >= ? AND date_modified < ?");
  s_credit_cards.BindInt64(0, delete_begin_t);
  s_credit_cards.BindInt64(1, delete_end_t);
  if (!s_credit_cards.Run())
    return false;

  // Remove unmasked credit cards in the time range.
  sql::Statement s_unmasked_cards;
  DeleteBuilder(db_, s_unmasked_cards, kUnmaskedCreditCardsTable,
                "unmask_date >= ? AND unmask_date < ?");
  s_unmasked_cards.BindInt64(0, delete_begin.ToInternalValue());
  s_unmasked_cards.BindInt64(1, delete_end.ToInternalValue());
  return s_unmasked_cards.Run();
}

bool AutofillTable::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

  // Remember Autofill profiles with URL origins in the time range.
  sql::Statement s_profiles_get;
  SelectBetween(db_, s_profiles_get, kAutofillProfilesTable, {kGuid, kOrigin},
                kDateModified, delete_begin_t, delete_end_t);

  std::vector<std::string> profile_guids;
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    std::string origin = s_profiles_get.ColumnString(1);
    if (GURL(origin).is_valid())
      profile_guids.push_back(guid);
  }
  if (!s_profiles_get.Succeeded())
    return false;

  // Clear out the origins for the found Autofill profiles.
  for (const std::string& guid : profile_guids) {
    sql::Statement s_profile;
    UpdateBuilder(db_, s_profile, kAutofillProfilesTable, {kOrigin}, "guid=?");
    s_profile.BindString(0, "");
    s_profile.BindString(1, guid);
    if (!s_profile.Run())
      return false;

    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfile(guid, AutofillProfile::Source::kLocalOrSyncable);
    if (!profile)
      return false;

    profiles->push_back(std::move(profile));
  }

  // Remember Autofill credit cards with URL origins in the time range.
  sql::Statement s_credit_cards_get;
  SelectBetween(db_, s_credit_cards_get, kCreditCardsTable, {kGuid, kOrigin},
                kDateModified, delete_begin_t, delete_end_t);

  std::vector<std::string> credit_card_guids;
  while (s_credit_cards_get.Step()) {
    std::string guid = s_credit_cards_get.ColumnString(0);
    std::string origin = s_credit_cards_get.ColumnString(1);
    if (GURL(origin).is_valid())
      credit_card_guids.push_back(guid);
  }
  if (!s_credit_cards_get.Succeeded())
    return false;

  // Clear out the origins for the found credit cards.
  for (const std::string& guid : credit_card_guids) {
    sql::Statement s_credit_card;
    UpdateBuilder(db_, s_credit_card, kCreditCardsTable, {kOrigin}, "guid=?");
    s_credit_card.BindString(0, "");
    s_credit_card.BindString(1, guid);
    if (!s_credit_card.Run())
      return false;
  }

  return true;
}

bool AutofillTable::ClearAutofillProfiles() {
  return Delete(db_, kAutofillProfilesTable) &&
         Delete(db_, kAutofillProfileNamesTable) &&
         Delete(db_, kAutofillProfileEmailsTable) &&
         Delete(db_, kAutofillProfileAddressesTable) &&
         Delete(db_, kAutofillProfilePhonesTable) &&
         Delete(db_, kAutofillProfileBirthdatesTable);
}

bool AutofillTable::ClearCreditCards() {
  return Delete(db_, kCreditCardsTable);
}

bool AutofillTable::GetAllSyncMetadata(syncer::ModelType model_type,
                                       syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);
  if (!GetAllSyncEntityMetadata(model_type, metadata_batch)) {
    return false;
  }

  sync_pb::ModelTypeState model_type_state;
  if (!GetModelTypeState(model_type, &model_type_state))
    return false;

  metadata_batch->SetModelTypeState(model_type_state);
  return true;
}

bool AutofillTable::UpdateEntityMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  InsertBuilder(db_, s, kAutofillSyncMetadataTable,
                {kModelType, kStorageKey, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);
  s.BindString(2, metadata.SerializeAsString());

  return s.Run();
}

bool AutofillTable::ClearEntityMetadata(syncer::ModelType model_type,
                                        const std::string& storage_key) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db_, s, kAutofillSyncMetadataTable,
                "model_type=? AND storage_key=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);

  return s.Run();
}

bool AutofillTable::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  // Hardcode the id to force a collision, ensuring that there remains only a
  // single entry.
  sql::Statement s;
  InsertBuilder(db_, s, kAutofillModelTypeStateTable, {kModelType, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, model_type_state.SerializeAsString());

  return s.Run();
}

bool AutofillTable::ClearModelTypeState(syncer::ModelType model_type) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db_, s, kAutofillModelTypeStateTable, "model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  return s.Run();
}

bool AutofillTable::RemoveOrphanAutofillTableRows() {
  // Get all the orphan guids.
  std::set<std::string> orphan_guids;
  sql::Statement s_orphan_profile_pieces_get(db_->GetUniqueStatement(
      "SELECT guid FROM (SELECT guid FROM autofill_profile_names UNION SELECT "
      "guid FROM autofill_profile_emails UNION SELECT guid FROM "
      "autofill_profile_phones UNION SELECT guid FROM "
      "autofill_profile_addresses UNION SELECT guid FROM "
      "autofill_profile_birthdates) "
      "WHERE guid NOT IN (SELECT guid FROM "
      "autofill_profiles)"));

  // Put the orphan guids in a set.
  while (s_orphan_profile_pieces_get.Step())
    orphan_guids.insert(s_orphan_profile_pieces_get.ColumnString(0));

  if (!s_orphan_profile_pieces_get.Succeeded())
    return false;

  // Remove the profile pieces for the orphan guids.
  for (const std::string& guid : orphan_guids) {
    if (!RemoveAutofillProfilePieces(guid, db_))
      return false;
  }

  return true;
}

bool AutofillTable::MigrateToVersion83RemoveServerCardTypeColumn() {
  // Sqlite does not support "alter table drop column" syntax, so it has be done
  // manually.
  constexpr base::StringPiece kMaskedCreditCardsTempTable =
      "masked_credit_cards_temp";
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kMaskedCreditCardsTempTable,
                     {{kId, "VARCHAR"},
                      {kStatus, "VARCHAR"},
                      {kNameOnCard, "VARCHAR"},
                      {kNetwork, "VARCHAR"},
                      {kLastFour, "VARCHAR"},
                      {kExpMonth, "INTEGER DEFAULT 0"},
                      {kExpYear, "INTEGER DEFAULT 0"},
                      {kBankName, "VARCHAR"}}) &&
         db_->Execute(
             "INSERT INTO masked_credit_cards_temp "
             "SELECT id, status, name_on_card, network, last_four, exp_month,"
             "exp_year, bank_name "
             "FROM masked_credit_cards") &&
         DropTable(db_, kMaskedCreditCardsTable) &&
         RenameTable(db_, kMaskedCreditCardsTempTable,
                     kMaskedCreditCardsTable) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion84AddNicknameColumn() {
  // Add the nickname column to the masked_credit_cards table.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kNickname,
                              "VARCHAR");
}

bool AutofillTable::MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard() {
  // Add the new card_issuer column to the masked_credit_cards table and set the
  // default value to ISSUER_UNKNOWN.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kCardIssuer,
                              "INTEGER DEFAULT 0");
}

bool AutofillTable::MigrateToVersion88AddNewNameColumns() {
  for (base::StringPiece column : {kHonorificPrefix, kFirstLastName,
                                   kConjunctionLastName, kSecondLastName}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileNamesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (base::StringPiece column :
       {kHonorificPrefixStatus, kFirstNameStatus, kMiddleNameStatus,
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

bool AutofillTable::MigrateToVersion92AddNewPrefixedNameColumn() {
  return AddColumnIfNotExists(db_, kAutofillProfileNamesTable,
                              kFullNameWithHonorificPrefix, "VARCHAR") &&
         AddColumnIfNotExists(db_, kAutofillProfileNamesTable,
                              kFullNameWithHonorificPrefixStatus,
                              "INTEGER DEFAULT 0");
}

bool AutofillTable::MigrateToVersion86RemoveUnmaskedCreditCardsUseColumns() {
  // Sqlite does not support "alter table drop column" syntax, so it has be
  // done manually.
  constexpr base::StringPiece kUnmaskedCreditCardsTempTable =
      "unmasked_credit_cards_temp";
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kUnmaskedCreditCardsTempTable,
                     {{kId, "VARCHAR"},
                      {kCardNumberEncrypted, "VARCHAR"},
                      {kUnmaskDate, "INTEGER NOT NULL DEFAULT 0"}}) &&
         db_->Execute(
             "INSERT INTO unmasked_credit_cards_temp "
             "SELECT id, card_number_encrypted, unmask_date "
             "FROM unmasked_credit_cards") &&
         DropTable(db_, kUnmaskedCreditCardsTable) &&
         RenameTable(db_, kUnmaskedCreditCardsTempTable,
                     kUnmaskedCreditCardsTable) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion87AddCreditCardNicknameColumn() {
  // Add the nickname column to the credit_card table.
  return AddColumnIfNotExists(db_, kCreditCardsTable, kNickname, "VARCHAR");
}

bool AutofillTable::MigrateToVersion90AddNewStructuredAddressColumns() {
  if (!db_->DoesTableExist("autofill_profile_addresses"))
    InitProfileAddressesTable();

  for (base::StringPiece column : {kDependentLocality, kCity, kState, kZipCode,
                                   kSortingCode, kCountryCode}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (base::StringPiece column :
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

bool AutofillTable::MigrateToVersion91AddMoreStructuredAddressColumns() {
  if (!db_->DoesTableExist(kAutofillProfileAddressesTable))
    InitProfileAddressesTable();

  for (base::StringPiece column : {kApartmentNumber, kFloor}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (base::StringPiece column : {kApartmentNumberStatus, kFloorStatus}) {
    // The default value of 0 corresponds to the verification status
    // |kNoStatus|.
    if (!AddColumnIfNotExists(db_, kAutofillProfileAddressesTable, column,
                              "INTEGER DEFAULT 0")) {
      return false;
    }
  }
  return true;
}

bool AutofillTable::MigrateToVersion93AddAutofillProfileLabelColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable))
    InitProfileAddressesTable();

  return AddColumnIfNotExists(db_, kAutofillProfilesTable, kLabel, "VARCHAR");
}

bool AutofillTable::
    MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable))
    InitProfileAddressesTable();

  return AddColumnIfNotExists(db_, kAutofillProfilesTable,
                              kDisallowSettingsVisibleUpdates,
                              "INTEGER NOT NULL DEFAULT 0");
}

bool AutofillTable::
    MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard() {
  // Add the new instrument_id column to the masked_credit_cards table and set
  // the default value to 0.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kInstrumentId,
                              "INTEGER DEFAULT 0");
}

bool AutofillTable::MigrateToVersion94AddPromoCodeColumnsToOfferData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesTableExist(kOfferDataTable))
    InitOfferDataTable();

  // Add the new promo_code and DisplayStrings text columns to the offer_data
  // table.
  for (base::StringPiece column :
       {kPromoCode, kValuePropText, kSeeDetailsText, kUsageInstructionsText}) {
    if (!AddColumnIfNotExists(db_, kOfferDataTable, column, "VARCHAR")) {
      return false;
    }
  }
  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion95AddVirtualCardMetadata() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesTableExist(kMaskedCreditCardsTable))
    InitMaskedCreditCardsTable();

  // Add virtual_card_enrollment_state to masked_credit_cards.
  if (!AddColumnIfNotExists(db_, kMaskedCreditCardsTable,
                            kVirtualCardEnrollmentState, "INTEGER DEFAULT 0")) {
    return false;
  }

  // Add card_art_url to masked_credit_cards.
  if (!AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kCardArtUrl,
                            "VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion98RemoveStatusColumnMaskedCreditCards() {
  // Sqlite does not support "alter table drop column" syntax, so it has be done
  // manually.
  constexpr base::StringPiece kMaskedCreditCardsTempTable =
      "masked_credit_cards_temp";
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kMaskedCreditCardsTempTable,
                     {{kId, "VARCHAR"},
                      {kNameOnCard, "VARCHAR"},
                      {kNetwork, "VARCHAR"},
                      {kLastFour, "VARCHAR"},
                      {kExpMonth, "INTEGER DEFAULT 0"},
                      {kExpYear, "INTEGER DEFAULT 0"},
                      {kBankName, "VARCHAR"},
                      {kNickname, "VARCHAR"},
                      {kCardIssuer, "INTEGER DEFAULT 0"},
                      {kInstrumentId, "INTEGER DEFAULT 0"},
                      {kVirtualCardEnrollmentState, "INTEGER DEFAULT 0"},
                      {kCardArtUrl, "VARCHAR"}}) &&
         db_->Execute(
             "INSERT INTO masked_credit_cards_temp "
             "SELECT id, name_on_card, network, last_four, exp_month, "
             "exp_year, bank_name, nickname, card_issuer, instrument_id, "
             "virtual_card_enrollment_state, card_art_url "
             "FROM masked_credit_cards") &&
         DropTable(db_, kMaskedCreditCardsTable) &&
         RenameTable(db_, kMaskedCreditCardsTempTable,
                     kMaskedCreditCardsTable) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion99RemoveAutofillProfilesTrashTable() {
  return DropTable(db_, "autofill_profiles_trash");
}

bool AutofillTable::MigrateToVersion100RemoveProfileValidityBitfieldColumn() {
  // Sqlite does not support "alter table drop column" syntax, so it has be done
  // manually.
  sql::Transaction transaction(db_);

  return transaction.Begin() &&
         CreateTable(db_, "autofill_profiles_tmp",
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kCompanyName, "VARCHAR"},
                      {kStreetAddress, "VARCHAR"},
                      {kDependentLocality, "VARCHAR"},
                      {kCity, "VARCHAR"},
                      {kState, "VARCHAR"},
                      {kZipcode, "VARCHAR"},
                      {kSortingCode, "VARCHAR"},
                      {kCountryCode, "VARCHAR"},
                      {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                      {kOrigin, "VARCHAR DEFAULT ''"},
                      {kLanguageCode, "VARCHAR"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kLabel, "VARCHAR"},
                      {kDisallowSettingsVisibleUpdates,
                       "INTEGER NOT NULL DEFAULT 0"}}) &&
         db_->Execute(
             "INSERT INTO autofill_profiles_tmp "
             "SELECT guid, company_name, street_address, dependent_locality, "
             "city, state, zipcode, sorting_code, country_code, date_modified, "
             "origin, language_code, use_count, use_date, label, "
             "disallow_settings_visible_updates "
             " FROM autofill_profiles") &&
         DropTable(db_, kAutofillProfilesTable) &&
         RenameTable(db_, "autofill_profiles_tmp", kAutofillProfilesTable) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion101RemoveCreditCardArtImageTable() {
  return db_->Execute("DROP TABLE IF EXISTS credit_card_art_images");
}

bool AutofillTable::MigrateToVersion102AddAutofillBirthdatesTable() {
  return CreateTable(db_, kAutofillProfileBirthdatesTable,
                     {{kGuid, "VARCHAR"},
                      {kDay, "INTEGER DEFAULT 0"},
                      {kMonth, "INTEGER DEFAULT 0"},
                      {kYear, "INTEGER DEFAULT 0"}});
}

bool AutofillTable::MigrateToVersion104AddProductDescriptionColumn() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesTableExist(kMaskedCreditCardsTable))
    InitMaskedCreditCardsTable();

  // Add product_description to masked_credit_cards.
  if (!AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kProductDescription,
                            "VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion105AddAutofillIBANTable() {
  return CreateTable(db_, kIBANsTable,
                     {{kGuid, "VARCHAR"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}});
}

bool AutofillTable::MigrateToVersion106RecreateAutofillIBANTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTable(db_, kIBANsTable) &&
         CreateTable(db_, kIBANsTable,
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}}) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion107AddContactInfoTables() {
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

bool AutofillTable::MigrateToVersion108AddCardIssuerIdColumn() {
  // Add card_issuer_id to masked_credit_cards.
  return db_->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kCardIssuerId,
                              "VARCHAR");
}

bool AutofillTable::MigrateToVersion109AddVirtualCardUsageDataTable() {
  return CreateTable(db_, kVirtualCardUsageDataTable,
                     {{kId, "VARCHAR PRIMARY KEY"},
                      {kInstrumentId, "INTEGER DEFAULT 0"},
                      {kMerchantDomain, "VARCHAR"},
                      {kLastFour, "VARCHAR"}});
}

bool AutofillTable::MigrateToVersion110AddInitialCreatorIdAndLastModifierId() {
  sql::Transaction transaction(db_);
  return db_->DoesTableExist(kContactInfoTable) && transaction.Begin() &&
         AddColumnIfNotExists(db_, kContactInfoTable, kInitialCreatorId,
                              "INTEGER DEFAULT 0") &&
         AddColumnIfNotExists(db_, kContactInfoTable, kLastModifierId,
                              "INTEGER DEFAULT 0") &&
         transaction.Commit();
}

bool AutofillTable::AddFormFieldValuesTime(
    const std::vector<FormFieldData>& elements,
    std::vector<AutofillChange>* changes,
    base::Time time) {
  // Only add one new entry for each unique element name.  Use |seen_names|
  // to track this.  Add up to |kMaximumUniqueNames| unique entries per
  // form.
  const size_t kMaximumUniqueNames = 256;
  std::set<std::u16string> seen_names;
  bool result = true;
  for (const FormFieldData& element : elements) {
    if (seen_names.size() >= kMaximumUniqueNames)
      break;
    if (base::Contains(seen_names, element.name))
      continue;
    result = result && AddFormFieldValueTime(element, changes, time);
    seen_names.insert(element.name);
  }
  return result;
}

bool AutofillTable::AddFormFieldValueTime(const FormFieldData& element,
                                          std::vector<AutofillChange>* changes,
                                          base::Time time) {
  sql::Statement s_exists(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM autofill WHERE name = ? AND value = ?"));
  s_exists.BindString16(0, element.name);
  s_exists.BindString16(1, element.value);
  if (!s_exists.Step())
    return false;

  bool already_exists = s_exists.ColumnInt(0) > 0;
  if (already_exists) {
    sql::Statement s(db_->GetUniqueStatement(
        "UPDATE autofill SET date_last_used = ?, count = count + 1 "
        "WHERE name = ? AND value = ?"));
    s.BindInt64(0, time.ToTimeT());
    s.BindString16(1, element.name);
    s.BindString16(2, element.value);
    if (!s.Run())
      return false;
  } else {
    time_t time_as_time_t = time.ToTimeT();
    sql::Statement s;
    InsertBuilder(
        db_, s, kAutofillTable,
        {kName, kValue, kValueLower, kDateCreated, kDateLastUsed, kCount});
    s.BindString16(0, element.name);
    s.BindString16(1, element.value);
    s.BindString16(2, base::i18n::ToLower(element.value));
    s.BindInt64(3, time_as_time_t);
    s.BindInt64(4, time_as_time_t);
    s.BindInt(5, 1);
    if (!s.Run())
      return false;
  }

  AutofillChange::Type change_type =
      already_exists ? AutofillChange::UPDATE : AutofillChange::ADD;
  changes->push_back(
      AutofillChange(change_type, AutofillKey(element.name, element.value)));
  return true;
}

bool AutofillTable::SupportsMetadataForModelType(
    syncer::ModelType model_type) const {
  return (model_type == syncer::AUTOFILL ||
          model_type == syncer::AUTOFILL_PROFILE ||
          model_type == syncer::AUTOFILL_WALLET_DATA ||
          model_type == syncer::AUTOFILL_WALLET_METADATA ||
          model_type == syncer::AUTOFILL_WALLET_OFFER ||
          model_type == syncer::AUTOFILL_WALLET_USAGE ||
          model_type == syncer::CONTACT_INFO);
}

int AutofillTable::GetKeyValueForModelType(syncer::ModelType model_type) const {
  return syncer::ModelTypeToStableIdentifier(model_type);
}

bool AutofillTable::GetAllSyncEntityMetadata(
    syncer::ModelType model_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillSyncMetadataTable, {kStorageKey, kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize AUTOFILL model type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
  }
  return true;
}

bool AutofillTable::GetModelTypeState(syncer::ModelType model_type,
                                      sync_pb::ModelTypeState* state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillModelTypeStateTable, {kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  if (!s.Step()) {
    return true;
  }

  std::string serialized_state = s.ColumnString(0);
  return state->ParseFromString(serialized_state);
}

bool AutofillTable::InsertAutofillEntry(const AutofillEntry& entry) {
  sql::Statement s;
  InsertBuilder(
      db_, s, kAutofillTable,
      {kName, kValue, kValueLower, kDateCreated, kDateLastUsed, kCount});
  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, base::i18n::ToLower(entry.key().value()));
  s.BindInt64(3, entry.date_created().ToTimeT());
  s.BindInt64(4, entry.date_last_used().ToTimeT());
  // TODO(isherman): The counts column is currently synced implicitly as the
  // number of timestamps.  Sync the value explicitly instead, since the DB
  // now only saves the first and last timestamp, which makes counting
  // timestamps completely meaningless as a way to track frequency of usage.
  s.BindInt(5, entry.date_last_used() == entry.date_created() ? 1 : 2);
  return s.Run();
}

void AutofillTable::AddMaskedCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK_GT(db_->transaction_nesting(), 0);
  sql::Statement masked_insert;
  InsertBuilder(
      db_, masked_insert, kMaskedCreditCardsTable,
      {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear, kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kCardArtUrl, kProductDescription});

  int index;
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
    index = 0;
    masked_insert.BindString(index++, card.server_id());
    masked_insert.BindString(index++, card.network());
    masked_insert.BindString16(index++, card.GetRawInfo(CREDIT_CARD_NAME_FULL));
    masked_insert.BindString16(index++, card.LastFourDigits());
    masked_insert.BindString16(index++, card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
    masked_insert.BindString16(index++,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
    masked_insert.BindString(index++, card.bank_name());
    masked_insert.BindString16(index++, card.nickname());
    masked_insert.BindInt(index++, static_cast<int>(card.card_issuer()));
    masked_insert.BindString(index++, card.issuer_id());
    masked_insert.BindInt64(index++, card.instrument_id());
    masked_insert.BindInt(index++, card.virtual_card_enrollment_state());
    masked_insert.BindString(index++, card.card_art_url().spec());
    masked_insert.BindString16(index++, card.product_description());
    masked_insert.Run();
    masked_insert.Reset(true);

    // Save the use count and use date of the card.
    UpdateServerCardMetadata(card);
  }
}

void AutofillTable::AddUnmaskedCreditCard(const std::string& id,
                                          const std::u16string& full_number) {
  sql::Statement s;
  InsertBuilder(db_, s, kUnmaskedCreditCardsTable,
                {kId, kCardNumberEncrypted, kUnmaskDate});
  s.BindString(0, id);

  std::string encrypted_data;
  autofill_table_encryptor_->EncryptString16(full_number, &encrypted_data);
  s.BindBlob(1, encrypted_data);
  s.BindInt64(2, AutofillClock::Now().ToInternalValue());  // unmask_date

  s.Run();
}

bool AutofillTable::DeleteFromMaskedCreditCards(const std::string& id) {
  DeleteWhereColumnEq(db_, kMaskedCreditCardsTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::DeleteFromUnmaskedCreditCards(const std::string& id) {
  DeleteWhereColumnEq(db_, kUnmaskedCreditCardsTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::InitMainTable() {
  if (!db_->DoesTableExist(kAutofillTable)) {
    return CreateTable(db_, kAutofillTable,
                       {{kName, "VARCHAR"},
                        {kValue, "VARCHAR"},
                        {kValueLower, "VARCHAR"},
                        {kDateCreated, "INTEGER DEFAULT 0"},
                        {kDateLastUsed, "INTEGER DEFAULT 0"},
                        {kCount, "INTEGER DEFAULT 1"}},
                       {kName, kValue}) &&
           CreateIndex(db_, kAutofillTable, {kName}) &&
           CreateIndex(db_, kAutofillTable, {kName, kValueLower});
  }
  return true;
}

bool AutofillTable::InitCreditCardsTable() {
  return CreateTableIfNotExists(db_, kCreditCardsTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kNameOnCard, "VARCHAR"},
                                 {kExpirationMonth, "INTEGER"},
                                 {kExpirationYear, "INTEGER"},
                                 {kCardNumberEncrypted, "BLOB"},
                                 {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                                 {kOrigin, "VARCHAR DEFAULT ''"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kBillingAddressId, "VARCHAR"},
                                 {kNickname, "VARCHAR"}});
}

bool AutofillTable::InitIBANsTable() {
  return CreateTableIfNotExists(db_, kIBANsTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kValue, "VARCHAR"},
                                 {kNickname, "VARCHAR"}});
}

bool AutofillTable::InitProfilesTable() {
  return CreateTableIfNotExists(
      db_, kAutofillProfilesTable,
      {{kGuid, "VARCHAR PRIMARY KEY"},
       {kCompanyName, "VARCHAR"},
       {kStreetAddress, "VARCHAR"},
       {kDependentLocality, "VARCHAR"},
       {kCity, "VARCHAR"},
       {kState, "VARCHAR"},
       {kZipcode, "VARCHAR"},
       {kSortingCode, "VARCHAR"},
       {kCountryCode, "VARCHAR"},
       {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
       {kOrigin, "VARCHAR DEFAULT ''"},
       {kLanguageCode, "VARCHAR"},
       {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
       {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
       {kLabel, "VARCHAR"},
       {kDisallowSettingsVisibleUpdates, "INTEGER NOT NULL DEFAULT 0"}});
}

bool AutofillTable::InitProfileNamesTable() {
  // The default value of 0 corresponds to the verification status
  // |kNoStatus|.
  return CreateTableIfNotExists(
      db_, kAutofillProfileNamesTable,
      {{kGuid, "VARCHAR"},
       {kFirstName, "VARCHAR"},
       {kMiddleName, "VARCHAR"},
       {kLastName, "VARCHAR"},
       {kFullName, "VARCHAR"},
       {kHonorificPrefix, "VARCHAR"},
       {kFirstLastName, "VARCHAR"},
       {kConjunctionLastName, "VARCHAR"},
       {kSecondLastName, "VARCHAR"},
       {kHonorificPrefixStatus, "INTEGER DEFAULT 0"},
       {kFirstNameStatus, "INTEGER DEFAULT 0"},
       {kMiddleNameStatus, "INTEGER DEFAULT 0"},
       {kLastNameStatus, "INTEGER DEFAULT 0"},
       {kFirstLastNameStatus, "INTEGER DEFAULT 0"},
       {kConjunctionLastNameStatus, "INTEGER DEFAULT 0"},
       {kSecondLastNameStatus, "INTEGER DEFAULT 0"},
       {kFullNameStatus, "INTEGER DEFAULT 0"},
       {kFullNameWithHonorificPrefix, "VARCHAR"},
       {kFullNameWithHonorificPrefixStatus, "INTEGER DEFAULT 0"}});
}

bool AutofillTable::InitProfileAddressesTable() {
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
       {kPremiseName, "VARCHAR"},
       {kStreetAddressStatus, "INTEGER DEFAULT 0"},
       {kStreetNameStatus, "INTEGER DEFAULT 0"},
       {kDependentStreetNameStatus, "INTEGER DEFAULT 0"},
       {kHouseNumberStatus, "INTEGER DEFAULT 0"},
       {kSubpremiseStatus, "INTEGER DEFAULT 0"},
       {kPremiseNameStatus, "INTEGER DEFAULT 0"},
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

bool AutofillTable::InitProfileEmailsTable() {
  return CreateTableIfNotExists(db_, kAutofillProfileEmailsTable,
                                {{kGuid, "VARCHAR"}, {kEmail, "VARCHAR"}});
}

bool AutofillTable::InitProfilePhonesTable() {
  return CreateTableIfNotExists(db_, kAutofillProfilePhonesTable,
                                {{kGuid, "VARCHAR"}, {kNumber, "VARCHAR"}});
}

bool AutofillTable::InitProfileBirthdatesTable() {
  return CreateTableIfNotExists(db_, kAutofillProfileBirthdatesTable,
                                {{kGuid, "VARCHAR"},
                                 {kDay, "INTEGER DEFAULT 0"},
                                 {kMonth, "INTEGER DEFAULT 0"},
                                 {kYear, "INTEGER DEFAULT 0"}});
}

bool AutofillTable::InitMaskedCreditCardsTable() {
  return CreateTableIfNotExists(
      db_, kMaskedCreditCardsTable,
      {{kId, "VARCHAR"},
       {kNameOnCard, "VARCHAR"},
       {kNetwork, "VARCHAR"},
       {kLastFour, "VARCHAR"},
       {kExpMonth, "INTEGER DEFAULT 0"},
       {kExpYear, "INTEGER DEFAULT 0"},
       {kBankName, "VARCHAR"},
       {kNickname, "VARCHAR"},
       {kCardIssuer, "INTEGER DEFAULT 0"},
       {kInstrumentId, "INTEGER DEFAULT 0"},
       {kVirtualCardEnrollmentState, "INTEGER DEFAULT 0"},
       {kCardArtUrl, "VARCHAR"},
       {kProductDescription, "VARCHAR"},
       {kCardIssuerId, "VARCHAR"}});
}

bool AutofillTable::InitUnmaskedCreditCardsTable() {
  return CreateTableIfNotExists(db_, kUnmaskedCreditCardsTable,
                                {{kId, "VARCHAR"},
                                 {kCardNumberEncrypted, "VARCHAR"},
                                 {kUnmaskDate, "INTEGER NOT NULL DEFAULT 0"}});
}

bool AutofillTable::InitServerCardMetadataTable() {
  return CreateTableIfNotExists(db_, kServerCardMetadataTable,
                                {{kId, "VARCHAR NOT NULL"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kBillingAddressId, "VARCHAR"}});
}

bool AutofillTable::InitServerAddressesTable() {
  return CreateTableIfNotExists(db_, kServerAddressesTable,
                                {{kId, "VARCHAR"},
                                 {kCompanyName, "VARCHAR"},
                                 {kStreetAddress, "VARCHAR"},
                                 {kAddress1, "VARCHAR"},
                                 {kAddress2, "VARCHAR"},
                                 {kAddress3, "VARCHAR"},
                                 {kAddress4, "VARCHAR"},
                                 {kPostalCode, "VARCHAR"},
                                 {kSortingCode, "VARCHAR"},
                                 {kCountryCode, "VARCHAR"},
                                 {kLanguageCode, "VARCHAR"},
                                 {kRecipientName, "VARCHAR"},
                                 {kPhoneNumber, "VARCHAR"}});
}

bool AutofillTable::InitServerAddressMetadataTable() {
  return CreateTableIfNotExists(
      db_, kServerAddressMetadataTable,
      {{kId, "VARCHAR NOT NULL"},
       {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
       {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
       {kHasConverted, "BOOL NOT NULL DEFAULT FALSE"}});
}

bool AutofillTable::InitAutofillSyncMetadataTable() {
  return CreateTableIfNotExists(db_, kAutofillSyncMetadataTable,
                                {{kModelType, "INTEGER NOT NULL"},
                                 {kStorageKey, "VARCHAR NOT NULL"},
                                 {kValue, "BLOB"}},
                                {kModelType, kStorageKey});
}

bool AutofillTable::InitModelTypeStateTable() {
  return CreateTableIfNotExists(
      db_, kAutofillModelTypeStateTable,
      {{kModelType, "INTEGER NOT NULL PRIMARY KEY"}, {kValue, "BLOB"}});
}

bool AutofillTable::InitPaymentsCustomerDataTable() {
  return CreateTableIfNotExists(db_, kPaymentsCustomerDataTable,
                                {{kCustomerId, "VARCHAR"}});
}

bool AutofillTable::InitPaymentsUPIVPATable() {
  return CreateTableIfNotExists(db_, kPaymentsUpiVpaTable, {{kVpa, "VARCHAR"}});
}

bool AutofillTable::InitServerCreditCardCloudTokenDataTable() {
  return CreateTableIfNotExists(db_, kServerCardCloudTokenDataTable,
                                {{kId, "VARCHAR"},
                                 {kSuffix, "VARCHAR"},
                                 {kExpMonth, "INTEGER DEFAULT 0"},
                                 {kExpYear, "INTEGER DEFAULT 0"},
                                 {kCardArtUrl, "VARCHAR"},
                                 {kInstrumentToken, "VARCHAR"}});
}

bool AutofillTable::InitOfferDataTable() {
  return CreateTableIfNotExists(db_, kOfferDataTable,
                                {{kOfferId, "UNSIGNED LONG"},
                                 {kOfferRewardAmount, "VARCHAR"},
                                 {kExpiry, "UNSIGNED LONG"},
                                 {kOfferDetailsUrl, "VARCHAR"},
                                 {kMerchantDomain, "VARCHAR"},
                                 {kPromoCode, "VARCHAR"},
                                 {kValuePropText, "VARCHAR"},
                                 {kSeeDetailsText, "VARCHAR"},
                                 {kUsageInstructionsText, "VARCHAR"}});
}

bool AutofillTable::InitOfferEligibleInstrumentTable() {
  return CreateTableIfNotExists(
      db_, kOfferEligibleInstrumentTable,
      {{kOfferId, "UNSIGNED LONG"}, {kInstrumentId, "UNSIGNED LONG"}});
}

bool AutofillTable::InitOfferMerchantDomainTable() {
  return CreateTableIfNotExists(
      db_, kOfferMerchantDomainTable,
      {{kOfferId, "UNSIGNED LONG"}, {kMerchantDomain, "VARCHAR"}});
}

bool AutofillTable::InitContactInfoTable() {
  return CreateTableIfNotExists(db_, kContactInfoTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kDateModified, "INTEGER NOT NULL DEFAULT 0"},
                                 {kLanguageCode, "VARCHAR"},
                                 {kLabel, "VARCHAR"},
                                 {kInitialCreatorId, "INTEGER DEFAULT 0"},
                                 {kLastModifierId, "INTEGER DEFAULT 0"}});
}

bool AutofillTable::InitContactInfoTypeTokensTable() {
  return CreateTableIfNotExists(db_, kContactInfoTypeTokensTable,
                                {{kGuid, "VARCHAR"},
                                 {kType, "INTEGER"},
                                 {kValue, "VARCHAR"},
                                 {kVerificationStatus, "INTEGER DEFAULT 0"}},
                                /*composite_primary_key=*/{kGuid, kType});
}

bool AutofillTable::InitVirtualCardUsageDataTable() {
  return CreateTableIfNotExists(db_, kVirtualCardUsageDataTable,
                                {{kId, "VARCHAR PRIMARY KEY"},
                                 {kInstrumentId, "INTEGER DEFAULT 0"},
                                 {kMerchantDomain, "VARCHAR"},
                                 {kLastFour, "VARCHAR"}});
}

}  // namespace autofill
