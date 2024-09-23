// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"

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

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/functional/overloaded.h"
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
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/data_model/payment_instrument.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

namespace {

constexpr std::string_view kCreditCardsTable = "credit_cards";
constexpr std::string_view kGuid = "guid";
constexpr std::string_view kNameOnCard = "name_on_card";
constexpr std::string_view kExpirationMonth = "expiration_month";
constexpr std::string_view kExpirationYear = "expiration_year";
constexpr std::string_view kCardNumberEncrypted = "card_number_encrypted";
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kUseDate = "use_date";
constexpr std::string_view kDateModified = "date_modified";
constexpr std::string_view kOrigin = "origin";
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
constexpr std::string_view kProductTermsUrl = "product_terms_url";

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
// In an older version of the table, the value used to be unencrypted.
constexpr std::string_view kValue = "value";
// kNickname = "nickname"

constexpr std::string_view kMaskedIbansTable = "masked_ibans";
// kInstrumentId = "instrument_id"
constexpr std::string_view kPrefix = "prefix";
// kSuffix = "suffix";
// kNickname = "nickname"

constexpr std::string_view kMaskedIbansMetadataTable = "masked_ibans_metadata";
// kInstrumentId = "instrument_id"
// kUseCount = "use_count"
// kUseDate = "use_date"

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

constexpr std::string_view kMaskedBankAccountsMetadataTable =
    "masked_bank_accounts_metadata";
// kInstrumentId = "instrument_id"
// kUseCount = "use_count"
// kUseDate = "use_date"
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kMaskedBankAccountsMetadataColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER NOT NULL"},
        {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
        {kUseDate, "INTEGER NOT NULL DEFAULT 0"}};

constexpr std::string_view kMaskedBankAccountsTable = "masked_bank_accounts";
// kInstrumentId = "instrument_id"
// kBankName = "bank_name"
constexpr std::string_view kAccountNumberSuffix = "account_number_suffix";
constexpr std::string_view kAccountType = "account_type";
// kNickname = "nickname"
constexpr std::string_view kDisplayIconUrl = "display_icon_url";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kMaskedBankAccountsColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
        {kBankName, "VARCHAR"},
        {kAccountNumberSuffix, "VARCHAR"},
        {kAccountType, "INTEGER DEFAULT 0"},
        {kDisplayIconUrl, "VARCHAR"},
        {kNickname, "VARCHAR"}};

constexpr std::string_view kMaskedCreditCardBenefitsTable =
    "masked_credit_card_benefits";
constexpr std::string_view kBenefitId = "benefit_id";
// kInstrumentId = "instrument_id"
constexpr std::string_view kBenefitType = "benefit_type";
constexpr std::string_view kBenefitCategory = "benefit_category";
constexpr std::string_view kBenefitDescription = "benefit_description";
constexpr std::string_view kStartTime = "start_time";
constexpr std::string_view kEndTime = "end_time";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kMaskedCreditCardBenefitsColumnNamesAndTypes = {
        {kBenefitId, "VARCHAR PRIMARY KEY NOT NULL"},
        {kInstrumentId, "INTEGER NOT NULL DEFAULT 0"},
        {kBenefitType, "INTEGER NOT NULL DEFAULT 0"},
        {kBenefitCategory, "INTEGER NOT NULL DEFAULT 0"},
        {kBenefitDescription, "VARCHAR NOT NULL"},
        {kStartTime, "INTEGER"},
        {kEndTime, "INTEGER"}};

constexpr std::string_view kBenefitMerchantDomainsTable =
    "benefit_merchant_domains";
// kBenefitId = "benefit_id"
// kMerchantDomain = "merchant_domain";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kBenefitMerchantDomainsColumnNamesAndTypes = {
        {kBenefitId, "VARCHAR NOT NULL"},
        {kMerchantDomain, "VARCHAR NOT NULL"}};

constexpr std::string_view kGenericPaymentInstrumentsTable =
    "generic_payment_instruments";
// kInstrumentId = "instrument_id"
constexpr std::string_view kSerializedValueEncrypted =
    "serialized_value_encrypted";
constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
    kGenericPaymentInstrumentsColumnNamesAndTypes = {
        {kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
        {kSerializedValueEncrypted, "VARCHAR NOT NULL"}};

void BindEncryptedStringToColumn(sql::Statement* s,
                                 int column_index,
                                 const std::string& value,
                                 const os_crypt_async::Encryptor& encryptor) {
  std::string encrypted_data;
  std::ignore = encryptor.EncryptString(value, &encrypted_data);
  s->BindBlob(column_index, encrypted_data);
}

void BindEncryptedU16StringToColumn(
    sql::Statement* s,
    int column_index,
    const std::u16string& value,
    const os_crypt_async::Encryptor& encryptor) {
  std::string encrypted_data;
  std::ignore = encryptor.EncryptString16(value, &encrypted_data);
  s->BindBlob(column_index, encrypted_data);
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               base::Time modification_date,
                               sql::Statement* s,
                               const os_crypt_async::Encryptor& encryptor) {
  DCHECK(base::Uuid::ParseCaseInsensitive(credit_card.guid()).is_valid());
  int index = 0;
  s->BindString(index++, credit_card.guid());

  for (FieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                         CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    s->BindString16(index++, Truncate(credit_card.GetRawInfo(type)));
  }
  BindEncryptedU16StringToColumn(
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
                                   base::Time modification_date,
                                   sql::Statement* s,
                                   const os_crypt_async::Encryptor& encryptor) {
  CHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  int index = 0;
  s->BindString(index++, guid);

  BindEncryptedU16StringToColumn(s, index++, cvc, encryptor);
  s->BindInt64(index++, modification_date.ToTimeT());
}

void BindServerCvcToStatement(const ServerCvc& server_cvc,
                              const os_crypt_async::Encryptor& encryptor,
                              sql::Statement* s) {
  int index = 0;
  s->BindInt64(index++, server_cvc.instrument_id);
  BindEncryptedU16StringToColumn(s, index++, server_cvc.cvc, encryptor);
  s->BindInt64(index++, server_cvc.last_updated_timestamp.ToTimeT());
}

void BindMaskedBankAccountToStatement(const BankAccount& bank_account,
                                      sql::Statement* s) {
  int index = 0;
  s->BindInt64(index++, bank_account.payment_instrument().instrument_id());
  s->BindString16(index++, bank_account.bank_name());
  s->BindString16(index++, bank_account.account_number_suffix());
  s->BindInt(index++, static_cast<int>(bank_account.account_type()));
  s->BindString16(index++, bank_account.payment_instrument().nickname());
  s->BindString(index++,
                bank_account.payment_instrument().display_icon_url().spec());
}

void BindIbanToStatement(const Iban& iban,
                         sql::Statement* s,
                         const os_crypt_async::Encryptor& encryptor) {
  DCHECK(base::Uuid::ParseCaseInsensitive(iban.guid()).is_valid());
  int index = 0;
  s->BindString(index++, iban.guid());

  s->BindInt64(index++, iban.use_count());
  s->BindInt64(index++, iban.use_date().ToTimeT());

  BindEncryptedU16StringToColumn(s, index++, iban.value(), encryptor);
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

void BindPaymentInstrumentToStatement(
    const sync_pb::PaymentInstrument& payment_instrument,
    sql::Statement* s,
    const os_crypt_async::Encryptor& encryptor) {
  int index = 0;
  s->BindInt64(index++, payment_instrument.instrument_id());
  BindEncryptedStringToColumn(
      s, index++, payment_instrument.SerializeAsString(), encryptor);
}

VirtualCardUsageData GetVirtualCardUsageDataFromStatement(sql::Statement& s) {
  int index = 0;
  std::string id = s.ColumnString(index++);
  int64_t instrument_id = s.ColumnInt64(index++);
  std::string merchant_domain = s.ColumnString(index++);
  std::u16string last_four = s.ColumnString16(index++);

  return {VirtualCardUsageData::UsageDataId(id),
          VirtualCardUsageData::InstrumentId(instrument_id),
          VirtualCardUsageData::VirtualCardLastFour(last_four),
          url::Origin::Create(GURL(merchant_domain))};
}

std::string DecryptStringFromColumn(
    sql::Statement& s,
    int column_index,
    const os_crypt_async::Encryptor& encryptor) {
  std::string value;
  std::string encrypted_value;
  s.ColumnBlobAsString(column_index, &encrypted_value);
  if (!encrypted_value.empty()) {
    std::ignore = encryptor.DecryptString(encrypted_value, &value);
  }
  return value;
}

std::u16string DecryptU16StringFromColumn(
    sql::Statement& s,
    int column_index,
    const os_crypt_async::Encryptor& encryptor) {
  std::u16string value;
  std::string encrypted_value;
  s.ColumnBlobAsString(column_index, &encrypted_value);
  if (!encrypted_value.empty()) {
    std::ignore = encryptor.DecryptString16(encrypted_value, &value);
  }
  return value;
}

std::unique_ptr<CreditCard> CreditCardFromStatement(
    sql::Statement& card_statement,
    std::optional<std::reference_wrapper<sql::Statement>> cvc_statement,
    const os_crypt_async::Encryptor& encryptor) {
  auto credit_card = std::make_unique<CreditCard>();

  int index = 0;
  credit_card->set_guid(card_statement.ColumnString(index++));
  DCHECK(base::Uuid::ParseCaseInsensitive(credit_card->guid()).is_valid());

  for (FieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                         CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    credit_card->SetRawInfo(type, card_statement.ColumnString16(index++));
  }
  credit_card->SetRawInfo(
      CREDIT_CARD_NUMBER,
      DecryptU16StringFromColumn(card_statement, index++, encryptor));
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
        DecryptU16StringFromColumn(cvc_statement.value(), 0, encryptor));
    credit_card->set_cvc_modification_date(
        base::Time::FromTimeT(cvc_statement->get().ColumnInt64(1)));
  }
  return credit_card;
}

std::unique_ptr<ServerCvc> ServerCvcFromStatement(
    sql::Statement& s,
    const os_crypt_async::Encryptor& encryptor) {
  return std::make_unique<ServerCvc>(ServerCvc{
      .instrument_id = s.ColumnInt64(0),
      .cvc = DecryptU16StringFromColumn(s, 1, encryptor),
      .last_updated_timestamp = base::Time::FromTimeT(s.ColumnInt64(2))});
}

std::unique_ptr<Iban> IbanFromStatement(
    sql::Statement& s,
    const os_crypt_async::Encryptor& encryptor) {
  int index = 0;
  auto iban = std::make_unique<Iban>(Iban::Guid(s.ColumnString(index++)));

  DCHECK(base::Uuid::ParseCaseInsensitive(iban->guid()).is_valid());
  iban->set_use_count(s.ColumnInt64(index++));
  iban->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));

  iban->SetRawInfo(IBAN_VALUE,
                   DecryptU16StringFromColumn(s, index++, encryptor));
  iban->set_nickname(s.ColumnString16(index++));
  return iban;
}

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

