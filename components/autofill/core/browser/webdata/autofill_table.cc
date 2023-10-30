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
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor_factory.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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

constexpr std::string_view kAutofillTable = "autofill";
constexpr std::string_view kName = "name";
constexpr std::string_view kValue = "value";
constexpr std::string_view kValueLower = "value_lower";
constexpr std::string_view kDateCreated = "date_created";
constexpr std::string_view kDateLastUsed = "date_last_used";
constexpr std::string_view kCount = "count";

constexpr std::string_view kAutofillProfilesTable = "autofill_profiles";
constexpr std::string_view kGuid = "guid";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kCompanyName = "company_name";
constexpr std::string_view kStreetAddress = "street_address";
constexpr std::string_view kDependentLocality = "dependent_locality";
constexpr std::string_view kCity = "city";
constexpr std::string_view kState = "state";
constexpr std::string_view kZipcode = "zipcode";
constexpr std::string_view kSortingCode = "sorting_code";
constexpr std::string_view kCountryCode = "country_code";
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kUseDate = "use_date";
constexpr std::string_view kDateModified = "date_modified";
constexpr std::string_view kOrigin = "origin";
constexpr std::string_view kLanguageCode = "language_code";
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
constexpr std::string_view kHonorificPrefix = "honorific_prefix";
constexpr std::string_view kFirstName = "first_name";
constexpr std::string_view kMiddleName = "middle_name";
constexpr std::string_view kLastName = "last_name";
constexpr std::string_view kFirstLastName = "first_last_name";
constexpr std::string_view kConjunctionLastName = "conjunction_last_name";
constexpr std::string_view kSecondLastName = "second_last_name";
constexpr std::string_view kFullName = "full_name";
constexpr std::string_view kFullNameWithHonorificPrefix =
    "full_name_with_honorific_prefix";
constexpr std::string_view kHonorificPrefixStatus = "honorific_prefix_status";
constexpr std::string_view kFirstNameStatus = "first_name_status";
constexpr std::string_view kMiddleNameStatus = "middle_name_status";
constexpr std::string_view kLastNameStatus = "last_name_status";
constexpr std::string_view kFirstLastNameStatus = "first_last_name_status";
constexpr std::string_view kConjunctionLastNameStatus =
    "conjunction_last_name_status";
constexpr std::string_view kSecondLastNameStatus = "second_last_name_status";
constexpr std::string_view kFullNameStatus = "full_name_status";
constexpr std::string_view kFullNameWithHonorificPrefixStatus =
    "full_name_with_honorific_prefix_status";

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

constexpr std::string_view kCreditCardsTable = "credit_cards";
// kGuid = "guid"
constexpr std::string_view kNameOnCard = "name_on_card";
constexpr std::string_view kExpirationMonth = "expiration_month";
constexpr std::string_view kExpirationYear = "expiration_year";
constexpr std::string_view kCardNumberEncrypted = "card_number_encrypted";
// kUseCount = "use_count"
// kUseDate = "use_date"
// kDateModified = "date_modified"
// kOrigin = "origin"
constexpr std::string_view kBillingAddressId = "billing_address_id";
constexpr std::string_view kNickname = "nickname";

constexpr std::string_view kMaskedCreditCardsTable = "masked_credit_cards";
constexpr std::string_view kId = "id";
constexpr std::string_view kStatus = "status";
// kNameOnCard = "name_on_card"
constexpr std::string_view kNetwork = "network";
constexpr std::string_view kLastFour = "last_four";
constexpr std::string_view kExpMonth = "exp_month";
constexpr std::string_view kExpYear = "exp_year";
constexpr std::string_view kBankName = "bank_name";
// kNickname = "nickname"
constexpr std::string_view kCardIssuer = "card_issuer";
constexpr std::string_view kCardIssuerId = "card_issuer_id";
constexpr std::string_view kInstrumentId = "instrument_id";
constexpr std::string_view kVirtualCardEnrollmentState =
    "virtual_card_enrollment_state";
constexpr std::string_view kVirtualCardEnrollmentType =
    "virtual_card_enrollment_type";
constexpr std::string_view kCardArtUrl = "card_art_url";
constexpr std::string_view kProductDescription = "product_description";

constexpr std::string_view kUnmaskedCreditCardsTable = "unmasked_credit_cards";
// kId = "id"
// kCardNumberEncrypted = "card_number_encrypted"
constexpr std::string_view kUnmaskDate = "unmask_date";

constexpr std::string_view kServerCardCloudTokenDataTable =
    "server_card_cloud_token_data";
// kId = "id"
constexpr std::string_view kSuffix = "suffix";
// kExpMonth = "exp_month"
// kExpYear = "exp_year"
// kCardArtUrl = "card_art_url"
constexpr std::string_view kInstrumentToken = "instrument_token";

constexpr std::string_view kServerCardMetadataTable = "server_card_metadata";
// kId = "id"
// kUseCount = "use_count"
// kUseDate = "use_date"
// kBillingAddressId = "billing_address_id"

// This shouldn't be used in new code, and it only exists for the purposes of
// migration logic. It has renamed to `local_ibans`.
constexpr std::string_view kIbansTable = "ibans";
constexpr std::string_view kLocalIbansTable = "local_ibans";
// kGuid = "guid"
// kUseCount = "use_count"
// kUseDate = "use_date"
constexpr std::string_view kValueEncrypted = "value_encrypted";
// kNickname = "nickname"

constexpr std::string_view kMaskedIbansTable = "masked_ibans";
// kInstrumentId = "instrument_id"
constexpr std::string_view kPrefix = "prefix";
// kSuffix = "suffix";
constexpr std::string_view kLength = "length";
// kNickname = "nickname"

constexpr std::string_view kMaskedIbansMetadataTable = "masked_ibans_metadata";
// kInstrumentId = "instrument_id"
// kUseCount = "use_count"
// kUseDate = "use_date"

constexpr std::string_view kServerAddressesTable = "server_addresses";
// kId = "id"
constexpr std::string_view kRecipientName = "recipient_name";
// kCompanyName = "company_name"
// kStreetAddress = "street_address"
constexpr std::string_view kAddress1 = "address_1";
constexpr std::string_view kAddress2 = "address_2";
constexpr std::string_view kAddress3 = "address_3";
constexpr std::string_view kAddress4 = "address_4";
constexpr std::string_view kPostalCode = "postal_code";
// kSortingCode = "sorting_code"
// kCountryCode = "country_code"
// kLanguageCode = "language_code"
constexpr std::string_view kPhoneNumber = "phone_number";

constexpr std::string_view kServerAddressMetadataTable =
    "server_address_metadata";
// kId = "id"
// kUseCount = "use_count"
// kUseDate = "use_date"
constexpr std::string_view kHasConverted = "has_converted";

constexpr std::string_view kAutofillSyncMetadataTable =
    "autofill_sync_metadata";
constexpr std::string_view kModelType = "model_type";
constexpr std::string_view kStorageKey = "storage_key";
// kValue = "value"

constexpr std::string_view kAutofillModelTypeStateTable =
    "autofill_model_type_state";
// kModelType = "model_type"
// kValue = "value"

constexpr std::string_view kPaymentsCustomerDataTable =
    "payments_customer_data";
constexpr std::string_view kCustomerId = "customer_id";

constexpr std::string_view kPaymentsUpiVpaTable = "payments_upi_vpa";

constexpr std::string_view kOfferDataTable = "offer_data";
constexpr std::string_view kOfferId = "offer_id";
constexpr std::string_view kOfferRewardAmount = "offer_reward_amount";
constexpr std::string_view kExpiry = "expiry";
constexpr std::string_view kOfferDetailsUrl = "offer_details_url";
constexpr std::string_view kPromoCode = "promo_code";
constexpr std::string_view kValuePropText = "value_prop_text";
constexpr std::string_view kSeeDetailsText = "see_details_text";
constexpr std::string_view kUsageInstructionsText = "usage_instructions_text";

constexpr std::string_view kOfferEligibleInstrumentTable =
    "offer_eligible_instrument";
// kOfferId = "offer_id"
// kInstrumentId = "instrument_id"

constexpr std::string_view kOfferMerchantDomainTable = "offer_merchant_domain";
// kOfferId = "offer_id"
constexpr std::string_view kMerchantDomain = "merchant_domain";

constexpr std::string_view kContactInfoTable = "contact_info";
constexpr std::string_view kLocalAddressesTable = "local_addresses";
// kGuid = "guid"
// kUseCount = "use_count"
// kUseDate = "use_date"
// kDateModified = "date_modified"
// kLanguageCode = "language_code"
// kLabel = "label"
constexpr std::string_view kInitialCreatorId = "initial_creator_id";
constexpr std::string_view kLastModifierId = "last_modifier_id";

constexpr std::string_view kContactInfoTypeTokensTable =
    "contact_info_type_tokens";
