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
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor_factory.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
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
constexpr std::string_view kLength = "length";
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

  for (FieldType type : {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
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
    std::optional<std::reference_wrapper<sql::Statement>> cvc_statement,
    const AutofillTableEncryptor& encryptor) {
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

}  // namespace

PaymentsAutofillTable::PaymentsAutofillTable()
    : autofill_table_encryptor_(
          AutofillTableEncryptorFactory::GetInstance()->Create()) {
  DCHECK(autofill_table_encryptor_);
}

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
         InitBenefitMerchantDomainsTable();
}

bool PaymentsAutofillTable::MigrateToVersion(int version,
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
  }
  return true;
}

bool PaymentsAutofillTable::SetMaskedBankAccounts(
    const std::vector<BankAccount>& bank_accounts) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Deletes all old values.
  Delete(db_, kMaskedBankAccountsTable);

  // Add bank accounts.
  sql::Statement insert;
  InsertBuilder(db_, insert, kMaskedBankAccountsTable,
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
    std::vector<std::unique_ptr<BankAccount>>& bank_accounts) {
  sql::Statement s;
  bank_accounts.clear();

  SelectBuilder(db_, s, kMaskedBankAccountsTable,
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
    bank_accounts.push_back(std::make_unique<BankAccount>(
        instrument_id, nickname, GURL(display_icon_url), bank_name,
        account_number_suffix,
        static_cast<BankAccount::AccountType>(account_type)));
  }
  return s.Succeeded();
}

bool PaymentsAutofillTable::AddLocalIban(const Iban& iban) {
  sql::Statement s;
  InsertBuilder(db_, s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname});
  BindIbanToStatement(iban, &s, *autofill_table_encryptor_);
  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
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
  UpdateBuilder(db_, s, kLocalIbansTable,
                {kGuid, kUseCount, kUseDate, kValueEncrypted, kNickname},
                "guid=?1");
  BindIbanToStatement(iban, &s, *autofill_table_encryptor_);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool PaymentsAutofillTable::RemoveLocalIban(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  return DeleteWhereColumnEq(db_, kLocalIbansTable, kGuid, guid);
}