PaymentsAutofillTable::PaymentsAutofillTable() = default;
PaymentsAutofillTable::~PaymentsAutofillTable() = default;

// static
PaymentsAutofillTable* PaymentsAutofillTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<PaymentsAutofillTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey PaymentsAutofillTable::GetTypeKey() const {
  return GetKey();
}

bool PaymentsAutofillTable::CreateTablesIfNecessary() {
  return InitCreditCardsTable() && InitLocalIbansTable() &&
         InitMaskedCreditCardsTable() && InitServerCardMetadataTable() &&
         InitPaymentsCustomerDataTable() &&
         InitServerCreditCardCloudTokenDataTable() && InitOfferDataTable() &&
         InitOfferEligibleInstrumentTable() && InitOfferMerchantDomainTable() &&
         InitVirtualCardUsageDataTable() && InitStoredCvcTable() &&
         InitMaskedBankAccountsTable() &&
         InitMaskedBankAccountsMetadataTable() && InitMaskedIbansTable() &&
         InitMaskedIbansMetadataTable() &&
         InitMaskedCreditCardBenefitsTable() &&
         InitBenefitMerchantDomainsTable() &&
         InitGenericPaymentInstrumentsTable();
}

bool PaymentsAutofillTable::MigrateToVersion(int version,
                                             bool* update_compatible_version) {
  if (!db()->is_open()) {
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
    case 87:
      *update_compatible_version = false;
      return MigrateToVersion87AddCreditCardNicknameColumn();
    case 89:
      *update_compatible_version = false;
      return MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard();
    case 94:
      *update_compatible_version = false;
      return MigrateToVersion94AddPromoCodeColumnsToOfferData();
    case 95:
      *update_compatible_version = false;
      return MigrateToVersion95AddVirtualCardMetadata();
    case 98:
      *update_compatible_version = true;
      return MigrateToVersion98RemoveStatusColumnMaskedCreditCards();
    case 101:
      // update_compatible_version is set to false because this table is not
      // used since M99.
      *update_compatible_version = false;
      return MigrateToVersion101RemoveCreditCardArtImageTable();
    case 104:
      *update_compatible_version = false;
      return MigrateToVersion104AddProductDescriptionColumn();
    case 105:
      *update_compatible_version = false;
      return MigrateToVersion105AddAutofillIbanTable();
    case 106:
      *update_compatible_version = true;
      return MigrateToVersion106RecreateAutofillIbanTable();
    case 108:
      *update_compatible_version = false;
      return MigrateToVersion108AddCardIssuerIdColumn();
    case 109:
      *update_compatible_version = false;
      return MigrateToVersion109AddVirtualCardUsageDataTable();
    case 111:
      *update_compatible_version = false;
      return MigrateToVersion111AddVirtualCardEnrollmentTypeColumn();
    case 115:
      *update_compatible_version = true;
      return MigrateToVersion115EncryptIbanValue();
    case 116:
      *update_compatible_version = false;
      return MigrateToVersion116AddStoredCvcTable();
    case 118:
      *update_compatible_version = true;
      return MigrateToVersion118RemovePaymentsUpiVpaTable();
    case 119:
      *update_compatible_version = true;
      return MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable();
    case 123:
      *update_compatible_version = false;
      return MigrateToVersion123AddProductTermsUrlColumnAndAddCardBenefitsTables();
    case 124:
      *update_compatible_version = true;
      return MigrateToVersion124AndDeletePaymentInstrumentRelatedTablesAndAddMaskedBankAccountTable();
    case 125:
      *update_compatible_version = true;
      return MigrateToVersion125DeleteFullServerCardsTable();
    case 129:
      *update_compatible_version = false;
      return MigrateToVersion129AddGenericPaymentInstrumentsTable();
    case 131:
      *update_compatible_version = true;
      return MigrateToVersion131RemoveGenericPaymentInstrumentTypeColumn();
    case 133:
      *update_compatible_version = true;
      return MigrateToVersion133RemoveLengthColumnFromMaskedIbansTable();
  }
  return true;
}

bool PaymentsAutofillTable::SetMaskedBankAccounts(
    const std::vector<BankAccount>& bank_accounts) {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  // Deletes all old values.
  Delete(db(), kMaskedBankAccountsTable);

  // Add bank accounts.
  sql::Statement insert;
  InsertBuilder(db(), insert, kMaskedBankAccountsTable,
                {kInstrumentId, kBankName, kAccountNumberSuffix, kAccountType,
                 kNickname, kDisplayIconUrl});
  for (BankAccount bank_account : bank_accounts) {
    BindMaskedBankAccountToStatement(bank_account, &insert);
    if (!insert.Run()) {
      return false;
    }
    insert.Reset(/*clear_bound_vars=*/true);
  }
  return transaction.Commit();
}