constexpr std::string_view kLocalAddressesTypeTokensTable =
    "local_addresses_type_tokens";
// kGuid = "guid"
constexpr std::string_view kType = "type";
// kValue = "value"
constexpr std::string_view kVerificationStatus = "verification_status";
constexpr std::string_view kObservations = "observations";

constexpr std::string_view kVirtualCardUsageDataTable =
    "virtual_card_usage_data";
// kId = "id"
// kInstrumentId = "instrument_id"
// kMerchantDomain = "merchant_domain"
// kLastFour = "last_four"

constexpr std::string_view kLocalStoredCvcTable = "local_stored_cvc";
// kGuid = "guid"
// kValueEncrypted = "value_encrypted"
constexpr std::string_view kLastUpdatedTimestamp = "last_updated_timestamp";

constexpr std::string_view kServerStoredCvcTable = "server_stored_cvc";
// kInstrumentId = "instrument_id"
// kValueEncrypted = "value_encrypted"
// kLastUpdatedTimestamp = "last_updated_timestamp"

constexpr std::string_view kPaymentInstrumentsTable = "payment_instruments";
// kInstrumentId = "instrument_id"
constexpr std::string_view kInstrumentType = "instrument_type";
// kNickname = "nickname"
constexpr std::string_view kDisplayIconUrl = "display_icon_url";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kPaymentInstrumentsColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER NOT NULL"},
        {kInstrumentType, "INTEGER NOT NULL"},
        {kDisplayIconUrl, "VARCHAR"},
        {kNickname, "VARCHAR"}};
constexpr std::initializer_list<std::string_view>
    kPaymentInstrumentsCompositePrimaryKey = {kInstrumentId, kInstrumentType};

constexpr std::string_view kPaymentInstrumentsMetadataTable =
    "payment_instruments_metadata";
// kInstrumentId = "instrument_id"
// kInstrumentType = "instrument_type"
// kUseCount = "use_count"
// kUseDate = "use_date"
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kPaymentInstrumentsMetadataColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER NOT NULL"},
        {kInstrumentType, "INTEGER NOT NULL"},
        {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
        {kUseDate, "INTEGER NOT NULL DEFAULT 0"}};
constexpr std::initializer_list<std::string_view>
    kPaymentInstrumentsMetadataCompositePrimaryKey = {kInstrumentId,
                                                      kInstrumentType};

constexpr std::string_view kPaymentInstrumentSupportedRailsTable =
    "payment_instrument_supported_rails";
// kInstrumentId = "instrument_id"
// kInstrumentType = "instrument_type"
constexpr std::string_view kPaymentRail = "payment_rail";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kPaymentInstrumentSupportedRailsColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER NOT NULL"},
        {kInstrumentType, "INTEGER NOT NULL"},
        {kPaymentRail, "INTEGER NOT NULL"}};
constexpr std::initializer_list<std::string_view>
    kPaymentInstrumentSupportedRailsCompositePrimaryKey = {
        kInstrumentId, kInstrumentType, kPaymentRail};

constexpr std::string_view kBankAccountsTable = "bank_accounts";
// kInstrumentId = "instrument_id"
// kBankName = "bank_name"
constexpr std::string_view kAccountNumberSuffix = "account_number_suffix";
constexpr std::string_view kAccountType = "account_type";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    bank_accounts_column_names_and_types = {
        {kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
        {kBankName, "VARCHAR"},
        {kAccountNumberSuffix, "VARCHAR"},
        {kAccountType, "INTEGER DEFAULT 0"}};

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
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key = {}) {
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
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key = {}) {
  return db->DoesTableExist(table_name) ||
         CreateTable(db, table_name, column_names_and_types,
                     composite_primary_key);
}

// Creates and index on `table_name` for the provided `columns`.
// The index is named after the table and columns, separated by '_'.
// Returns true if successful.
bool CreateIndex(sql::Database* db,
                 std::string_view table_name,
                 std::initializer_list<std::string_view> columns) {
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
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
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
                 std::string_view from,
                 std::string_view to) {
  return db->Execute(
      base::StrCat({"ALTER TABLE ", from, " RENAME TO ", to}).c_str());
}

// Wrapper around `sql::Database::DoesColumnExist()`, because that function
// only accepts const char* parameters.
bool DoesColumnExist(sql::Database* db,
                     std::string_view table_name,
                     std::string_view column_name) {
  return db->DoesColumnExist(std::string(table_name).c_str(),
                             std::string(column_name).c_str());
}

// Adds a column named `column_name` of `type` to `table_name` and returns true
// if successful.
bool AddColumn(sql::Database* db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type) {
  return db->Execute(base::StrCat({"ALTER TABLE ", table_name, " ADD COLUMN ",
                                   column_name, " ", type})
                         .c_str());
}

// Like `AddColumn()`, but conditioned on `column` not existing in `table_name`.
// Returns true if the column is now part of the table
bool AddColumnIfNotExists(sql::Database* db,
                          std::string_view table_name,
                          std::string_view column_name,
                          std::string_view type) {
  return DoesColumnExist(db, table_name, column_name) ||
         AddColumn(db, table_name, column_name, type);
}

// Drops the column named `column_name` from `table_name` and returns true if
// successful.
bool DropColumn(sql::Database* db,
                std::string_view table_name,
                std::string_view column_name) {
  return db->Execute(
      base::StrCat({"ALTER TABLE ", table_name, " DROP COLUMN ", column_name})
          .c_str());
  ;
}

// Drops `table_name` and returns true if successful.
bool DropTable(sql::Database* db, std::string_view table_name) {
  return db->Execute(base::StrCat({"DROP TABLE ", table_name}).c_str());
}

// Initializes `statement` with DELETE FROM `table_name`. A WHERE clause
// can optionally be specified in `where_clause`.
void DeleteBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause = "") {
  auto where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", table_name, where}).c_str()));
}

// Like `DeleteBuilder()`, but runs the statement and returns true if it was
// successful.
bool Delete(sql::Database* db,
            std::string_view table_name,
            std::string_view where_clause = "") {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, where_clause);
  return statement.Run();
}

// Wrapper around `DeleteBuilder()`, which initializes the where clause as
// `column` = `value`.
// Runs the statement and returns true if it was successful.
bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindString(0, value);
  return statement.Run();
}

// Wrapper around `DeleteBuilder()`, which initializes the where clause as
// `column` = `value` for int64_t type.
// Runs the statement and returns true if it was successful.
bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindInt64(0, value);
  return statement.Run();
}

// Initializes `statement` with UPDATE `table_name` SET `column_names` = ?, with
// a placeholder for every `column_names`. A WHERE clause can optionally be
// specified in `where_clause`.
void UpdateBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
                   std::string_view where_clause = "") {
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
                   std::string_view table_name,
                   std::initializer_list<std::string_view> columns,
                   std::string_view modifiers = "") {
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"SELECT ", base::JoinString(columns, ", "), " FROM ",
                    table_name, " ", modifiers})
          .c_str()));
}

// Wrapper around `SelectBuilder()` that restricts the it to the provided
// `guid`. Returns `statement.is_valid() && statement.Step()`.
bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  std::string_view table_name,
                  std::initializer_list<std::string_view> columns,
                  std::string_view guid) {
  SelectBuilder(db, statement, table_name, columns, "WHERE guid=?");
  statement.BindString(0, guid);
  return statement.is_valid() && statement.Step();
}