std::unique_ptr<Iban> PaymentsAutofillTable::GetLocalIban(const std::string& guid) {
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

bool PaymentsAutofillTable::GetLocalIbans(std::vector<std::unique_ptr<Iban>>* ibans) {
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

bool PaymentsAutofillTable::AddCreditCard(const CreditCard& credit_card) {
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

bool PaymentsAutofillTable::UpdateLocalCvc(const std::string& guid,
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

bool PaymentsAutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
  DeleteWhereColumnEq(db_, kLocalStoredCvcTable, kGuid, guid);
  return DeleteWhereColumnEq(db_, kCreditCardsTable, kGuid, guid);
}

bool PaymentsAutofillTable::AddFullServerCreditCard(const CreditCard& credit_card) {
  // TODO(crbug.com/1497734): Remove this method entirely.
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

std::unique_ptr<CreditCard> PaymentsAutofillTable::GetCreditCard(
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
      has_cvc
          ? std::optional<std::reference_wrapper<sql::Statement>>{cvc_statement}
          : std::nullopt,
      *autofill_table_encryptor_);
}

bool PaymentsAutofillTable::GetCreditCards(
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

bool PaymentsAutofillTable::GetServerCreditCards(
    std::vector<std::unique_ptr<CreditCard>>& credit_cards) const {
  credit_cards.clear();
  auto instrument_to_cvc = base::MakeFlatMap<int64_t, std::u16string>(
      GetAllServerCvcs(), {}, [](const auto& server_cvc) {
        return std::make_pair(server_cvc->instrument_id, server_cvc->cvc);
      });

  sql::Statement s;
  SelectBuilder(
      db_, s, base::StrCat({kMaskedCreditCardsTable, " AS masked"}),
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
    card->set_use_date(base::Time::FromInternalValue(s.ColumnInt64(index++)));
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
    card->set_cvc(instrument_to_cvc[card->instrument_id()]);
    credit_cards.push_back(std::move(card));
  }
  return s.Succeeded();
}

void PaymentsAutofillTable::SetServerCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  Delete(db_, kMaskedCreditCardsTable);

  AddMaskedCreditCards(credit_cards);

  // Delete all items in the metadata table that aren't in the new set.
  Delete(db_, kServerCardMetadataTable,
         "id NOT IN (SELECT id FROM masked_credit_cards)");

  transaction.Commit();
}

bool PaymentsAutofillTable::UnmaskServerCreditCard(const CreditCard& masked,
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

bool PaymentsAutofillTable::MaskServerCreditCard(const std::string& id) {
  return DeleteFromUnmaskedCreditCards(id);
}

bool PaymentsAutofillTable::AddServerCvc(const ServerCvc& server_cvc) {
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

bool PaymentsAutofillTable::UpdateServerCvc(const ServerCvc& server_cvc) {
  sql::Statement s;
  UpdateBuilder(db_, s, kServerStoredCvcTable,
                {kInstrumentId, kValueEncrypted, kLastUpdatedTimestamp},
                "instrument_id=?1");
  BindServerCvcToStatement(server_cvc, *autofill_table_encryptor_, &s);
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::RemoveServerCvc(int64_t instrument_id) {
  DeleteWhereColumnEq(db_, kServerStoredCvcTable, kInstrumentId, instrument_id);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::ClearServerCvcs() {
  Delete(db_, kServerStoredCvcTable);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::ReconcileServerCvcs() {
  sql::Statement s(db_->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kServerStoredCvcTable, " WHERE ",
                    kInstrumentId, " NOT IN (SELECT ", kInstrumentId, " FROM ",
                    kMaskedCreditCardsTable, ")"})
          .c_str()));
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

std::vector<std::unique_ptr<ServerCvc>> PaymentsAutofillTable::GetAllServerCvcs()
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

bool PaymentsAutofillTable::ClearLocalCvcs() {
  Delete(db_, kLocalStoredCvcTable);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::AddServerCardMetadata(
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

bool PaymentsAutofillTable::UpdateServerCardMetadata(const CreditCard& credit_card) {
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

bool PaymentsAutofillTable::UpdateServerCardMetadata(
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

bool PaymentsAutofillTable::RemoveServerCardMetadata(const std::string& id) {
  DeleteWhereColumnEq(db_, kServerCardMetadataTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::GetServerCardsMetadata(
    std::vector<AutofillMetadata>& cards_metadata) const {
  cards_metadata.clear();

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
    cards_metadata.push_back(card_metadata);
  }
  return s.Succeeded();
}

bool PaymentsAutofillTable::AddOrUpdateServerIbanMetadata(
    const AutofillMetadata& iban_metadata) {
  // There's no need to verify if removal succeeded, because if it's a new IBAN,
  // the removal call won't do anything.
  RemoveServerIbanMetadata(iban_metadata.id);

  sql::Statement s;
  InsertBuilder(db_, s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});
  s.BindString(0, iban_metadata.id);
  s.BindInt64(1, iban_metadata.use_count);
  s.BindTime(2, iban_metadata.use_date);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::RemoveServerIbanMetadata(const std::string& instrument_id) {
  DeleteWhereColumnEq(db_, kMaskedIbansMetadataTable, kInstrumentId,
                      instrument_id);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::GetServerIbansMetadata(
    std::vector<AutofillMetadata>& ibans_metadata) const {
  ibans_metadata.clear();
  sql::Statement s;
  SelectBuilder(db_, s, kMaskedIbansMetadataTable,
                {kInstrumentId, kUseCount, kUseDate});

  while (s.Step()) {
    int index = 0;
    AutofillMetadata iban_metadata;
    iban_metadata.id = s.ColumnString(index++);
    iban_metadata.use_count = s.ColumnInt64(index++);
    iban_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    ibans_metadata.push_back(iban_metadata);
  }
  return s.Succeeded();
}

void PaymentsAutofillTable::SetServerCardsData(
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

bool PaymentsAutofillTable::GetCreditCardCloudTokenData(
    std::vector<std::unique_ptr<CreditCardCloudTokenData>>&
        credit_card_cloud_token_data) {
  credit_card_cloud_token_data.clear();

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
    credit_card_cloud_token_data.push_back(std::move(data));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::GetServerIbans(std::vector<std::unique_ptr<Iban>>& ibans) {
  sql::Statement s;
  SelectBuilder(db_, s, kMaskedIbansTable,
                {kInstrumentId, kUseCount, kUseDate, kNickname, kPrefix,
                 kSuffix, kLength},
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
    iban->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
    iban->set_nickname(s.ColumnString16(index++));
    iban->set_prefix(s.ColumnString16(index++));
    iban->set_suffix(s.ColumnString16(index++));
    iban->set_length(s.ColumnInt64(index++));
    ibans.push_back(std::move(iban));
  }

  return s.Succeeded();
}

bool PaymentsAutofillTable::SetServerIbansData(const std::vector<Iban>& ibans) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Delete all old ones first.
  Delete(db_, kMaskedIbansTable);

  sql::Statement s;
  InsertBuilder(db_, s, kMaskedIbansTable,
                {kInstrumentId, kNickname, kPrefix, kSuffix, kLength});
  for (const Iban& iban : ibans) {
    CHECK_EQ(Iban::RecordType::kServerIban, iban.record_type());
    int index = 0;
    s.BindString(index++, base::NumberToString(iban.instrument_id()));
    s.BindString16(index++, iban.nickname());
    s.BindString16(index++, iban.prefix());
    s.BindString16(index++, iban.suffix());
    s.BindInt64(index++, iban.length());
    if (!s.Run()) {
      return false;
    }
    s.Reset(/*clear_bound_vars=*/true);
  }
  return transaction.Commit();
}

void PaymentsAutofillTable::SetServerIbansForTesting(const std::vector<Iban>& ibans) {
  Delete(db_, kMaskedIbansMetadataTable);
  SetServerIbansData(ibans);
  for (const Iban& iban : ibans) {
    AddOrUpdateServerIbanMetadata(iban.GetMetadata());
  }
}

void PaymentsAutofillTable::SetPaymentsCustomerData(
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

bool PaymentsAutofillTable::GetPaymentsCustomerData(
    std::unique_ptr<PaymentsCustomerData>& customer_data) const {
  sql::Statement s;
  SelectBuilder(db_, s, kPaymentsCustomerDataTable, {kCustomerId});
  if (s.Step()) {
    customer_data = std::make_unique<PaymentsCustomerData>(
        /*customer_id=*/s.ColumnString(0));
  }

  return s.Succeeded();
}

void PaymentsAutofillTable::SetAutofillOffers(
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

bool PaymentsAutofillTable::GetAutofillOffers(
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

bool PaymentsAutofillTable::AddOrUpdateVirtualCardUsageData(
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

std::unique_ptr<VirtualCardUsageData> PaymentsAutofillTable::GetVirtualCardUsageData(
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

bool PaymentsAutofillTable::RemoveVirtualCardUsageData(
    const std::string& usage_data_id) {
  if (!GetVirtualCardUsageData(usage_data_id)) {
    return false;
  }

  return DeleteWhereColumnEq(db_, kVirtualCardUsageDataTable, kId,
                             usage_data_id);
}

void PaymentsAutofillTable::SetVirtualCardUsageData(
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

bool PaymentsAutofillTable::GetAllVirtualCardUsageData(
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

bool PaymentsAutofillTable::RemoveAllVirtualCardUsageData() {
  return Delete(db_, kVirtualCardUsageDataTable);
}

bool PaymentsAutofillTable::ClearAllServerData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  bool changed = false;
  for (std::string_view table_name :
       {kMaskedCreditCardsTable, kMaskedIbansTable, kServerCardMetadataTable,
        kPaymentsCustomerDataTable, kServerCardCloudTokenDataTable,
        kOfferDataTable, kOfferEligibleInstrumentTable,
        kOfferMerchantDomainTable, kVirtualCardUsageDataTable,
        kMaskedCreditCardBenefitsTable, kBenefitMerchantDomainsTable,
        kMaskedBankAccountsTable, kMaskedBankAccountsMetadataTable}) {
    Delete(db_, table_name);
    changed |= db_->GetLastChangeCount() > 0;
  }

  transaction.Commit();
  return changed;
}

bool PaymentsAutofillTable::ClearAllLocalData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  ClearLocalPaymentMethodsData();
  bool changed = db_->GetLastChangeCount() > 0;

  transaction.Commit();
  return changed;
}

bool PaymentsAutofillTable::RemoveAutofillDataModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

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
  return s_cvc.Run();
}

bool PaymentsAutofillTable::RemoveOriginURLsModifiedBetween(
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

void PaymentsAutofillTable::ClearLocalPaymentMethodsData() {
  Delete(db_, kLocalStoredCvcTable);
  Delete(db_, kCreditCardsTable);
  Delete(db_, kLocalIbansTable);
}

bool PaymentsAutofillTable::SetCreditCardBenefits(
    const std::vector<CreditCardBenefit>& credit_card_benefits) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Remove all old benefits to rewrite the benefit tables entirely.
  if (!ClearAllCreditCardBenefits()) {
    return false;
  }

  for (const CreditCardBenefit& credit_card_benefit : credit_card_benefits) {
    if (!absl::visit([](const auto& a) { return a.IsValid(); },
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
    InsertBuilder(db_, insert_benefit, kMaskedCreditCardBenefitsTable,
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
        InsertBuilder(db_, insert_benefit_merchant_domain,
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
    std::vector<CreditCardBenefit>* credit_card_benefits) {
  sql::Statement get_benefits;
  SelectBuilder(db_, get_benefits, kMaskedCreditCardBenefitsTable,
                {kBenefitId, kInstrumentId, kBenefitType, kBenefitDescription,
                 kStartTime, kEndTime, kBenefitCategory});

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
        credit_card_benefits->push_back(CreditCardFlatRateBenefit(
            benefit_id, linked_card_instrument_id, benefit_description,
            start_time, expiry_time));
        break;
      case 1:
        credit_card_benefits->push_back(CreditCardCategoryBenefit(
            benefit_id, linked_card_instrument_id, benefit_category,
            benefit_description, start_time, expiry_time));
        break;
      case 2:
        credit_card_benefits->push_back(CreditCardMerchantBenefit(
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
  sql::Transaction transaction(db_);
  return transaction.Begin() && Delete(db_, kMaskedCreditCardBenefitsTable) &&
         Delete(db_, kBenefitMerchantDomainsTable) && transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion83RemoveServerCardTypeColumn() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kMaskedCreditCardsTable, "type") &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion84AddNicknameColumn() {
  // Add the nickname column to the masked_credit_cards table.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kNickname,
                              "VARCHAR");
}

bool PaymentsAutofillTable::
    MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard() {
  // Add the new card_issuer column to the masked_credit_cards table and set
  // the default value to ISSUER_UNKNOWN.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kCardIssuer,
                              "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion87AddCreditCardNicknameColumn() {
  // Add the nickname column to the credit_card table.
  return AddColumnIfNotExists(db_, kCreditCardsTable, kNickname, "VARCHAR");
}

bool PaymentsAutofillTable::
    MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard() {
  // Add the new instrument_id column to the masked_credit_cards table and set
  // the default value to 0.
  return AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kInstrumentId,
                              "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion94AddPromoCodeColumnsToOfferData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_->DoesTableExist(kOfferDataTable)) {
    InitOfferDataTable();
  }

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

bool PaymentsAutofillTable::MigrateToVersion95AddVirtualCardMetadata() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_->DoesTableExist(kMaskedCreditCardsTable)) {
    InitMaskedCreditCardsTable();
  }

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

bool PaymentsAutofillTable::
    MigrateToVersion98RemoveStatusColumnMaskedCreditCards() {
  sql::Transaction transaction(db_);
  return transaction.Begin() &&
         DropColumn(db_, kMaskedCreditCardsTable, kStatus) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion101RemoveCreditCardArtImageTable() {
  return DropTableIfExists(db_, "credit_card_art_images");
}

bool PaymentsAutofillTable::MigrateToVersion104AddProductDescriptionColumn() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_->DoesTableExist(kMaskedCreditCardsTable)) {
    InitMaskedCreditCardsTable();
  }

  // Add product_description to masked_credit_cards.
  if (!AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kProductDescription,
                            "VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion105AddAutofillIbanTable() {
  return CreateTable(db_, kIbansTable,
                     {{kGuid, "VARCHAR"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::MigrateToVersion106RecreateAutofillIbanTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTableIfExists(db_, kIbansTable) &&
         CreateTable(db_, kIbansTable,
                     {{kGuid, "VARCHAR PRIMARY KEY"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                      {kValue, "VARCHAR"},
                      {kNickname, "VARCHAR"}}) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion108AddCardIssuerIdColumn() {
  // Add card_issuer_id to masked_credit_cards.
  return db_->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db_, kMaskedCreditCardsTable, kCardIssuerId,
                              "VARCHAR");
}

bool PaymentsAutofillTable::MigrateToVersion109AddVirtualCardUsageDataTable() {
  return CreateTable(db_, kVirtualCardUsageDataTable,
                     {{kId, "VARCHAR PRIMARY KEY"},
                      {kInstrumentId, "INTEGER DEFAULT 0"},
                      {kMerchantDomain, "VARCHAR"},
                      {kLastFour, "VARCHAR"}});
}

bool PaymentsAutofillTable::
    MigrateToVersion111AddVirtualCardEnrollmentTypeColumn() {
  return db_->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumnIfNotExists(db_, kMaskedCreditCardsTable,
                              kVirtualCardEnrollmentType, "INTEGER DEFAULT 0");
}

bool PaymentsAutofillTable::MigrateToVersion115EncryptIbanValue() {
  // Encrypt all existing IBAN values and rename the column name from `value`
  // to `value_encrypted` by the following steps:
  // 1. Read all existing guid and value data from `ibans`, encrypt all
  // values,
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

bool PaymentsAutofillTable::MigrateToVersion116AddStoredCvcTable() {
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

bool PaymentsAutofillTable::MigrateToVersion118RemovePaymentsUpiVpaTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTableIfExists(db_, kPaymentsUpiVpaTable) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::
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

bool PaymentsAutofillTable::
    MigrateToVersion123AddProductTermsUrlColumnAndAddCardBenefitsTables() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && db_->DoesTableExist(kMaskedCreditCardsTable) &&
         AddColumn(db_, kMaskedCreditCardsTable, kProductTermsUrl, "VARCHAR") &&
         CreateTable(db_, kMaskedCreditCardBenefitsTable,
                     kMaskedCreditCardBenefitsColumnNamesAndTypes) &&
         CreateTable(db_, kBenefitMerchantDomainsTable,
                     kBenefitMerchantDomainsColumnNamesAndTypes) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::
    MigrateToVersion124AndDeletePaymentInstrumentRelatedTablesAndAddMaskedBankAccountTable() {
  sql::Transaction transaction(db_);
  return transaction.Begin() && DropTableIfExists(db_, "payment_instruments") &&
         DropTableIfExists(db_, "payment_instruments_metadata") &&
         DropTableIfExists(db_, "bank_accounts") &&
         DropTableIfExists(db_, "payment_instrument_supported_rails") &&
         CreateTable(db_, kMaskedBankAccountsTable,
                     kMaskedBankAccountsColumnNamesAndTypes) &&
         CreateTable(db_, kMaskedBankAccountsMetadataTable,
                     kMaskedBankAccountsMetadataColumnNamesAndTypes) &&
         transaction.Commit();
}

bool PaymentsAutofillTable::MigrateToVersion125DeleteFullServerCardsTable() {
  return DropTableIfExists(db_, "unmasked_credit_cards");
}

void PaymentsAutofillTable::AddMaskedCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK_GT(db_->transaction_nesting(), 0);
  sql::Statement masked_insert;
  InsertBuilder(
      db_, masked_insert, kMaskedCreditCardsTable,
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

void PaymentsAutofillTable::AddUnmaskedCreditCard(const std::string& id,
                                          const std::u16string& full_number) {
  // TODO(crbug.com/1497734): Remove this method entirely.
}

bool PaymentsAutofillTable::DeleteFromMaskedCreditCards(const std::string& id) {
  DeleteWhereColumnEq(db_, kMaskedCreditCardsTable, kId, id);
  return db_->GetLastChangeCount() > 0;
}

bool PaymentsAutofillTable::DeleteFromUnmaskedCreditCards(const std::string& id) {
  // TODO(crbug.com/1497734): Remove this method entirely.
  return false;
}

base::flat_set<url::Origin>
PaymentsAutofillTable::GetMerchantDomainsForBenefitId(
    const CreditCardBenefitBase::BenefitId benefit_id) {
  base::flat_set<url::Origin> merchant_domains;
  sql::Statement s;
  SelectBuilder(db_, s, kBenefitMerchantDomainsTable, {kMerchantDomain},
                "WHERE benefit_id = ?");
  s.BindString(0, *benefit_id);
  while (s.Step()) {
    merchant_domains.insert(url::Origin::Create(GURL(s.ColumnString(0))));
  }
  return merchant_domains;
}

bool PaymentsAutofillTable::InitCreditCardsTable() {
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

bool PaymentsAutofillTable::InitLocalIbansTable() {
  return CreateTableIfNotExists(db_, kLocalIbansTable,
                                {{kGuid, "VARCHAR PRIMARY KEY"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kValueEncrypted, "VARCHAR"},
                                 {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedCreditCardsTable() {
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
       {kVirtualCardEnrollmentType, "INTEGER DEFAULT 0"},
       {kProductTermsUrl, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedIbansTable() {
  return CreateTableIfNotExists(
      db_, kMaskedIbansTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kPrefix, "VARCHAR NOT NULL"},
       {kSuffix, "VARCHAR NOT NULL"},
       {kLength, "INTEGER NOT NULL DEFAULT 0"},
       {kNickname, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedIbansMetadataTable() {
  return CreateTableIfNotExists(
      db_, kMaskedIbansMetadataTable,
      {{kInstrumentId, "VARCHAR PRIMARY KEY NOT NULL"},
       {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
       {kUseDate, "INTEGER NOT NULL DEFAULT 0"}});
}

bool PaymentsAutofillTable::InitServerCardMetadataTable() {
  return CreateTableIfNotExists(db_, kServerCardMetadataTable,
                                {{kId, "VARCHAR NOT NULL"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"},
                                 {kBillingAddressId, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitPaymentsCustomerDataTable() {
  return CreateTableIfNotExists(db_, kPaymentsCustomerDataTable,
                                {{kCustomerId, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitServerCreditCardCloudTokenDataTable() {
  return CreateTableIfNotExists(db_, kServerCardCloudTokenDataTable,
                                {{kId, "VARCHAR"},
                                 {kSuffix, "VARCHAR"},
                                 {kExpMonth, "INTEGER DEFAULT 0"},
                                 {kExpYear, "INTEGER DEFAULT 0"},
                                 {kCardArtUrl, "VARCHAR"},
                                 {kInstrumentToken, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitStoredCvcTable() {
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

bool PaymentsAutofillTable::InitOfferDataTable() {
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

bool PaymentsAutofillTable::InitOfferEligibleInstrumentTable() {
  return CreateTableIfNotExists(
      db_, kOfferEligibleInstrumentTable,
      {{kOfferId, "UNSIGNED LONG"}, {kInstrumentId, "UNSIGNED LONG"}});
}

bool PaymentsAutofillTable::InitOfferMerchantDomainTable() {
  return CreateTableIfNotExists(
      db_, kOfferMerchantDomainTable,
      {{kOfferId, "UNSIGNED LONG"}, {kMerchantDomain, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitVirtualCardUsageDataTable() {
  return CreateTableIfNotExists(db_, kVirtualCardUsageDataTable,
                                {{kId, "VARCHAR PRIMARY KEY"},
                                 {kInstrumentId, "INTEGER DEFAULT 0"},
                                 {kMerchantDomain, "VARCHAR"},
                                 {kLastFour, "VARCHAR"}});
}

bool PaymentsAutofillTable::InitMaskedBankAccountsTable() {
  return CreateTableIfNotExists(db_, kMaskedBankAccountsTable,
                                kMaskedBankAccountsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitMaskedBankAccountsMetadataTable() {
  return CreateTableIfNotExists(db_, kMaskedBankAccountsMetadataTable,
                                kMaskedBankAccountsMetadataColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitMaskedCreditCardBenefitsTable() {
  return CreateTableIfNotExists(db_, kMaskedCreditCardBenefitsTable,
                                kMaskedCreditCardBenefitsColumnNamesAndTypes);
}

bool PaymentsAutofillTable::InitBenefitMerchantDomainsTable() {
  return CreateTableIfNotExists(db_, kBenefitMerchantDomainsTable,
                                kBenefitMerchantDomainsColumnNamesAndTypes);
}

}  // namespace autofill