bool PaymentsAutofillTable::GetMaskedBankAccounts(
    std::vector<BankAccount>& bank_accounts) {
  sql::Statement s;
  bank_accounts.clear();

  SelectBuilder(db(), s, kMaskedBankAccountsTable,
                {kInstrumentId, kBankName, kAccountNumberSuffix, kAccountType,
                 kNickname, kDisplayIconUrl});
  while (s.Step()) {
    int index = 0;
    auto instrument_id = s.ColumnInt64(index++);
    auto bank_name = s.ColumnString16(index++);
    auto account_number_suffix = s.ColumnString16(index++);
    int account_type = s.ColumnInt(index++);
    auto nickname = s.ColumnString16(index++);
    auto display_icon_url = s.ColumnString16(index++);
    if (account_type >
            static_cast<int>(BankAccount::AccountType::kTransactingAccount) ||
        account_type < static_cast<int>(BankAccount::AccountType::kUnknown)) {
      continue;
    }
    bank_accounts.push_back(
        BankAccount(instrument_id, nickname, GURL(display_icon_url), bank_name,
                    account_number_suffix,
                    static_cast<BankAccount::AccountType>(account_type)));
  }
  return s.Succeeded();
}

bool PaymentsAutofillTable::AddLocalIban(const Iban& iban) {
  sql::Statement s;
  InsertBuilder(db(), s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname});
  BindIbanToStatement(iban, &s, *encryptor());
  if (!s.Run())
    return false;

  DCHECK_GT(db()->GetLastChangeCount(), 0);
  return true;
}

bool PaymentsAutofillTable::UpdateLocalIban(const Iban& iban) {
  DCHECK(base::Uuid::ParseCaseInsensitive(iban.guid()).is_valid());

  std::unique_ptr<Iban> old_iban = GetLocalIban(iban.guid());
  if (!old_iban) {
    return false;
  }

  if (*old_iban == iban) {
    return true;
  }

  sql::Statement s;
  UpdateBuilder(db(), s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname},
                "guid=?1");
  BindIbanToStatement(iban, &s, *encryptor());

  bool result = s.Run();
  DCHECK_GT(db()->GetLastChangeCount(), 0);
  return result;
}

bool PaymentsAutofillTable::RemoveLocalIban(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  return DeleteWhereColumnEq(db(), kLocalIbansTable, kGuid, guid);
}

std::unique_ptr<Iban> PaymentsAutofillTable::GetLocalIban(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Statement s;
  SelectBuilder(db(), s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname},
                "WHERE guid = ?");
  s.BindString(0, guid);

  if (!s.Step())
    return nullptr;

  return IbanFromStatement(s, *encryptor());
}

bool PaymentsAutofillTable::GetLocalIbans(std::vector<std::unique_ptr<Iban>>* ibans) {
  DCHECK(ibans);
  ibans->clear();

  sql::Statement s;
  SelectBuilder(db(), s, kLocalIbansTable, {kGuid},
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

bool PaymentsAutofillTable::AddCreditCard(const CreditCard& credit_card) {
  // We have 2 independent DB operations:
  // 1. Insert a credit_card
  // 2. Insert a CVC.
  // We don't wrap these in a transaction because a credit_card without a CVC is
  // a valid record, we are OK that the CC is stored but the CVC fails silently.
  // We only return false if credit_card insert fails.
  sql::Statement card_statement;
  InsertBuilder(db(), card_statement, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname});
  BindCreditCardToStatement(credit_card, AutofillClock::Now(), &card_statement,
                            *encryptor());

  if (!card_statement.Run()) {
    return false;
  }

  DCHECK_GT(db()->GetLastChangeCount(), 0);

  // If credit card contains cvc, will store cvc in local_stored_cvc table.
  if (!credit_card.cvc().empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    sql::Statement cvc_statement;
    InsertBuilder(db(), cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp});
    BindLocalStoredCvcToStatement(credit_card.guid(), credit_card.cvc(),
                                  AutofillClock::Now(), &cvc_statement,
                                  *encryptor());
    cvc_statement.Run();
  }

  return true;
}

bool PaymentsAutofillTable::UpdateCreditCard(const CreditCard& credit_card) {
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
  UpdateBuilder(db(), card_statement, kCreditCardsTable,
                {kGuid, kNameOnCard, kExpirationMonth, kExpirationYear,
                 kCardNumberEncrypted, kUseCount, kUseDate, kDateModified,
                 kOrigin, kBillingAddressId, kNickname},
                "guid=?1");
  BindCreditCardToStatement(credit_card,
                            card_updated ? AutofillClock::Now()
                                         : old_credit_card->modification_date(),
                            &card_statement, *encryptor());
  bool card_result = card_statement.Run();
  CHECK(db()->GetLastChangeCount() > 0);

  return cvc_result || card_result;
}

bool PaymentsAutofillTable::UpdateLocalCvc(const std::string& guid,
                                   const std::u16string& cvc) {
  std::unique_ptr<CreditCard> old_credit_card = GetCreditCard(guid);
  CHECK(old_credit_card);
  if (old_credit_card->cvc() == cvc) {
    return false;
  }
  if (cvc.empty()) {
    // Delete the CVC record if the new CVC is empty.
    return DeleteWhereColumnEq(db(), kLocalStoredCvcTable, kGuid, guid);
  }
  sql::Statement cvc_statement;
  // If existing card doesn't have CVC, we will insert CVC into
  // `kLocalStoredCvcTable` table. If existing card does have CVC, we will
  // update CVC for `kLocalStoredCvcTable` table.
  if (old_credit_card->cvc().empty()) {
    InsertBuilder(db(), cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp});
  } else {
    UpdateBuilder(db(), cvc_statement, kLocalStoredCvcTable,
                  {kGuid, kValueEncrypted, kLastUpdatedTimestamp}, "guid=?1");
  }
  BindLocalStoredCvcToStatement(guid, cvc, AutofillClock::Now(), &cvc_statement,
                                *encryptor());
  bool cvc_result = cvc_statement.Run();
  CHECK(db()->GetLastChangeCount() > 0);
  return cvc_result;
}

bool PaymentsAutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  DeleteWhereColumnEq(db(), kLocalStoredCvcTable, kGuid, guid);
  return DeleteWhereColumnEq(db(), kCreditCardsTable, kGuid, guid);
}

bool PaymentsAutofillTable::AddServerCreditCardForTesting(
    const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kMaskedServerCard,
            credit_card.record_type());
  DCHECK(!credit_card.number().empty());
  DCHECK(!credit_card.server_id().empty());
  DCHECK(!credit_card.network().empty());

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromMaskedCreditCards(credit_card.server_id());

  AddMaskedCreditCards({credit_card});

  transaction.Commit();

  return db()->GetLastChangeCount() > 0;
}