// Wrapper around `SelectBuilder()` that restricts it to the half-open interval
// [low, high[ of `column_between`.
void SelectBetween(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> columns,
                   std::string_view column_between,
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

void AddAutofillProfileDetailsFromStatement(sql::Statement& s,
                                            AutofillProfile* profile) {
  int index = 0;
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
}

void BindEncryptedValueToColumn(sql::Statement* s,
                                int column_index,
                                const std::u16string& value,
                                const AutofillTableEncryptor& encryptor) {
  std::string encrypted_data;
  encryptor.EncryptString16(value, &encrypted_data);
  s->BindBlob(column_index, encrypted_data);
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               const base::Time& modification_date,
                               sql::Statement* s,
                               const AutofillTableEncryptor& encryptor) {
  DCHECK(base::Uuid::ParseCaseInsensitive(credit_card.guid()).is_valid());
  int index = 0;
  s->BindString(index++, credit_card.guid());

  for (ServerFieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                               CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    s->BindString16(index++, Truncate(credit_card.GetRawInfo(type)));
  }
  BindEncryptedValueToColumn(
      s, index++, credit_card.GetRawInfo(CREDIT_CARD_NUMBER), encryptor);

  s->BindInt64(index++, credit_card.use_count());
  s->BindInt64(index++, credit_card.use_date().ToTimeT());
  s->BindInt64(index++, modification_date.ToTimeT());
  s->BindString(index++, credit_card.origin());
  s->BindString(index++, credit_card.billing_address_id());
  s->BindString16(index++, credit_card.nickname());
}

void BindLocalStoredCvcToStatement(const std::string& guid,
                                   const std::u16string& cvc,
                                   const base::Time& modification_date,
                                   sql::Statement* s,
                                   const AutofillTableEncryptor& encryptor) {
  CHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  int index = 0;
  s->BindString(index++, guid);

  BindEncryptedValueToColumn(s, index++, cvc, encryptor);
  s->BindInt64(index++, modification_date.ToTimeT());
}

void BindServerCvcToStatement(const ServerCvc& server_cvc,
                              const AutofillTableEncryptor& encryptor,
                              sql::Statement* s) {
  int index = 0;
  s->BindInt64(index++, server_cvc.instrument_id);
  BindEncryptedValueToColumn(s, index++, server_cvc.cvc, encryptor);
  s->BindInt64(index++, server_cvc.last_updated_timestamp.ToTimeT());
}

void BindIbanToStatement(const Iban& iban,
                         sql::Statement* s,
                         const AutofillTableEncryptor& encryptor) {
  DCHECK(base::Uuid::ParseCaseInsensitive(iban.guid()).is_valid());
  int index = 0;
  s->BindString(index++, iban.guid());

  s->BindInt64(index++, iban.use_count());
  s->BindInt64(index++, iban.use_date().ToTimeT());

  BindEncryptedValueToColumn(s, index++, iban.value(), encryptor);
  s->BindString16(index++, iban.nickname());
}

void BindVirtualCardUsageDataToStatement(
    const VirtualCardUsageData& virtual_card_usage_data,
    sql::Statement& s) {
  s.BindString(0, *virtual_card_usage_data.usage_data_id());
  s.BindInt64(1, *virtual_card_usage_data.instrument_id());
  s.BindString(2, virtual_card_usage_data.merchant_origin().Serialize());
  s.BindString16(3, *virtual_card_usage_data.virtual_card_last_four());
}

std::unique_ptr<VirtualCardUsageData> GetVirtualCardUsageDataFromStatement(
    sql::Statement& s) {
  int index = 0;
  std::string id = s.ColumnString(index++);
  int64_t instrument_id = s.ColumnInt64(index++);
  std::string merchant_domain = s.ColumnString(index++);
  std::u16string last_four = s.ColumnString16(index++);

  return std::make_unique<VirtualCardUsageData>(
      VirtualCardUsageData::UsageDataId(id),
      VirtualCardUsageData::InstrumentId(instrument_id),
      VirtualCardUsageData::VirtualCardLastFour(last_four),
      url::Origin::Create(GURL(merchant_domain)));
}

std::u16string UnencryptValueFromColumn(
    sql::Statement& s,
    int column_index,
    const AutofillTableEncryptor& encryptor) {
  std::u16string value;
  std::string encrypted_value;
  s.ColumnBlobAsString(column_index, &encrypted_value);
  if (!encrypted_value.empty()) {
    encryptor.DecryptString16(encrypted_value, &value);
  }
  return value;
}

std::unique_ptr<CreditCard> CreditCardFromStatement(
    sql::Statement& card_statement,
    absl::optional<std::reference_wrapper<sql::Statement>> cvc_statement,
    const AutofillTableEncryptor& encryptor) {
  auto credit_card = std::make_unique<CreditCard>();

  int index = 0;
  credit_card->set_guid(card_statement.ColumnString(index++));
  DCHECK(base::Uuid::ParseCaseInsensitive(credit_card->guid()).is_valid());

  for (ServerFieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                               CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    credit_card->SetRawInfo(type, card_statement.ColumnString16(index++));
  }
  credit_card->SetRawInfo(
      CREDIT_CARD_NUMBER,
      UnencryptValueFromColumn(card_statement, index++, encryptor));
  credit_card->set_use_count(card_statement.ColumnInt64(index++));
  credit_card->set_use_date(
      base::Time::FromTimeT(card_statement.ColumnInt64(index++)));
  credit_card->set_modification_date(
      base::Time::FromTimeT(card_statement.ColumnInt64(index++)));
  credit_card->set_origin(card_statement.ColumnString(index++));
  credit_card->set_billing_address_id(card_statement.ColumnString(index++));
  credit_card->SetNickname(card_statement.ColumnString16(index++));
  // Only set cvc if we retrieve cvc from local_stored_cvc table.
  if (cvc_statement) {
    credit_card->set_cvc(
        UnencryptValueFromColumn(cvc_statement.value(), 0, encryptor));
  }
  return credit_card;
}

std::unique_ptr<ServerCvc> ServerCvcFromStatement(
    sql::Statement& s,
    const AutofillTableEncryptor& encryptor) {
  return std::make_unique<ServerCvc>(ServerCvc{
      .instrument_id = s.ColumnInt64(0),
      .cvc = UnencryptValueFromColumn(s, 1, encryptor),
      .last_updated_timestamp = base::Time::FromTimeT(s.ColumnInt64(2))});
}

std::unique_ptr<Iban> IbanFromStatement(
    sql::Statement& s,
    const AutofillTableEncryptor& encryptor) {
  int index = 0;
  auto iban = std::make_unique<Iban>(Iban::Guid(s.ColumnString(index++)));

  DCHECK(base::Uuid::ParseCaseInsensitive(iban->guid()).is_valid());
  iban->set_use_count(s.ColumnInt64(index++));
  iban->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));

  iban->SetRawInfo(IBAN_VALUE, UnencryptValueFromColumn(s, index++, encryptor));
  iban->set_nickname(s.ColumnString16(index++));
  return iban;
}

bool AddAutofillProfileNamesToProfile(sql::Database* db,
                                      AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfileNamesTable)) {
    return false;
  }
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
      for (ServerFieldType type :
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

bool AddAutofillProfileEmailsToProfile(sql::Database* db,
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

bool AddAutofillProfilePhonesToProfile(sql::Database* db,
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

bool AddAutofillProfileBirthdateToProfile(sql::Database* db,
                                          AutofillProfile* profile) {
  if (!db->DoesTableExist(kAutofillProfileBirthdatesTable)) {
    return false;
  }
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
// contains the values for every relevant ServerFieldType. The following two
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
  if (!s.Run())
    return false;
  for (ServerFieldType type :
       AutofillTable::GetStoredTypesForAutofillProfile()) {
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
    InsertBuilder(db, s, GetProfileTypeTokensTable(profile.source()),
                  {kGuid, kType, kValue, kVerificationStatus, kObservations});
    s.BindString(0, profile.guid());
    s.BindInt(1, type);
    s.BindString16(2, Truncate(profile.GetRawInfo(type)));
    s.BindInt(3, profile.GetVerificationStatusInt(type));
    s.BindBlob(
        4, profile.token_quality().SerializeObservationsForStoredType(type));
    if (!s.Run())
      return false;
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
  // Note that `GetStoredTypesForAutofillProfile()` might change in future
  // versions. Due to the flexible layout of the type tokens table, this is not
  // a problem.
  for (ServerFieldType type :
       AutofillTable::GetStoredTypesForAutofillProfile()) {
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

// static
const size_t AutofillTable::kMaxDataLength = 1024;

AutofillTable::AutofillTable()
    : autofill_table_encryptor_(
          AutofillTableEncryptorFactory::GetInstance()->Create()) {
  DCHECK(autofill_table_encryptor_);
}

AutofillTable::~AutofillTable() = default;

// static
AutofillTable* AutofillTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<AutofillTable*>(db->GetTable(GetKey()));
}

// static
base::span<const ServerFieldType>
AutofillTable::GetStoredTypesForAutofillProfile() {
  static constexpr ServerFieldType stored_types[]{
      COMPANY_NAME,
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
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_APT_NUM,
      ADDRESS_HOME_FLOOR,
      ADDRESS_HOME_OVERFLOW,
      ADDRESS_HOME_LANDMARK,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS,
      ADDRESS_HOME_BETWEEN_STREETS_1,
      ADDRESS_HOME_BETWEEN_STREETS_2,
      ADDRESS_HOME_ADMIN_LEVEL2,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      BIRTHDATE_DAY,
      BIRTHDATE_MONTH,
      BIRTHDATE_4_DIGIT_YEAR};
  return stored_types;
}

WebDatabaseTable::TypeKey AutofillTable::GetTypeKey() const {
  return GetKey();
}

bool AutofillTable::CreateTablesIfNecessary() {
  return InitMainTable() && InitCreditCardsTable() && InitLocalIbansTable() &&
         InitMaskedCreditCardsTable() && InitUnmaskedCreditCardsTable() &&
         InitServerCardMetadataTable() && InitServerAddressesTable() &&
         InitServerAddressMetadataTable() && InitAutofillSyncMetadataTable() &&
         InitModelTypeStateTable() && InitPaymentsCustomerDataTable() &&
         InitServerCreditCardCloudTokenDataTable() && InitOfferDataTable() &&
         InitOfferEligibleInstrumentTable() && InitOfferMerchantDomainTable() &&
         InitProfileMetadataTable(AutofillProfile::Source::kAccount) &&
         InitProfileTypeTokensTable(AutofillProfile::Source::kAccount) &&
         InitProfileMetadataTable(AutofillProfile::Source::kLocalOrSyncable) &&
         InitProfileTypeTokensTable(
             AutofillProfile::Source::kLocalOrSyncable) &&
         InitVirtualCardUsageDataTable() && InitStoredCvcTable() &&
         InitMaskedIbansTable() && InitMaskedIbansMetadataTable() &&
         InitBankAccountsTable() && InitPaymentInstrumentsTable() &&
         InitPaymentInstrumentsMetadataTable() &&
         InitPaymentInstrumentSupportedRailsTable();
}

bool AutofillTable::MigrateToVersion(int version,
                                     bool* update_compatible_version) {
  if (!db_->is_open()) {
    return false;
  }
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
      return MigrateToVersion105AddAutofillIbanTable();
    case 106:
      *update_compatible_version = true;
      return MigrateToVersion106RecreateAutofillIbanTable();
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
    case 111:
      *update_compatible_version = false;
      return MigrateToVersion111AddVirtualCardEnrollmentTypeColumn();
    case 112:  // AutofillTable didn't change in WebDatabase version 112.
      *update_compatible_version = false;
      return true;
    case 113:
      *update_compatible_version = false;
      return MigrateToVersion113MigrateLocalAddressProfilesToNewTable();
    case 114:
      *update_compatible_version = true;
      return MigrateToVersion114DropLegacyAddressTables();
    case 115:
      *update_compatible_version = true;
      return MigrateToVersion115EncryptIbanValue();
    case 116:
      *update_compatible_version = false;
      return MigrateToVersion116AddStoredCvcTable();
    case 117:
      *update_compatible_version = false;
      return MigrateToVersion117AddProfileObservationColumn();
    case 118:
      *update_compatible_version = true;
      return MigrateToVersion118RemovePaymentsUpiVpaTable();
    case 119:
      *update_compatible_version = true;
      return MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable();
    case 120:
      *update_compatible_version = false;
      return MigrateToVersion120AddPaymentInstrumentAndBankAccountTables();
  }
  return true;
}

bool AutofillTable::AddFormFieldValues(
    const std::vector<FormFieldData>& elements,
    std::vector<AutocompleteChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, AutofillClock::Now());
}

bool AutofillTable::AddFormFieldValue(
    const FormFieldData& element,
    std::vector<AutocompleteChange>* changes) {
  return AddFormFieldValueTime(element, changes, AutofillClock::Now());
}

bool AutofillTable::GetFormValuesForElementName(
    const std::u16string& name,
    const std::u16string& prefix,
    std::vector<AutocompleteEntry>* entries,
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
      entries->push_back(AutocompleteEntry(
          AutocompleteKey(/*name=*/s.ColumnString16(0),
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
      entries->push_back(AutocompleteEntry(
          AutocompleteKey(/*name=*/s1.ColumnString16(0),
                          /*value=*/s1.ColumnString16(1)),
          /*date_created=*/base::Time::FromTimeT(s1.ColumnInt64(2)),
          /*date_last_used=*/base::Time::FromTimeT(s1.ColumnInt64(3))));
    }

    succeeded = s1.Succeeded();
  }

  return succeeded;
}

bool AutofillTable::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<AutocompleteChange>* changes) {
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
  std::vector<AutocompleteChange> tentative_changes;
  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    int count = s.ColumnInt(2);
    time_t date_created_time_t = s.ColumnInt64(3);
    time_t date_last_used_time_t = s.ColumnInt64(4);

    // If *all* uses of the element were between |delete_begin| and
    // |delete_end|, then delete the element.  Otherwise, update the use
    // timestamps and use count.
    AutocompleteChange::Type change_type;
    if (date_created_time_t >= delete_begin_time_t &&
        date_last_used_time_t < delete_end_time_t) {
      change_type = AutocompleteChange::REMOVE;
    } else {
      change_type = AutocompleteChange::UPDATE;

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

    tentative_changes.emplace_back(change_type, AutocompleteKey(name, value));
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
    std::vector<AutocompleteChange>* changes) {
  const auto change_type = AutocompleteChange::EXPIRE;

  base::Time expiration_time =
      AutofillClock::Now() - kAutocompleteRetentionPolicyPeriod;

  // Query for the name and value of all form elements that were last used
  // before the |expiration_time|.
  sql::Statement select_for_delete;
  SelectBuilder(db_, select_for_delete, kAutofillTable, {kName, kValue},
                "WHERE date_last_used < ?");
  select_for_delete.BindInt64(0, expiration_time.ToTimeT());
  std::vector<AutocompleteChange> tentative_changes;
  while (select_for_delete.Step()) {
    std::u16string name = select_for_delete.ColumnString16(0);
    std::u16string value = select_for_delete.ColumnString16(1);
    tentative_changes.emplace_back(change_type, AutocompleteKey(name, value));
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

bool AutofillTable::GetAllAutocompleteEntries(
    std::vector<AutocompleteEntry>* entries) {
  sql::Statement s;
  SelectBuilder(db_, s, kAutofillTable,
                {kName, kValue, kDateCreated, kDateLastUsed});

  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    base::Time date_created = base::Time::FromTimeT(s.ColumnInt64(2));
    base::Time date_last_used = base::Time::FromTimeT(s.ColumnInt64(3));
    entries->push_back(AutocompleteEntry(AutocompleteKey(name, value),
                                         date_created, date_last_used));
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

bool AutofillTable::UpdateAutocompleteEntries(
    const std::vector<AutocompleteEntry>& entries) {
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
    if (!InsertAutocompleteEntry(entry)) {
      return false;
    }
  }

  return true;
}

bool AutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  sql::Transaction transaction(db_);
  return transaction.Begin() && AddAutofillProfileToTable(db_, profile) &&
         transaction.Commit();
}

bool AutofillTable::UpdateAutofillProfile(const AutofillProfile& profile) {
  DCHECK(base::Uuid::ParseCaseInsensitive(profile.guid()).is_valid());

  std::unique_ptr<AutofillProfile> old_profile =
      GetAutofillProfile(profile.guid(), profile.source());
  if (!old_profile)
    return false;

  // Implementing an update as remove + add has multiple advantages:
  // - Prevents outdated (ServerFieldType, value) pairs from remaining in the
  //   `GetProfileTypeTokensTable(profile)`, in case field types are removed.
  // - Simpler code.
  // The possible downside is performance. This is not an issue, as updates
  // happen rarely and asynchronously.
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         RemoveAutofillProfile(profile.guid(), profile.source()) &&
         AddAutofillProfileToTable(db_, profile) && transaction.Commit();
}

bool AutofillTable::RemoveAutofillProfile(
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

bool AutofillTable::RemoveAllAutofillProfiles(
    AutofillProfile::Source profile_source) {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         Delete(db_, GetProfileMetadataTable(profile_source)) &&
         Delete(db_, GetProfileTypeTokensTable(profile_source)) &&
         transaction.Commit();
}

std::unique_ptr<AutofillProfile> AutofillTable::GetAutofillProfile(
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
    ServerFieldType type;
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
    ServerFieldType type = ToSafeServerFieldType(s.ColumnInt(0), UNKNOWN_TYPE);
    if (type == UNKNOWN_TYPE) {
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

  // TODO(crbug.com/1464568): Define a proper migration strategy from stored
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

bool AutofillTable::GetAutofillProfiles(
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
AutofillTable::GetAutofillProfileFromLegacyTable(
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
      guid, AutofillProfile::Source::kLocalOrSyncable);
  DCHECK(base::Uuid::ParseCaseInsensitive(profile->guid()).is_valid());

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

// TODO(crbug.com/1443393): This function's implementation is very similar to
// `GetAutofillProfiles()`. Simplify somehow.
bool AutofillTable::GetAutofillProfilesFromLegacyTable(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) const {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillProfilesTable, {kGuid});

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile =
        GetAutofillProfileFromLegacyTable(guid);
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
    insert.Reset(/*clear_bound_vars=*/true);

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

bool AutofillTable::AddBankAccount(const BankAccount& bank_account) {
  // TODO(crbug.com/1475426): Add implementation.
  return false;
}

bool AutofillTable::UpdateBankAccount(const BankAccount& bank_account) {
  // TODO(crbug.com/1475426): Add implementation.
  return false;
}

bool AutofillTable::RemoveBankAccount(int64_t instrument_id) {
  // TODO(crbug.com/1475426): Add implementation.
  return false;
}

bool AutofillTable::AddLocalIban(const Iban& iban) {
  sql::Statement s;
  InsertBuilder(db_, s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname});
  BindIbanToStatement(iban, &s, *autofill_table_encryptor_);
  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return true;
}

bool AutofillTable::UpdateLocalIban(const Iban& iban) {
  DCHECK(base::Uuid::ParseCaseInsensitive(iban.guid()).is_valid());

  std::unique_ptr<Iban> old_iban = GetLocalIban(iban.guid());
  if (!old_iban) {
    return false;
  }

  if (*old_iban == iban) {
    return true;
  }

  sql::Statement s;
  UpdateBuilder(db_, s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname},
                "guid=?1");
  BindIbanToStatement(iban, &s, *autofill_table_encryptor_);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveLocalIban(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  return DeleteWhereColumnEq(db_, kLocalIbansTable, kGuid, guid);
}

std::unique_ptr<Iban> AutofillTable::GetLocalIban(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Statement s;
  SelectBuilder(db_, s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname},
                "WHERE guid = ?");
  s.BindString(0, guid);

  if (!s.Step())
    return nullptr;

  return IbanFromStatement(s, *autofill_table_encryptor_);
}

bool AutofillTable::GetLocalIbans(std::vector<std::unique_ptr<Iban>>* ibans) {
  DCHECK(ibans);
  ibans->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kLocalIbansTable, {kGuid},
                "ORDER BY use_date DESC, guid");

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<Iban> iban = GetLocalIban(guid);
    if (!iban)
      return false;
    ibans->push_back(std::move(iban));
  }

  return s.Succeeded();
}

bool AutofillTable::AddCreditCard(const CreditCard& credit_card) {
  // We have 2 independent DB operations:
  // 1. Insert a credit_card
  // 2. Insert a CVC.
  // We don't wrap these in a transaction because a credit_card without a CVC is
  // a valid record, we are OK that the CC is stored but the CVC fails silently.
  // We only return false if credit_card insert fails.
  sql::Statement card_statement;
  InsertBuilder(db_, card_statement, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname});
  BindCreditCardToStatement(credit_card, AutofillClock::Now(), &card_statement,
                            *autofill_table_encryptor_);

  if (!card_statement.Run()) {
    return false;
  }

  DCHECK_GT(db_->GetLastChangeCount(), 0);

  // If credit card contains cvc, will store cvc in local_stored_cvc table.
  if (!credit_card.cvc().empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    sql::Statement cvc_statement;
    InsertBuilder(db_, cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp});
    BindLocalStoredCvcToStatement(credit_card.guid(), credit_card.cvc(),
                                  AutofillClock::Now(), &cvc_statement,
                                  *autofill_table_encryptor_);
    cvc_statement.Run();
  }

  return true;
}

bool AutofillTable::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK(base::Uuid::ParseCaseInsensitive(credit_card.guid()).is_valid());

  std::unique_ptr<CreditCard> old_credit_card =
      GetCreditCard(credit_card.guid());
  if (!old_credit_card)
    return false;

  bool cvc_result = false;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    cvc_result = UpdateLocalCvc(credit_card.guid(), credit_card.cvc());
  }

  // If only cvc is updated, we don't need to update credit_card table
  // date_modified field. Since we already checked if cvc updated, to ignore
  // cvc, we set old_credit_card cvc to new cvc.
  old_credit_card->set_cvc(credit_card.cvc());
  bool card_updated = *old_credit_card != credit_card;
  sql::Statement card_statement;
  UpdateBuilder(db_, card_statement, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname},
                "guid=?1");
  BindCreditCardToStatement(credit_card,
                            card_updated ? AutofillClock::Now()
                                         : old_credit_card->modification_date(),
                            &card_statement, *autofill_table_encryptor_);
  bool card_result = card_statement.Run();
  CHECK(db_->GetLastChangeCount() > 0);

  return cvc_result || card_result;
}