std::unique_ptr<CreditCard> PaymentsAutofillTable::GetCreditCard(
    const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  sql::Statement card_statement;
  SelectBuilder(db(), card_statement, kCreditCardsTable,
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
  SelectBuilder(db(), cvc_statement, kLocalStoredCvcTable,
                {kValueEncrypted, kLastUpdatedTimestamp}, "WHERE guid = ?");
  cvc_statement.BindString(0, guid);

  bool has_cvc = cvc_statement.Step();
  return CreditCardFromStatement(
      card_statement,
      has_cvc
          ? std::optional<std::reference_wrapper<sql::Statement>>{cvc_statement}
          : std::nullopt,
      *encryptor());
}

bool PaymentsAutofillTable::GetCreditCards(
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s;
  SelectBuilder(db(), s, kCreditCardsTable, {kGuid},
                "ORDER BY date_modified DESC, guid");

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<CreditCard> credit_card = GetCreditCard(guid);
    if (!credit_card)
      return false;
    // Clear the CVC from the local `credit_card` entry if the CVC storage flag
    // is disabled. This ensures CVC is not deleted if a user toggles flags back
    // and forth, but is still inaccessible if the feature is disabled.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillEnableCvcStorageAndFilling)) {
      credit_card->clear_cvc();
    }
    credit_cards->push_back(std::move(credit_card));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::GetServerCreditCards(
    std::vector<std::unique_ptr<CreditCard>>& credit_cards) const {
  credit_cards.clear();
  auto instrument_to_cvc = base::MakeFlatMap<int64_t, ServerCvc>(
      GetAllServerCvcs(), {}, [](const std::unique_ptr<ServerCvc>& server_cvc) {
        return std::make_pair(server_cvc->instrument_id, *server_cvc);
      });

  sql::Statement s;
  SelectBuilder(
      db(), s, base::StrCat({kMaskedCreditCardsTable, " AS masked"}),
      {kLastFour, base::StrCat({"masked.", kId}),
       base::StrCat({"metadata.", kUseCount}),
       base::StrCat({"metadata.", kUseDate}), kNetwork, kNameOnCard, kExpMonth,
       kExpYear, base::StrCat({"metadata.", kBillingAddressId}), kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kVirtualCardEnrollmentType, kCardArtUrl,
       kProductDescription, kProductTermsUrl},
      "LEFT OUTER JOIN server_card_metadata AS metadata USING (id)");
  while (s.Step()) {
    int index = 0;

    std::u16string last_four = s.ColumnString16(index++);
    std::string server_id = s.ColumnString(index++);
    std::unique_ptr<CreditCard> card = std::make_unique<CreditCard>(
        CreditCard::RecordType::kMaskedServerCard, server_id);
    card->SetRawInfo(CREDIT_CARD_NUMBER, last_four);
    card->set_use_count(s.ColumnInt64(index++));
    card->set_use_date(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s.ColumnInt64(index++))));
    // Modification date is not tracked for server cards. Explicitly set it here
    // to override the default value of AutofillClock::Now().
    card->set_modification_date(base::Time());

    std::string card_network = s.ColumnString(index++);
    // The issuer network must be set after setting the number to override the
    // autodetected issuer network.
    card->SetNetworkForMaskedCard(card_network.c_str());

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
    card->set_product_terms_url(GURL(s.ColumnString(index++)));
    // Add CVC to the the `card` if the CVC storage flag is enabled.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableCvcStorageAndFilling)) {
      const ServerCvc& cvc = instrument_to_cvc[card->instrument_id()];
      card->set_cvc(cvc.cvc);
      card->set_cvc_modification_date(cvc.last_updated_timestamp);
    }
    credit_cards.push_back(std::move(card));
  }
  return s.Succeeded();
}

void PaymentsAutofillTable::SetServerCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db(), kMaskedCreditCardsTable);

  AddMaskedCreditCards(credit_cards);

  // Delete all items in the metadata table that aren't in the new set.
  Delete(db(), kServerCardMetadataTable,
         "id NOT IN (SELECT id FROM masked_credit_cards)");

  transaction.Commit();
}

bool PaymentsAutofillTable::AddServerCvc(const ServerCvc& server_cvc) {
  if (server_cvc.cvc.empty()) {
    return false;
  }

  sql::Statement s;
  InsertBuilder(db(), s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp});
  BindServerCvcToStatement(server_cvc, *encryptor(), &s);
  s.Run();
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::UpdateServerCvc(const ServerCvc& server_cvc) {
  sql::Statement s;
  UpdateBuilder(db(), s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp},
                "instrument_id=?1");
  BindServerCvcToStatement(server_cvc, *encryptor(), &s);
  s.Run();
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::RemoveServerCvc(int64_t instrument_id) {
  DeleteWhereColumnEq(db(), kServerStoredCvcTable, kInstrumentId,
                      instrument_id);
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::ClearServerCvcs() {
  Delete(db(), kServerStoredCvcTable);
  return db()->GetLastChangeCount() > 0;
}

std::vector<std::unique_ptr<ServerCvc>>
PaymentsAutofillTable::DeleteOrphanedServerCvcs() {
  std::vector<std::unique_ptr<ServerCvc>> cvcs_to_be_deleted;
  sql::Statement s(db()->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kServerStoredCvcTable, " WHERE ",
                    kInstrumentId, " NOT IN (SELECT ", kInstrumentId, " FROM ",
                    kMaskedCreditCardsTable, ") RETURNING *"})));
  while (s.Step()) {
    cvcs_to_be_deleted.push_back(ServerCvcFromStatement(s, *encryptor()));
  }
  return cvcs_to_be_deleted;
}

std::vector<std::unique_ptr<ServerCvc>> PaymentsAutofillTable::GetAllServerCvcs()
    const {
  std::vector<std::unique_ptr<ServerCvc>> cvcs;
  sql::Statement s;
  SelectBuilder(db(), s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp});
  while (s.Step()) {
    cvcs.push_back(ServerCvcFromStatement(s, *encryptor()));
  }
  return cvcs;
}

bool PaymentsAutofillTable::ClearLocalCvcs() {
  Delete(db(), kLocalStoredCvcTable);
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::AddServerCardMetadata(
    const PaymentsMetadata& card_metadata) {
  sql::Statement s;
  InsertBuilder(db(), s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, card_metadata.use_count);
  s.BindTime(1, card_metadata.use_date);
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::UpdateServerCardMetadata(const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::RecordType::kLocalCard, credit_card.record_type());

  DeleteWhereColumnEq(db(), kServerCardMetadataTable, kId,
                      credit_card.server_id());

  sql::Statement s;
  InsertBuilder(db(), s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, credit_card.use_count());
  s.BindTime(1, credit_card.use_date());
  s.BindString(2, credit_card.billing_address_id());
  s.BindString(3, credit_card.server_id());
  s.Run();

  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::UpdateServerCardMetadata(
    const PaymentsMetadata& card_metadata) {
  // Do not check if there was a record that got deleted. Inserting a new one is
  // also fine.
  RemoveServerCardMetadata(card_metadata.id);
  sql::Statement s;
  InsertBuilder(db(), s, kServerCardMetadataTable,
                {kUseCount, kUseDate, kBillingAddressId, kId});
  s.BindInt64(0, card_metadata.use_count);
  s.BindTime(1, card_metadata.use_date);
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::RemoveServerCardMetadata(const std::string& id) {
  DeleteWhereColumnEq(db(), kServerCardMetadataTable, kId, id);
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::GetServerCardsMetadata(
    std::vector<PaymentsMetadata>& cards_metadata) const {
  cards_metadata.clear();

  sql::Statement s;
  SelectBuilder(db(), s, kServerCardMetadataTable,
                {kId, kUseCount, kUseDate, kBillingAddressId});

  while (s.Step()) {
    int index = 0;

    PaymentsMetadata card_metadata;
    card_metadata.id = s.ColumnString(index++);
    card_metadata.use_count = s.ColumnInt64(index++);
    card_metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s.ColumnInt64(index++)));
    card_metadata.billing_address_id = s.ColumnString(index++);
    cards_metadata.push_back(card_metadata);
  }
  return s.Succeeded();
}

bool PaymentsAutofillTable::AddOrUpdateServerIbanMetadata(
    const PaymentsMetadata& iban_metadata) {
  // There's no need to verify if removal succeeded, because if it's a new IBAN,
  // the removal call won't do anything.
  RemoveServerIbanMetadata(iban_metadata.id);

  sql::Statement s;
  InsertBuilder(db(), s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});
  s.BindString(0, iban_metadata.id);
  s.BindInt64(1, iban_metadata.use_count);
  s.BindTime(2, iban_metadata.use_date);
  s.Run();

  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::RemoveServerIbanMetadata(const std::string& instrument_id) {
  DeleteWhereColumnEq(db(), kMaskedIbansMetadataTable, kInstrumentId,
                      instrument_id);
  return db()->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::GetServerIbansMetadata(
    std::vector<PaymentsMetadata>& ibans_metadata) const {
  ibans_metadata.clear();
  sql::Statement s;
  SelectBuilder(db(), s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});

  while (s.Step()) {
    int index = 0;
    PaymentsMetadata iban_metadata;
    iban_metadata.id = s.ColumnString(index++);
    iban_metadata.use_count = s.ColumnInt64(index++);
    iban_metadata.use_date = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds((s.ColumnInt64(index++))));
    ibans_metadata.push_back(iban_metadata);
  }
  return s.Succeeded();
}

void PaymentsAutofillTable::SetServerCardsData(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db(), kMaskedCreditCardsTable);

  // Add all the masked cards.
  sql::Statement masked_insert;
  InsertBuilder(
      db(), masked_insert, kMaskedCreditCardsTable,
      {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear, kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kVirtualCardEnrollmentType, kCardArtUrl,
       kProductDescription, kProductTermsUrl});

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
    masked_insert.BindString(index++, card.product_terms_url().spec());
    masked_insert.Run();
    masked_insert.Reset(/*clear_bound_vars=*/true);
  }

  transaction.Commit();
}

void PaymentsAutofillTable::SetCreditCardCloudTokenData(
    const std::vector<CreditCardCloudTokenData>& credit_card_cloud_token_data) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Deletes all old values.
  Delete(db(), kServerCardCloudTokenDataTable);

  // Inserts new values.
  sql::Statement insert_cloud_token;
  InsertBuilder(
      db(), insert_cloud_token, kServerCardCloudTokenDataTable,
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

bool PaymentsAutofillTable::GetCreditCardCloudTokenData(
    std::vector<std::unique_ptr<CreditCardCloudTokenData>>&
        credit_card_cloud_token_data) {
  credit_card_cloud_token_data.clear();

  sql::Statement s;
  SelectBuilder(
      db(), s, kServerCardCloudTokenDataTable,
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
    credit_card_cloud_token_data.push_back(std::move(data));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::GetServerIbans(std::vector<std::unique_ptr<Iban>>& ibans) {
  sql::Statement s;
  SelectBuilder(
      db(), s, kMaskedIbansTable,
      {kInstrumentId, kUseCount, kUseDate, kNickname, kPrefix, kSuffix},
      "LEFT OUTER JOIN masked_ibans_metadata USING (instrument_id)");

  ibans.clear();
  while (s.Step()) {
    int index = 0;
    int64_t instrument_id = 0;
    if (!base::StringToInt64(s.ColumnString(index++), &instrument_id)) {
      continue;
    }
    std::unique_ptr<Iban> iban =
        std::make_unique<Iban>(Iban::InstrumentId(instrument_id));
    iban->set_use_count(s.ColumnInt64(index++));
    iban->set_use_date(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s.ColumnInt64(index++))));
    iban->set_nickname(s.ColumnString16(index++));
    iban->set_prefix(s.ColumnString16(index++));
    iban->set_suffix(s.ColumnString16(index++));
    ibans.push_back(std::move(iban));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::SetServerIbansData(const std::vector<Iban>& ibans) {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  // Delete all old ones first.
  Delete(db(), kMaskedIbansTable);

  sql::Statement s;
  InsertBuilder(db(), s, kMaskedIbansTable,
                {kInstrumentId, kNickname, kPrefix, kSuffix});
  for (const Iban& iban : ibans) {
    CHECK_EQ(Iban::RecordType::kServerIban, iban.record_type());
    int index = 0;
    s.BindString(index++, base::NumberToString(iban.instrument_id()));
    s.BindString16(index++, iban.nickname());
    s.BindString16(index++, iban.prefix());
    s.BindString16(index++, iban.suffix());
    if (!s.Run()) {
      return false;
    }
    s.Reset(/*clear_bound_vars=*/true);
  }
  return transaction.Commit();
}

void PaymentsAutofillTable::SetServerIbansForTesting(const std::vector<Iban>& ibans) {
  Delete(db(), kMaskedIbansMetadataTable);
  SetServerIbansData(ibans);
  for (const Iban& iban : ibans) {
    AddOrUpdateServerIbanMetadata(iban.GetMetadata());
  }
}

void PaymentsAutofillTable::SetPaymentsCustomerData(
    const PaymentsCustomerData* customer_data) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db(), kPaymentsCustomerDataTable);

  if (customer_data) {
    sql::Statement insert_customer_data;
    InsertBuilder(db(), insert_customer_data, kPaymentsCustomerDataTable,
                  {kCustomerId});
    insert_customer_data.BindString(0, customer_data->customer_id);
    insert_customer_data.Run();
  }

  transaction.Commit();
}

bool PaymentsAutofillTable::GetPaymentsCustomerData(
    std::unique_ptr<PaymentsCustomerData>& customer_data) const {
  sql::Statement s;
  SelectBuilder(db(), s, kPaymentsCustomerDataTable, {kCustomerId});
  if (s.Step()) {
    customer_data = std::make_unique<PaymentsCustomerData>(
        /*customer_id=*/s.ColumnString(0));
  }

  return s.Succeeded();
}

void PaymentsAutofillTable::SetAutofillOffers(
    const std::vector<AutofillOfferData>& autofill_offer_data) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db(), kOfferDataTable);
  Delete(db(), kOfferEligibleInstrumentTable);
  Delete(db(), kOfferMerchantDomainTable);

  // Insert new values.
  sql::Statement insert_offers;
  InsertBuilder(
      db(), insert_offers, kOfferDataTable,
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
      InsertBuilder(db(), insert_offer_eligible_instruments,
                    kOfferEligibleInstrumentTable, {kOfferId, kInstrumentId});
      insert_offer_eligible_instruments.BindInt64(0, data.GetOfferId());
      insert_offer_eligible_instruments.BindInt64(1, instrument_id);
      insert_offer_eligible_instruments.Run();
    }

    for (const GURL& merchant_origin : data.GetMerchantOrigins()) {
      // Insert new offer_merchant_domain values.
      sql::Statement insert_offer_merchant_domains;
      InsertBuilder(db(), insert_offer_merchant_domains,
                    kOfferMerchantDomainTable, {kOfferId, kMerchantDomain});
      insert_offer_merchant_domains.BindInt64(0, data.GetOfferId());
      insert_offer_merchant_domains.BindString(1, merchant_origin.spec());
      insert_offer_merchant_domains.Run();
    }
  }
  transaction.Commit();
}