bool AutofillTable::UpdateLocalCvc(const std::string& guid,
                                   const std::u16string& cvc) {
  std::unique_ptr<CreditCard> old_credit_card = GetCreditCard(guid);
  CHECK(old_credit_card);
  if (old_credit_card->cvc() == cvc) {
    return false;
  }
  if (cvc.empty()) {
    // Delete the CVC record if the new CVC is empty.
    return DeleteWhereColumnEq(db_, kLocalStoredCvcTable, kGuid, guid);
  }
  sql::Statement cvc_statement;
  // If existing card doesn't have CVC, we will insert CVC into
  // `kLocalStoredCvcTable` table. If existing card does have CVC, we will
  // update CVC for `kLocalStoredCvcTable` table.
  if (old_credit_card->cvc().empty()) {
    InsertBuilder(db_, cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp});
  } else {
    UpdateBuilder(db_, cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp}, "guid=?1");
  }
  BindLocalStoredCvcToStatement(guid, cvc, AutofillClock::Now(), &cvc_statement,
                                *autofill_table_encryptor_);
  bool cvc_result = cvc_statement.Run();
  CHECK(db_->GetLastChangeCount() > 0);
  return cvc_result;
}

bool AutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  DeleteWhereColumnEq(db_, kLocalStoredCvcTable, kGuid, guid);
  return DeleteWhereColumnEq(db_, kCreditCardsTable, kGuid, guid);
}

bool AutofillTable::AddFullServerCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kFullServerCard, credit_card.record_type());
  DCHECK(!credit_card.number().empty());
  DCHECK(!credit_card.server_id().empty());

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromUnmaskedCreditCards(credit_card.server_id());
  DeleteFromMaskedCreditCards(credit_card.server_id());

  CreditCard masked(credit_card);
  masked.set_record_type(CreditCard::RecordType::kMaskedServerCard);
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
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Statement card_statement;
  SelectBuilder(db_, card_statement, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname},
                "WHERE guid = ?");
  card_statement.BindString(0, guid);

  if (!card_statement.Step()) {
    return nullptr;
  }

  // Get cvc from local_stored_cvc table.
  sql::Statement cvc_statement;
  SelectBuilder(db_, cvc_statement, kLocalStoredCvcTable, {kValueEncrypted},
                "WHERE guid = ?");
  cvc_statement.BindString(0, guid);

  bool has_cvc = cvc_statement.Step();
  return CreditCardFromStatement(
      card_statement,
      has_cvc ? absl::optional<
                    std::reference_wrapper<sql::Statement>>{cvc_statement}
              : absl::nullopt,
      *autofill_table_encryptor_);
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
  auto instrument_to_cvc = base::MakeFlatMap<int64_t, std::u16string>(
      GetAllServerCvcs(), {}, [](const auto& server_cvc) {
        return std::make_pair(server_cvc->instrument_id, server_cvc->cvc);
      });

  sql::Statement s;
  SelectBuilder(
      db_, s, base::StrCat({kMaskedCreditCardsTable, " AS masked"}),
      {kCardNumberEncrypted, kLastFour, base::StrCat({"masked.", kId}),
       base::StrCat({"metadata.", kUseCount}),
       base::StrCat({"metadata.", kUseDate}), kNetwork, kNameOnCard, kExpMonth,
       kExpYear, base::StrCat({"metadata.", kBillingAddressId}), kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kVirtualCardEnrollmentType, kCardArtUrl,
       kProductDescription},
      "LEFT OUTER JOIN unmasked_credit_cards USING (id) "
      "LEFT OUTER JOIN server_card_metadata AS metadata USING (id)");
  while (s.Step()) {
    int index = 0;

    // If the card_number_encrypted field is nonempty, we can assume this card
    // is a full card, otherwise it's masked.
    std::u16string full_card_number =
        UnencryptValueFromColumn(s, index++, *autofill_table_encryptor_);
    std::u16string last_four = s.ColumnString16(index++);
    CreditCard::RecordType record_type =
        full_card_number.empty() ? CreditCard::RecordType::kMaskedServerCard
                                 : CreditCard::RecordType::kFullServerCard;
    std::string server_id = s.ColumnString(index++);
    std::unique_ptr<CreditCard> card =
        std::make_unique<CreditCard>(record_type, server_id);
    card->SetRawInfo(CREDIT_CARD_NUMBER,
                     record_type == CreditCard::RecordType::kMaskedServerCard
                         ? last_four
                         : full_card_number);
    card->set_use_count(s.ColumnInt64(index++));
    card->set_use_date(base::Time::FromInternalValue(s.ColumnInt64(index++)));
    // Modification date is not tracked for server cards. Explicitly set it here
    // to override the default value of AutofillClock::Now().
    card->set_modification_date(base::Time());

    std::string card_network = s.ColumnString(index++);
    if (record_type == CreditCard::RecordType::kMaskedServerCard) {
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
    card->set_virtual_card_enrollment_type(
        static_cast<CreditCard::VirtualCardEnrollmentType>(
            s.ColumnInt(index++)));
    card->set_card_art_url(GURL(s.ColumnString(index++)));
    card->set_product_description(s.ColumnString16(index++));
    card->set_cvc(instrument_to_cvc[card->instrument_id()]);
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
  unmasked.set_record_type(CreditCard::RecordType::kFullServerCard);
  unmasked.SetNumber(full_number);
  unmasked.RecordAndLogUse();
  UpdateServerCardMetadata(unmasked);

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::MaskServerCreditCard(const std::string& id) {
  return DeleteFromUnmaskedCreditCards(id);
}

bool AutofillTable::AddServerCvc(const ServerCvc& server_cvc) {
  if (server_cvc.cvc.empty()) {
    return false;
  }

  sql::Statement s;
  InsertBuilder(db_, s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp});
  BindServerCvcToStatement(server_cvc, *autofill_table_encryptor_, &s);
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerCvc(const ServerCvc& server_cvc) {
  sql::Statement s;
  UpdateBuilder(db_, s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp},
                "instrument_id=?1");
  BindServerCvcToStatement(server_cvc, *autofill_table_encryptor_, &s);
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::RemoveServerCvc(int64_t instrument_id) {
  DeleteWhereColumnEq(db_, kServerStoredCvcTable, kInstrumentId, instrument_id);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::ClearServerCvcs() {
  Delete(db_, kServerStoredCvcTable);
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::ReconcileServerCvcs() {
  sql::Statement s(db_->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kServerStoredCvcTable, " WHERE ",
                    kInstrumentId, " NOT IN (SELECT ", kInstrumentId, " FROM ",
                    kMaskedCreditCardsTable, ")"})
          .c_str()));
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

std::vector<std::unique_ptr<ServerCvc>> AutofillTable::GetAllServerCvcs()
    const {
  std::vector<std::unique_ptr<ServerCvc>> cvcs;
  sql::Statement s;
  SelectBuilder(db_, s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp});
  while (s.Step()) {
    cvcs.push_back(ServerCvcFromStatement(s, *autofill_table_encryptor_));
  }
  return cvcs;
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
  DCHECK_NE(CreditCard::RecordType::kLocalCard, credit_card.record_type());

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

bool AutofillTable::AddOrUpdateServerIbanMetadata(const Iban& iban) {
  CHECK_EQ(Iban::RecordType::kServerIban, iban.record_type());
  // There's no need to verify if removal succeeded, because if it's a new IBAN,
  // the removal call won't do anything.
  RemoveServerIbanMetadata(iban.instrument_id());

  sql::Statement s;
  InsertBuilder(db_, s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});
  s.BindString(0, iban.GetMetadata().id);
  s.BindInt64(1, iban.GetMetadata().use_count);
  s.BindTime(2, iban.GetMetadata().use_date);
  return s.Run();
}

bool AutofillTable::RemoveServerIbanMetadata(const std::string& instrument_id) {
  return DeleteWhereColumnEq(db_, kMaskedIbansMetadataTable, kInstrumentId,
                             instrument_id);
}

std::vector<AutofillMetadata> AutofillTable::GetServerIbansMetadata() const {
  sql::Statement s;
  SelectBuilder(db_, s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});

  std::vector<AutofillMetadata> ibans_metadata;
  while (s.Step()) {
    int index = 0;
    AutofillMetadata iban_metadata;
    iban_metadata.id = s.ColumnString(index++);
    iban_metadata.use_count = s.ColumnInt64(index++);
    iban_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    ibans_metadata.push_back(iban_metadata);
  }
  return ibans_metadata;
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
  InsertBuilder(db_, masked_insert, kMaskedCreditCardsTable,
                {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear,
                 kBankName, kNickname, kCardIssuer, kCardIssuerId,
                 kInstrumentId, kVirtualCardEnrollmentState,
                 kVirtualCardEnrollmentType, kCardArtUrl, kProductDescription});

  int index;
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::RecordType::kMaskedServerCard, card.record_type());
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
    masked_insert.BindInt(
        index++, static_cast<int>(card.virtual_card_enrollment_type()));
    masked_insert.BindString(index++, card.card_art_url().spec());
    masked_insert.BindString16(index++, card.product_description());
    masked_insert.Run();
    masked_insert.Reset(/*clear_bound_vars=*/true);
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
    insert_cloud_token.Reset(/*clear_bound_vars=*/true);
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

std::vector<std::unique_ptr<Iban>> AutofillTable::GetServerIbans() {
  sql::Statement s;
  SelectBuilder(db_, s, kMaskedIbansTable,
                {kInstrumentId, kUseCount, kUseDate, kNickname, kPrefix,
                 kSuffix, kLength},
                "LEFT OUTER JOIN masked_ibans_metadata USING (instrument_id)");

  std::vector<std::unique_ptr<Iban>> ibans;
  while (s.Step()) {
    int index = 0;
    std::unique_ptr<Iban> iban =
        std::make_unique<Iban>(Iban::InstrumentId(s.ColumnString(index++)));
    iban->set_use_count(s.ColumnInt64(index++));
    iban->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
    iban->set_nickname(s.ColumnString16(index++));
    iban->set_prefix(s.ColumnString16(index++));
    iban->set_suffix(s.ColumnString16(index++));
    iban->set_length(s.ColumnInt64(index++));
    ibans.push_back(std::move(iban));
  }

  return ibans;
}

bool AutofillTable::SetServerIbans(const std::vector<Iban>& ibans) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Delete all old ones first.
  Delete(db_, kMaskedIbansTable);
  Delete(db_, kMaskedIbansMetadataTable);
  sql::Statement s;
  InsertBuilder(db_, s, kMaskedIbansTable,
                {kInstrumentId, kNickname, kPrefix, kSuffix, kLength});
  for (const Iban& iban : ibans) {
    CHECK_EQ(Iban::RecordType::kServerIban, iban.record_type());
    int index = 0;
    s.BindString(index++, iban.instrument_id());
    s.BindString16(index++, iban.nickname());
    s.BindString16(index++, iban.prefix());
    s.BindString16(index++, iban.suffix());
    s.BindInt64(index++, iban.length());
    if (!s.Run()) {
      return false;
    }
    s.Reset(/*clear_bound_vars=*/true);

    // Save the use count and use date of the IBAN.
    AddOrUpdateServerIbanMetadata(iban);
  }
  return transaction.Commit();
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
    insert_offers.Reset(/*clear_bound_vars=*/true);

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

bool AutofillTable::AddOrUpdateVirtualCardUsageData(
    const VirtualCardUsageData& virtual_card_usage_data) {
  std::unique_ptr<VirtualCardUsageData> existing_data =
      GetVirtualCardUsageData(*virtual_card_usage_data.usage_data_id());
  sql::Statement s;
  if (!existing_data) {
    InsertBuilder(db_, s, kVirtualCardUsageDataTable,
                  {kId, kInstrumentId, kMerchantDomain, kLastFour});
  } else {
    UpdateBuilder(db_, s, kVirtualCardUsageDataTable,
                  {kId, kInstrumentId, kMerchantDomain, kLastFour}, "id=?1");
  }
  BindVirtualCardUsageDataToStatement(virtual_card_usage_data, s);
  return s.Run();
}

std::unique_ptr<VirtualCardUsageData> AutofillTable::GetVirtualCardUsageData(
    const std::string& usage_data_id) {
  sql::Statement s;
  SelectBuilder(db_, s, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour},
                "WHERE id = ?");
  s.BindString(0, usage_data_id);
  if (!s.Step()) {
    return nullptr;
  }
  return GetVirtualCardUsageDataFromStatement(s);
}

bool AutofillTable::RemoveVirtualCardUsageData(
    const std::string& usage_data_id) {
  if (!GetVirtualCardUsageData(usage_data_id)) {
    return false;
  }

  return DeleteWhereColumnEq(db_, kVirtualCardUsageDataTable, kId,
                             usage_data_id);
}

void AutofillTable::SetVirtualCardUsageData(
    const std::vector<VirtualCardUsageData>& virtual_card_usage_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return;
  }

  // Delete old data.
  Delete(db_, kVirtualCardUsageDataTable);
  // Insert new values.
  sql::Statement insert_data;
  InsertBuilder(db_, insert_data, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});
  for (const VirtualCardUsageData& data : virtual_card_usage_data) {
    BindVirtualCardUsageDataToStatement(data, insert_data);
    insert_data.Run();
    insert_data.Reset(/*clear_bound_vars=*/true);
  }
  transaction.Commit();
}

bool AutofillTable::GetAllVirtualCardUsageData(
    std::vector<std::unique_ptr<VirtualCardUsageData>>*
        virtual_card_usage_data) {
  virtual_card_usage_data->clear();

  sql::Statement s;
  SelectBuilder(db_, s, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});
  while (s.Step()) {
    virtual_card_usage_data->push_back(GetVirtualCardUsageDataFromStatement(s));
  }

  return s.Succeeded();
}