bool PaymentsAutofillTable::GetAutofillOffers(
    std::vector<std::unique_ptr<AutofillOfferData>>* autofill_offer_data) {
  autofill_offer_data->clear();

  sql::Statement s;
  SelectBuilder(
      db(), s, kOfferDataTable,
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
    SelectBuilder(db(), s_offer_eligible_instrument,
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
    SelectBuilder(db(), s_offer_merchant_domain, kOfferMerchantDomainTable,
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

bool PaymentsAutofillTable::AddOrUpdateVirtualCardUsageData(
    const VirtualCardUsageData& virtual_card_usage_data) {
  std::optional<VirtualCardUsageData> existing_data =
      GetVirtualCardUsageData(*virtual_card_usage_data.usage_data_id());
  sql::Statement s;
  if (!existing_data) {
    InsertBuilder(db(), s, kVirtualCardUsageDataTable,
                  {kId, kInstrumentId, kMerchantDomain, kLastFour});
  } else {
    UpdateBuilder(db(), s, kVirtualCardUsageDataTable,
                  {kId, kInstrumentId, kMerchantDomain, kLastFour}, "id=?1");
  }
  BindVirtualCardUsageDataToStatement(virtual_card_usage_data, s);
  return s.Run();
}

std::optional<VirtualCardUsageData>
PaymentsAutofillTable::GetVirtualCardUsageData(
    const std::string& usage_data_id) {
  sql::Statement s;
  SelectBuilder(db(), s, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour},
                "WHERE id = ?");
  s.BindString(0, usage_data_id);
  if (!s.Step()) {
    return std::nullopt;
  }
  return GetVirtualCardUsageDataFromStatement(s);
}

bool PaymentsAutofillTable::RemoveVirtualCardUsageData(
    const std::string& usage_data_id) {
  if (!GetVirtualCardUsageData(usage_data_id)) {
    return false;
  }

  return DeleteWhereColumnEq(db(), kVirtualCardUsageDataTable, kId,
                             usage_data_id);
}

void PaymentsAutofillTable::SetVirtualCardUsageData(
    const std::vector<VirtualCardUsageData>& virtual_card_usage_data) {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return;
  }

  // Delete old data.
  Delete(db(), kVirtualCardUsageDataTable);
  // Insert new values.
  sql::Statement insert_data;
  InsertBuilder(db(), insert_data, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});
  for (const VirtualCardUsageData& data : virtual_card_usage_data) {
    BindVirtualCardUsageDataToStatement(data, insert_data);
    insert_data.Run();
    insert_data.Reset(/*clear_bound_vars=*/true);
  }
  transaction.Commit();
}

bool PaymentsAutofillTable::GetAllVirtualCardUsageData(
    std::vector<VirtualCardUsageData>& virtual_card_usage_data) {
  virtual_card_usage_data.clear();

  sql::Statement s;
  SelectBuilder(db(), s, kVirtualCardUsageDataTable,
                {kId, kInstrumentId, kMerchantDomain, kLastFour});
  while (s.Step()) {
    virtual_card_usage_data.push_back(GetVirtualCardUsageDataFromStatement(s));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::RemoveAllVirtualCardUsageData() {
  return Delete(db(), kVirtualCardUsageDataTable);
}

bool PaymentsAutofillTable::ClearAllServerData() {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  bool changed = false;
  for (std::string_view table_name :
       {kMaskedCreditCardsTable, kMaskedIbansTable, kServerCardMetadataTable,
        kPaymentsCustomerDataTable, kServerCardCloudTokenDataTable,
        kOfferDataTable, kOfferEligibleInstrumentTable,
        kOfferMerchantDomainTable, kVirtualCardUsageDataTable,
        kMaskedCreditCardBenefitsTable, kBenefitMerchantDomainsTable,
        kMaskedBankAccountsTable, kMaskedBankAccountsMetadataTable,
        kGenericPaymentInstrumentsTable}) {
    Delete(db(), table_name);
    changed |= db()->GetLastChangeCount() > 0;
  }

  transaction.Commit();
  return changed;
}

bool PaymentsAutofillTable::SetCreditCardBenefits(
    const std::vector<CreditCardBenefit>& credit_card_benefits) {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  // Remove all old benefits to rewrite the benefit tables entirely.
  if (!ClearAllCreditCardBenefits()) {
    return false;
  }

  for (const CreditCardBenefit& credit_card_benefit : credit_card_benefits) {
    if (!absl::visit([](const auto& a) { return a.IsValidForWriteFromSync(); },
                     credit_card_benefit)) {
      continue;
    }
    const CreditCardBenefitBase& benefit_base = absl::visit(
        [](const auto& a) -> const CreditCardBenefitBase& { return a; },
        credit_card_benefit);

    int benefit_type =
        absl::visit(base::Overloaded{
                        // WARNING: Do not renumber, since the identifiers are
                        // stored in the database.
                        [](const CreditCardFlatRateBenefit&) { return 0; },
                        [](const CreditCardCategoryBenefit&) { return 1; },
                        [](const CreditCardMerchantBenefit&) { return 2; },
                        // Next free benefit type: 3.
                    },
                    credit_card_benefit);

    // Insert new card benefit data.
    sql::Statement insert_benefit;
    InsertBuilder(db(), insert_benefit, kMaskedCreditCardBenefitsTable,
                  {kBenefitId, kInstrumentId, kBenefitType, kBenefitCategory,
                   kBenefitDescription, kStartTime, kEndTime});
    int index = 0;
    insert_benefit.BindString(index++, *benefit_base.benefit_id());
    insert_benefit.BindInt64(index++,
                             *benefit_base.linked_card_instrument_id());
    insert_benefit.BindInt(index++, benefit_type);
    insert_benefit.BindInt(
        index++, base::to_underlying(absl::visit(
                     base::Overloaded{
                         [](const CreditCardCategoryBenefit& a) {
                           return a.benefit_category();
                         },
                         [](const auto& a) {
                           return CreditCardCategoryBenefit::BenefitCategory::
                               kUnknownBenefitCategory;
                         },
                     },
                     credit_card_benefit)));
    insert_benefit.BindString16(index++, benefit_base.benefit_description());
    insert_benefit.BindTime(index++, benefit_base.start_time());
    insert_benefit.BindTime(index++, benefit_base.expiry_time());
    if (!insert_benefit.Run()) {
      return false;
    }

    // Insert merchant domains linked with the benefit.
    if (const auto* merchant_benefit =
            absl::get_if<CreditCardMerchantBenefit>(&credit_card_benefit)) {
      for (const url::Origin& domain : merchant_benefit->merchant_domains()) {
        sql::Statement insert_benefit_merchant_domain;
        InsertBuilder(db(), insert_benefit_merchant_domain,
                      kBenefitMerchantDomainsTable,
                      {kBenefitId, kMerchantDomain});
        insert_benefit_merchant_domain.BindString(
            0, *merchant_benefit->benefit_id());
        insert_benefit_merchant_domain.BindString(1, domain.Serialize());
        if (!insert_benefit_merchant_domain.Run()) {
          return false;
        }
      }
    }
  }
  return transaction.Commit();
}

bool PaymentsAutofillTable::GetAllCreditCardBenefits(
    std::vector<CreditCardBenefit>& credit_card_benefits) {
  return GetCreditCardBenefitsForInstrumentId(std::nullopt,
                                              credit_card_benefits);
}

bool PaymentsAutofillTable::GetCreditCardBenefitsForInstrumentId(
    const std::optional<int64_t> instrument_id,
    std::vector<CreditCardBenefit>& credit_card_benefits) {
  sql::Statement get_benefits;
  std::string statement_modifiers =
      instrument_id ? base::StrCat({"WHERE instrument_id = ",
                                    base::NumberToString(*instrument_id)})
                    : "";
  SelectBuilder(db(), get_benefits, kMaskedCreditCardBenefitsTable,
                {kBenefitId, kInstrumentId, kBenefitType, kBenefitDescription,
                 kStartTime, kEndTime, kBenefitCategory},
                statement_modifiers);

  while (get_benefits.Step()) {
    int index = 0;
    CreditCardBenefitBase::BenefitId benefit_id(
        get_benefits.ColumnString(index++));
    CreditCardBenefitBase::LinkedCardInstrumentId linked_card_instrument_id(
        get_benefits.ColumnInt64(index++));
    int benefit_type = get_benefits.ColumnInt(index++);
    std::u16string benefit_description = get_benefits.ColumnString16(index++);
    base::Time start_time = get_benefits.ColumnTime(index++);
    base::Time expiry_time = get_benefits.ColumnTime(index++);
    CreditCardCategoryBenefit::BenefitCategory benefit_category =
        static_cast<CreditCardCategoryBenefit::BenefitCategory>(
            get_benefits.ColumnInt(index++));

    switch (benefit_type) {
      case 0:
        credit_card_benefits.push_back(CreditCardFlatRateBenefit(
            benefit_id, linked_card_instrument_id, benefit_description,
            start_time, expiry_time));
        break;
      case 1:
        credit_card_benefits.push_back(CreditCardCategoryBenefit(
            benefit_id, linked_card_instrument_id, benefit_category,
            benefit_description, start_time, expiry_time));
        break;
      case 2:
        credit_card_benefits.push_back(CreditCardMerchantBenefit(
            benefit_id, linked_card_instrument_id, benefit_description,
            GetMerchantDomainsForBenefitId(benefit_id), start_time,
            expiry_time));
        break;
      default:
        LOG(ERROR) << "Invalid CreditCardBenefit of type " << benefit_type;
    }
  }

  return get_benefits.Succeeded();
}

bool PaymentsAutofillTable::ClearAllCreditCardBenefits() {
  sql::Transaction transaction(db());
  return transaction.Begin() && Delete(db(), kMaskedCreditCardBenefitsTable) &&
         Delete(db(), kBenefitMerchantDomainsTable) && transaction.Commit();
}

bool PaymentsAutofillTable::SetPaymentInstruments(
    const std::vector<sync_pb::PaymentInstrument>& payment_instruments) {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  // Delete the existing values.
  Delete(db(), kGenericPaymentInstrumentsTable);

  // Insert the new values.
  sql::Statement insert;
  InsertBuilder(db(), insert, kGenericPaymentInstrumentsTable,
                {kInstrumentId, kSerializedValueEncrypted});
  for (const sync_pb::PaymentInstrument& payment_instrument :
       payment_instruments) {
    BindPaymentInstrumentToStatement(payment_instrument, &insert, *encryptor());
    insert.Run();
    insert.Reset(/*clear_bound_vars=*/true);
  }

  return transaction.Commit();
}

bool PaymentsAutofillTable::GetPaymentInstruments(
    std::vector<sync_pb::PaymentInstrument>& payment_instruments) {
  payment_instruments.clear();

  sql::Statement s;
  SelectBuilder(db(), s, kGenericPaymentInstrumentsTable,
                {kInstrumentId, kSerializedValueEncrypted});

  while (s.Step()) {
    int index = 0;
    int64_t instrument_id = s.ColumnInt64(index++);
    auto serialized_value = DecryptStringFromColumn(s, index++, *encryptor());
    sync_pb::PaymentInstrument payment_instrument;
    if (payment_instrument.ParseFromString(serialized_value)) {
      payment_instruments.emplace_back(payment_instrument);
    } else {
      DLOG(WARNING)
          << "Instrument dropped: Failed to deserialize AUTOFILL data type "
             "sync_pb::PaymentInstrument with id = "
          << instrument_id;
    }
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::MigrateToVersion83RemoveServerCardTypeColumn() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DropColumn(db(), kMaskedCreditCardsTable, "type") &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion84AddNicknameColumn() {
  // Add the nickname column to the masked_credit_cards table.
  return AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kNickname,
                              "VARCHAR");
}

bool PaymentsAutofillTable::
    MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard() {
  // Add the new card_issuer column to the masked_credit_cards table and set
  // the default value to ISSUER_UNKNOWN.
  return AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kCardIssuer,
                              "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion87AddCreditCardNicknameColumn() {
  // Add the nickname column to the credit_card table.
  return AddColumnIfNotExists(db(), kCreditCardsTable, kNickname, "VARCHAR");
}

bool PaymentsAutofillTable::
    MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard() {
  // Add the new instrument_id column to the masked_credit_cards table and set
  // the default value to 0.
  return AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kInstrumentId,
                              "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion94AddPromoCodeColumnsToOfferData() {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  if (!db()->DoesTableExist(kOfferDataTable)) {
    InitOfferDataTable();
  }

  // Add the new promo_code and DisplayStrings text columns to the offer_data
  // table.
  for (std::string_view column :
       {kPromoCode, kValuePropText, kSeeDetailsText, kUsageInstructionsText}) {
    if (!AddColumnIfNotExists(db(), kOfferDataTable, column, "VARCHAR")) {
      return false;
    }
  }
  return transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion95AddVirtualCardMetadata() {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  if (!db()->DoesTableExist(kMaskedCreditCardsTable)) {
    InitMaskedCreditCardsTable();
  }

  // Add virtual_card_enrollment_state to masked_credit_cards.
  if (!AddColumnIfNotExists(db(), kMaskedCreditCardsTable,
                            kVirtualCardEnrollmentState, "INTEGER DEFAULT 0")) {
    return false;
  }

  // Add card_art_url to masked_credit_cards.
  if (!AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kCardArtUrl,
                            "VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool PaymentsAutofillTable::
    MigrateToVersion98RemoveStatusColumnMaskedCreditCards() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DropColumn(db(), kMaskedCreditCardsTable, kStatus) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion101RemoveCreditCardArtImageTable() {
  return DropTableIfExists(db(), "credit_card_art_images");
}

bool PaymentsAutofillTable::MigrateToVersion104AddProductDescriptionColumn() {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  if (!db()->DoesTableExist(kMaskedCreditCardsTable)) {
    InitMaskedCreditCardsTable();
  }

  // Add product_description to masked_credit_cards.
  if (!AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kProductDescription,
                            "VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion105AddAutofillIbanTable() {
  return CreateTable(db(), kIbansTable,
                     {{kGuid, "VARCHAR"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::MigrateToVersion106RecreateAutofillIbanTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() && DropTableIfExists(db(), kIbansTable) &&
         CreateTable(db(), kIbansTable,
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}}) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion108AddCardIssuerIdColumn() {
  // Add card_issuer_id to masked_credit_cards.
  return db()->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db(), kMaskedCreditCardsTable, kCardIssuerId,
                              "VARCHAR");
}

bool PaymentsAutofillTable::MigrateToVersion109AddVirtualCardUsageDataTable() {
  return CreateTable(db(), kVirtualCardUsageDataTable,
                     {{kId, "VARCHAR PRIMARY KEY"},
                      {kInstrumentId, "INTEGER DEFAULT 0"},
                      {kMerchantDomain, "VARCHAR"},
                      {kLastFour, "VARCHAR"}});
}

bool PaymentsAutofillTable::
    MigrateToVersion111AddVirtualCardEnrollmentTypeColumn() {
  return db()->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db(), kMaskedCreditCardsTable,
                              kVirtualCardEnrollmentType, "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion115EncryptIbanValue() {
  // Encrypt all existing IBAN values and rename the column name from `value`
  // to `value_encrypted` by the following steps:
  // 1. Read all existing guid and value data from `ibans`, encrypt all
  // values,
  //    and rewrite to `ibans`.
  // 2. Rename `value` column to `value_encrypted` for `ibans` table.
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }
  sql::Statement s;
  SelectBuilder(db(), s, kIbansTable, {kGuid, kValue});
  std::vector<std::pair<std::string, std::u16string>> iban_guid_to_value_pairs;
  while (s.Step()) {
    iban_guid_to_value_pairs.emplace_back(s.ColumnString(0),
                                          s.ColumnString16(1));
  }
  if (!s.Succeeded()) {
    return false;
  }

  for (const auto& [guid, value] : iban_guid_to_value_pairs) {
    UpdateBuilder(db(), s, kIbansTable, {kGuid, kValue}, "guid=?1");
    int index = 0;
    s.BindString(index++, guid);
    BindEncryptedU16StringToColumn(&s, index++, value, *encryptor());
    if (!s.Run()) {
      return false;
    }
  }

  return db()->Execute(
             base::StrCat({"ALTER TABLE ", kIbansTable, " RENAME COLUMN ",
                           kValue, " TO ", kValueEncrypted})) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion116AddStoredCvcTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         CreateTable(db(), kLocalStoredCvcTable,
                     {{kGuid, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kValueEncrypted, "VARCHAR NOT NULL"},
                      {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         CreateTable(db(), kServerStoredCvcTable,
                     {{kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
                      {kValueEncrypted, "VARCHAR NOT NULL"},
                      {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion118RemovePaymentsUpiVpaTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() && DropTableIfExists(db(), kPaymentsUpiVpaTable) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::
    MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         CreateTable(db(), kMaskedIbansTable,
                     {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kPrefix, "VARCHAR NOT NULL"},
                      {kSuffix, "VARCHAR NOT NULL"},
                      {"length", "INTEGER NOT NULL DEFAULT 0"},
                      {kNickname, "VARCHAR"}}) &&
         CreateTable(db(), kMaskedIbansMetadataTable,
                     {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"}}) &&
         (!db()->DoesTableExist(kIbansTable) ||
          RenameTable(db(), kIbansTable, kLocalIbansTable)) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::
    MigrateToVersion123AddProductTermsUrlColumnAndAddCardBenefitsTables() {
  sql::Transaction transaction(db());
  return transaction.Begin() && db()->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumn(db(), kMaskedCreditCardsTable, kProductTermsUrl,
                   "VARCHAR") &&
         CreateTable(db(), kMaskedCreditCardBenefitsTable,
                     kMaskedCreditCardBenefitsColumnNamesAndTypes) &&
         CreateTable(db(), kBenefitMerchantDomainsTable,
                     kBenefitMerchantDomainsColumnNamesAndTypes) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::
    MigrateToVersion124AndDeletePaymentInstrumentRelatedTablesAndAddMaskedBankAccountTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DropTableIfExists(db(), "payment_instruments") &&
         DropTableIfExists(db(), "payment_instruments_metadata") &&
         DropTableIfExists(db(), "bank_accounts") &&
         DropTableIfExists(db(), "payment_instrument_supported_rails") &&
         CreateTable(db(), kMaskedBankAccountsTable,
                     kMaskedBankAccountsColumnNamesAndTypes) &&
         CreateTable(db(), kMaskedBankAccountsMetadataTable,
                     kMaskedBankAccountsMetadataColumnNamesAndTypes) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion125DeleteFullServerCardsTable() {
  return DropTableIfExists(db(), "unmasked_credit_cards");
}

bool PaymentsAutofillTable::
    MigrateToVersion129AddGenericPaymentInstrumentsTable() {
  return CreateTable(db(), kGenericPaymentInstrumentsTable,
                     kGenericPaymentInstrumentsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::
    MigrateToVersion131RemoveGenericPaymentInstrumentTypeColumn() {
  return DropColumnIfExists(db(), kGenericPaymentInstrumentsTable,
                            "payment_instrument_type");
}

bool PaymentsAutofillTable::
    MigrateToVersion133RemoveLengthColumnFromMaskedIbansTable() {
  return DropColumnIfExists(db(), kMaskedIbansTable, "length");
}

void PaymentsAutofillTable::AddMaskedCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK_GT(db()->transaction_nesting(), 0);
  sql::Statement masked_insert;
  InsertBuilder(
      db(), masked_insert, kMaskedCreditCardsTable,
      {kId, kNetwork, kNameOnCard, kLastFour, kExpMonth, kExpYear, kBankName,
       kNickname, kCardIssuer, kCardIssuerId, kInstrumentId,
       kVirtualCardEnrollmentState, kVirtualCardEnrollmentType, kCardArtUrl,
       kProductDescription, kProductTermsUrl});

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
    masked_insert.BindString(index++, card.product_terms_url().spec());
    masked_insert.Run();
    masked_insert.Reset(/*clear_bound_vars=*/true);

    // Save the use count and use date of the card.
    UpdateServerCardMetadata(card);
  }
}

bool PaymentsAutofillTable::DeleteFromMaskedCreditCards(const std::string& id) {
  DeleteWhereColumnEq(db(), kMaskedCreditCardsTable, kId, id);
  return db()->GetLastChangeCount() > 0;
}

base::flat_set<url::Origin>
PaymentsAutofillTable::GetMerchantDomainsForBenefitId(
    const CreditCardBenefitBase::BenefitId& benefit_id) {
  base::flat_set<url::Origin> merchant_domains;
  sql::Statement s;
  SelectBuilder(db(), s, kBenefitMerchantDomainsTable, {kMerchantDomain},
                "WHERE benefit_id = ?");
  s.BindString(0, *benefit_id);
  while (s.Step()) {
    merchant_domains.insert(url::Origin::Create(GURL(s.ColumnString(0))));
  }
  return merchant_domains;
}

bool PaymentsAutofillTable::InitCreditCardsTable() {
  return CreateTableIfNotExists(db(), kCreditCardsTable,
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

bool PaymentsAutofillTable::InitLocalIbansTable() {
  return CreateTableIfNotExists(db(), kLocalIbansTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kValueEncrypted, "VARCHAR"},
                                 {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedCreditCardsTable() {
  return CreateTableIfNotExists(
      db(), kMaskedCreditCardsTable,
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
       {kVirtualCardEnrollmentType, "INTEGER DEFAULT 0"},
       {kProductTermsUrl, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedIbansTable() {
  return CreateTableIfNotExists(
      db(), kMaskedIbansTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kPrefix, "VARCHAR NOT NULL"},
       {kSuffix, "VARCHAR NOT NULL"},
       {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedIbansMetadataTable() {
  return CreateTableIfNotExists(
      db(), kMaskedIbansMetadataTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
       {kUseDate, "INTEGER NOT NULL DEFAULT 0"}});
}

bool PaymentsAutofillTable::InitServerCardMetadataTable() {
  return CreateTableIfNotExists(db(), kServerCardMetadataTable,
                                {{kId, "VARCHAR NOT NULL"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kBillingAddressId, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitPaymentsCustomerDataTable() {
  return CreateTableIfNotExists(db(), kPaymentsCustomerDataTable,
                                {{kCustomerId, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitServerCreditCardCloudTokenDataTable() {
  return CreateTableIfNotExists(db(), kServerCardCloudTokenDataTable,
                                {{kId, "VARCHAR"},
                                 {kSuffix, "VARCHAR"},
                                 {kExpMonth, "INTEGER DEFAULT 0"},
                                 {kExpYear, "INTEGER DEFAULT 0"},
                                 {kCardArtUrl, "VARCHAR"},
                                 {kInstrumentToken, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitStoredCvcTable() {
  return CreateTableIfNotExists(
             db(), kLocalStoredCvcTable,
             {{kGuid, "VARCHAR PRIMARY KEY NOT NULL"},
              {kValueEncrypted, "VARCHAR NOT NULL"},
              {kLastUpdatedTimestamp, "INTEGER NOT NULL"}}) &&
         CreateTableIfNotExists(
             db(), kServerStoredCvcTable,
             {{kInstrumentId, "INTEGER PRIMARY KEY NOT NULL"},
              {kValueEncrypted, "VARCHAR NOT NULL"},
              {kLastUpdatedTimestamp, "INTEGER NOT NULL"}});
}

bool PaymentsAutofillTable::InitOfferDataTable() {
  return CreateTableIfNotExists(db(), kOfferDataTable,
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

bool PaymentsAutofillTable::InitOfferEligibleInstrumentTable() {
  return CreateTableIfNotExists(
      db(), kOfferEligibleInstrumentTable,
      {{kOfferId, "UNSIGNED LONG"}, {kInstrumentId, "UNSIGNED LONG"}});
}

bool PaymentsAutofillTable::InitOfferMerchantDomainTable() {
  return CreateTableIfNotExists(
      db(), kOfferMerchantDomainTable,
      {{kOfferId, "UNSIGNED LONG"}, {kMerchantDomain, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitVirtualCardUsageDataTable() {
  return CreateTableIfNotExists(db(), kVirtualCardUsageDataTable,
                                {{kId, "VARCHAR PRIMARY KEY"},
                                 {kInstrumentId, "INTEGER DEFAULT 0"},
                                 {kMerchantDomain, "VARCHAR"},
                                 {kLastFour, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedBankAccountsTable() {
  return CreateTableIfNotExists(db(), kMaskedBankAccountsTable,
                                kMaskedBankAccountsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitMaskedBankAccountsMetadataTable() {
  return CreateTableIfNotExists(db(), kMaskedBankAccountsMetadataTable,
                                kMaskedBankAccountsMetadataColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitMaskedCreditCardBenefitsTable() {
  return CreateTableIfNotExists(db(), kMaskedCreditCardBenefitsTable,
                                kMaskedCreditCardBenefitsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitBenefitMerchantDomainsTable() {
  return CreateTableIfNotExists(db(), kBenefitMerchantDomainsTable,
                                kBenefitMerchantDomainsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitGenericPaymentInstrumentsTable() {
  return CreateTableIfNotExists(db(), kGenericPaymentInstrumentsTable,
                                kGenericPaymentInstrumentsColumnNamesAndTypes);
}

}  // namespace autofill