bool AutofillTable::RemoveAllVirtualCardUsageData() {
  return Delete(db_, kVirtualCardUsageDataTable);
}

bool AutofillTable::ClearAllServerData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  bool changed = false;
  for (std::string_view table_name :
       {kMaskedCreditCardsTable, kMaskedIbansTable, kUnmaskedCreditCardsTable,
        kServerAddressesTable, kServerCardMetadataTable,
        kServerAddressMetadataTable, kPaymentsCustomerDataTable,
        kServerCardCloudTokenDataTable, kOfferDataTable,
        kOfferEligibleInstrumentTable, kOfferMerchantDomainTable,
        kVirtualCardUsageDataTable}) {
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

  RemoveAllAutofillProfiles(AutofillProfile::Source::kLocalOrSyncable);
  bool changed = db_->GetLastChangeCount() > 0;
  ClearLocalPaymentMethodsData();
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
  SelectBetween(
      db_, s_profiles_get,
      GetProfileMetadataTable(AutofillProfile::Source::kLocalOrSyncable),
      {kGuid}, kDateModified, delete_begin_t, delete_end_t);

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

  // Remove Autofill profiles in the time range.
  for (const std::unique_ptr<AutofillProfile>& profile : *profiles) {
    if (!RemoveAutofillProfile(profile->guid(),
                               AutofillProfile::Source::kLocalOrSyncable)) {
      return false;
    }
  }

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

  // Remove credit card cvcs in the time range.
  sql::Statement s_cvc;
  DeleteBuilder(db_, s_cvc, kLocalStoredCvcTable,
                "last_updated_timestamp >= ? AND last_updated_timestamp < ?");
  s_cvc.BindInt64(0, delete_begin_t);
  s_cvc.BindInt64(1, delete_end_t);
  if (!s_cvc.Run()) {
    return false;
  }

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
    const base::Time& delete_end) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

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

void AutofillTable::ClearLocalPaymentMethodsData() {
  Delete(db_, kLocalStoredCvcTable);
  Delete(db_, kCreditCardsTable);
  Delete(db_, kLocalIbansTable);
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

bool AutofillTable::DeleteAllSyncMetadata(syncer::ModelType model_type) {
  return DeleteWhereColumnEq(db_, kAutofillSyncMetadataTable, kModelType,
                             GetKeyValueForModelType(model_type));
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

bool AutofillTable::MigrateToVersion83RemoveServerCardTypeColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kMaskedCreditCardsTable, kType) &&
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
  for (std::string_view column : {kHonorificPrefix, kFirstLastName,
                                  kConjunctionLastName, kSecondLastName}) {
    if (!AddColumnIfNotExists(db_, kAutofillProfileNamesTable, column,
                              "VARCHAR")) {
      return false;
    }
  }

  for (std::string_view column :
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
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kUnmaskedCreditCardsTable, kUseCount) &&
         DropColumn(db_, kUnmaskedCreditCardsTable, kUseDate) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion87AddCreditCardNicknameColumn() {
  // Add the nickname column to the credit_card table.
  return AddColumnIfNotExists(db_, kCreditCardsTable, kNickname, "VARCHAR");
}

bool AutofillTable::MigrateToVersion90AddNewStructuredAddressColumns() {
  if (!db_->DoesTableExist("autofill_profile_addresses"))
    InitLegacyProfileAddressesTable();

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

bool AutofillTable::MigrateToVersion91AddMoreStructuredAddressColumns() {
  if (!db_->DoesTableExist(kAutofillProfileAddressesTable))
    InitLegacyProfileAddressesTable();

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

bool AutofillTable::MigrateToVersion93AddAutofillProfileLabelColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable))
    InitLegacyProfileAddressesTable();

  return AddColumnIfNotExists(db_, kAutofillProfilesTable, kLabel, "VARCHAR");
}

bool AutofillTable::
    MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn() {
  if (!db_->DoesTableExist(kAutofillProfilesTable))
    InitLegacyProfileAddressesTable();

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
  for (std::string_view column :
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
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kMaskedCreditCardsTable, kStatus) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion99RemoveAutofillProfilesTrashTable() {
  return DropTable(db_, "autofill_profiles_trash");
}

bool AutofillTable::MigrateToVersion100RemoveProfileValidityBitfieldColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kAutofillProfilesTable, "validity_bitfield") &&
         DropColumn(db_, kAutofillProfilesTable,
                    "is_client_validity_states_updated") &&
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

bool AutofillTable::MigrateToVersion105AddAutofillIbanTable() {
  return CreateTable(db_, kIbansTable,
                     {{kGuid, "VARCHAR"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}});
}

bool AutofillTable::MigrateToVersion106RecreateAutofillIbanTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTable(db_, kIbansTable) &&
         CreateTable(db_, kIbansTable,
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

bool AutofillTable::MigrateToVersion111AddVirtualCardEnrollmentTypeColumn() {
  return db_->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db_, kMaskedCreditCardsTable,
                              kVirtualCardEnrollmentType, "INTEGER DEFAULT 0");
}

bool AutofillTable::MigrateToVersion113MigrateLocalAddressProfilesToNewTable() {
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

bool AutofillTable::MigrateToVersion114DropLegacyAddressTables() {
  sql::Transaction transaction(db_);
  bool success = transaction.Begin();
  for (std::string_view deprecated_table :
       {kAutofillProfilesTable, kAutofillProfileAddressesTable,
        kAutofillProfileNamesTable, kAutofillProfileEmailsTable,
        kAutofillProfilePhonesTable, kAutofillProfileBirthdatesTable}) {
    success = success && (!db_->DoesTableExist(deprecated_table) ||
                          DropTable(db_, deprecated_table));
  }
  return success && transaction.Commit();
}

bool AutofillTable::MigrateToVersion115EncryptIbanValue() {
  // Encrypt all existing IBAN values and rename the column name from `value` to
  // `value_encrypted` by the following steps:
  // 1. Read all existing guid and value data from `ibans`, encrypt all values,
  //    and rewrite to `ibans`.
  // 2. Rename `value` column to `value_encrypted` for `ibans` table.
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }
  sql::Statement s;
  SelectBuilder(db_, s, kIbansTable, {kGuid, kValue});
  std::vector<std::pair<std::string, std::u16string>> iban_guid_to_value_pairs;
  while (s.Step()) {
    iban_guid_to_value_pairs.emplace_back(s.ColumnString(0),
                                          s.ColumnString16(1));
  }
  if (!s.Succeeded()) {
    return false;
  }

  for (const auto& [guid, value] : iban_guid_to_value_pairs) {
    UpdateBuilder(db_, s, kIbansTable, {kGuid, kValue}, "guid=?1");
    int index = 0;
    s.BindString(index++, guid);
    BindEncryptedValueToColumn(&s, index++, value, *autofill_table_encryptor_);
    if (!s.Run()) {
      return false;
    }
  }

  return db_->Execute(
             base::StrCat({"ALTER TABLE ", kIbansTable, " RENAME COLUMN ",
                           kValue, " TO ", kValueEncrypted})
                 .c_str()) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion116AddStoredCvcTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kLocalStoredCvcTable,
                     {{kGuid, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kValueEncrypted, "VARCHAR NOT NULL"},
                      {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         CreateTable(db_, kServerStoredCvcTable,
                     {{kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
                      {kValueEncrypted, "VARCHAR NOT NULL"},
                      {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion117AddProfileObservationColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         AddColumn(db_, kContactInfoTypeTokensTable, kObservations, "BLOB") &&
         AddColumn(db_, kLocalAddressesTypeTokensTable, kObservations,
                   "BLOB") &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion118RemovePaymentsUpiVpaTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         (!db_->DoesTableExist(kPaymentsUpiVpaTable) ||
          DropTable(db_, kPaymentsUpiVpaTable)) &&
         transaction.Commit();
}

bool AutofillTable::
    MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kMaskedIbansTable,
                     {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kPrefix, "VARCHAR NOT NULL"},
                      {kSuffix, "VARCHAR NOT NULL"},
                      {kLength, "INTEGER NOT NULL DEFAULT 0"},
                      {kNickname, "VARCHAR"}}) &&
         CreateTable(db_, kMaskedIbansMetadataTable,
                     {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"}}) &&
         (!db_->DoesTableExist(kIbansTable) ||
          RenameTable(db_, kIbansTable, kLocalIbansTable)) &&
         transaction.Commit();
}

bool AutofillTable::
    MigrateToVersion120AddPaymentInstrumentAndBankAccountTables() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         CreateTable(db_, kBankAccountsTable,
                     bank_accounts_column_names_and_types) &&
         CreateTable(db_, kPaymentInstrumentsTable,
                     kPaymentInstrumentsColumnNamesAndTypes,
                     kPaymentInstrumentsCompositePrimaryKey) &&
         CreateTable(db_, kPaymentInstrumentsMetadataTable,
                     kPaymentInstrumentsMetadataColumnNamesAndTypes,
                     kPaymentInstrumentsMetadataCompositePrimaryKey) &&
         CreateTable(db_, kPaymentInstrumentSupportedRailsTable,
                     kPaymentInstrumentSupportedRailsColumnNamesAndTypes,
                     kPaymentInstrumentSupportedRailsCompositePrimaryKey) &&
         transaction.Commit();
}

bool AutofillTable::AddFormFieldValuesTime(
    const std::vector<FormFieldData>& elements,
    std::vector<AutocompleteChange>* changes,
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

bool AutofillTable::AddFormFieldValueTime(
    const FormFieldData& element,
    std::vector<AutocompleteChange>* changes,
    base::Time time) {
  if (!db_->is_open()) {
    return false;
  }
  // TODO(crbug.com/1424298): Remove once it is understood where the `false`
  // results are coming from.
  auto create_debug_info = [this](const char* failure_location) {
    std::vector<std::string> message_parts = {base::StringPrintf(
        "(Failure during %s, SQL error code = %d, table_exists = %d, ",
        failure_location, db_->GetErrorCode(),
        db_->DoesTableExist("autofill"))};

    for (const char* kColumnName :
         {"count", "date_last_used", "name", "value"}) {
      message_parts.push_back(
          base::StringPrintf("column %s exists = %d,", kColumnName,
                             db_->DoesColumnExist("autofill", kColumnName)));
    }

    return base::StrCat(message_parts);
  };

  sql::Statement s_exists(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM autofill WHERE name = ? AND value = ?"));
  s_exists.BindString16(0, element.name);
  s_exists.BindString16(1, element.value);
  if (!s_exists.Step()) {
    SCOPED_CRASH_KEY_STRING1024("autofill", "sql", create_debug_info("SELECT"));
    NOTREACHED();
    return false;
  }

  bool already_exists = s_exists.ColumnInt(0) > 0;
  if (already_exists) {
    sql::Statement s(db_->GetUniqueStatement(
        "UPDATE autofill SET date_last_used = ?, count = count + 1 "
        "WHERE name = ? AND value = ?"));
    s.BindInt64(0, time.ToTimeT());
    s.BindString16(1, element.name);
    s.BindString16(2, element.value);
    if (!s.Run()) {
      SCOPED_CRASH_KEY_STRING1024("autofill", "sql",
                                  create_debug_info("UPDATE"));
      NOTREACHED();
      return false;
    }
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
    if (!s.Run()) {
      SCOPED_CRASH_KEY_STRING1024("autofill", "sql",
                                  create_debug_info("INSERT"));
      NOTREACHED();
      return false;
    }
  }

  AutocompleteChange::Type change_type =
      already_exists ? AutocompleteChange::UPDATE : AutocompleteChange::ADD;
  changes->push_back(AutocompleteChange(
      change_type, AutocompleteKey(element.name, element.value)));
  return true;
}

bool AutofillTable::SupportsMetadataForModelType(
    syncer::ModelType model_type) const {
  return (model_type == syncer::AUTOFILL ||
          model_type == syncer::AUTOFILL_PROFILE ||
          model_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
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

bool AutofillTable::InsertAutocompleteEntry(const AutocompleteEntry& entry) {
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
  InsertBuilder(db_, masked_insert, kMaskedCreditCardsTable,
                {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear,
                 kBankName, kNickname, kCardIssuer, kCardIssuerId,
                 kInstrumentId, kVirtualCardEnrollmentState,
                 kVirtualCardEnrollmentType, kCardArtUrl, kProductDescription});

  int index;
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::RecordType::kMaskedServerCard, card.record_type());
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
    masked_insert.BindInt(index++, static_cast<int>(
                                   card.virtual_card_enrollment_type()));
    masked_insert.BindString(index++, card.card_art_url().spec());
    masked_insert.BindString16(index++, card.product_description());
    masked_insert.Run();
    masked_insert.Reset(/*clear_bound_vars=*/true);

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

bool AutofillTable::InitLocalIbansTable() {
  return CreateTableIfNotExists(db_, kLocalIbansTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kValueEncrypted, "VARCHAR"},
                                 {kNickname, "VARCHAR"}});
}

bool AutofillTable::InitLegacyProfilesTable() {
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

bool AutofillTable::InitLegacyProfileNamesTable() {
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

bool AutofillTable::InitLegacyProfileAddressesTable() {
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

bool AutofillTable::InitLegacyProfileEmailsTable() {
  return CreateTableIfNotExists(db_, kAutofillProfileEmailsTable,
                                {{kGuid, "VARCHAR"}, {kEmail, "VARCHAR"}});
}

bool AutofillTable::InitLegacyProfilePhonesTable() {
  return CreateTableIfNotExists(db_, kAutofillProfilePhonesTable,
                                {{kGuid, "VARCHAR"}, {kNumber, "VARCHAR"}});
}

bool AutofillTable::InitLegacyProfileBirthdatesTable() {
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
       {kCardIssuerId, "VARCHAR"},
       {kVirtualCardEnrollmentType, "INTEGER DEFAULT 0"}});
}

bool AutofillTable::InitMaskedIbansTable() {
  return CreateTableIfNotExists(
      db_, kMaskedIbansTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kPrefix, "VARCHAR NOT NULL"},
       {kSuffix, "VARCHAR NOT NULL"},
       {kLength, "INTEGER NOT NULL DEFAULT 0"},
       {kNickname, "VARCHAR"}});
}

bool AutofillTable::InitMaskedIbansMetadataTable() {
  return CreateTableIfNotExists(
      db_, kMaskedIbansMetadataTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
       {kUseDate, "INTEGER NOT NULL DEFAULT 0"}});
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

bool AutofillTable::InitServerCreditCardCloudTokenDataTable() {
  return CreateTableIfNotExists(db_, kServerCardCloudTokenDataTable,
                                {{kId, "VARCHAR"},
                                 {kSuffix, "VARCHAR"},
                                 {kExpMonth, "INTEGER DEFAULT 0"},
                                 {kExpYear, "INTEGER DEFAULT 0"},
                                 {kCardArtUrl, "VARCHAR"},
                                 {kInstrumentToken, "VARCHAR"}});
}

bool AutofillTable::InitStoredCvcTable() {
  return CreateTableIfNotExists(
             db_, kLocalStoredCvcTable,
             {{kGuid, "VARCHAR PRIMARY KEY NOT NULL"},
              {kValueEncrypted, "VARCHAR NOT NULL"},
              {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         CreateTableIfNotExists(
             db_, kServerStoredCvcTable,
             {{kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
              {kValueEncrypted, "VARCHAR NOT NULL"},
              {kLastUpdatedTimestamp, "INTEGER NOT NULL"}});
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

bool AutofillTable::InitProfileMetadataTable(AutofillProfile::Source source) {
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

bool AutofillTable::InitProfileTypeTokensTable(AutofillProfile::Source source) {
  return CreateTableIfNotExists(db_, GetProfileTypeTokensTable(source),
                                {{kGuid, "VARCHAR"},
                                 {kType, "INTEGER"},
                                 {kValue, "VARCHAR"},
                                 {kVerificationStatus, "INTEGER DEFAULT 0"},
                                 {kObservations, "BLOB"}},
                                /*composite_primary_key=*/{kGuid, kType});
}

bool AutofillTable::InitVirtualCardUsageDataTable() {
  return CreateTableIfNotExists(db_, kVirtualCardUsageDataTable,
                                {{kId, "VARCHAR PRIMARY KEY"},
                                 {kInstrumentId, "INTEGER DEFAULT 0"},
                                 {kMerchantDomain, "VARCHAR"},
                                 {kLastFour, "VARCHAR"}});
}

bool AutofillTable::InitBankAccountsTable() {
  return CreateTableIfNotExists(db_, kBankAccountsTable,
                                bank_accounts_column_names_and_types);
}

bool AutofillTable::InitPaymentInstrumentsTable() {
  return CreateTableIfNotExists(db_, kPaymentInstrumentsTable,
                                kPaymentInstrumentsColumnNamesAndTypes,
                                kPaymentInstrumentsCompositePrimaryKey);
}

bool AutofillTable::InitPaymentInstrumentsMetadataTable() {
  return CreateTableIfNotExists(db_, kPaymentInstrumentsMetadataTable,
                                kPaymentInstrumentsMetadataColumnNamesAndTypes,
                                kPaymentInstrumentsMetadataCompositePrimaryKey);
}

bool AutofillTable::InitPaymentInstrumentSupportedRailsTable() {
  return CreateTableIfNotExists(
      db_, kPaymentInstrumentSupportedRailsTable,
      kPaymentInstrumentSupportedRailsColumnNamesAndTypes,
      kPaymentInstrumentSupportedRailsCompositePrimaryKey);
}

}  // namespace autofill
