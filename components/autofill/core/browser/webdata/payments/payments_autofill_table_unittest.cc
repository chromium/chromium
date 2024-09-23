// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::Time;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

CreditCardBenefitBase::BenefitId get_benefit_id(
    const CreditCardBenefit& benefit) {
  return absl::visit([](const auto& a) { return a.benefit_id(); }, benefit);
}

class PaymentsAutofillTableTest : public testing::Test {
 public:
  PaymentsAutofillTableTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<PaymentsAutofillTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_, &encryptor_));
  }

  // Get date_modifed `column` of `table_name` with specific `instrument_id` or
  // `guid`.
  time_t GetDateModified(std::string_view table_name,
                         std::string_view column,
                         absl::variant<std::string, int64_t> id) {
    sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(base::StrCat(
        {"SELECT ", column, " FROM ", table_name, " WHERE ",
         absl::holds_alternative<std::string>(id) ? "guid" : "instrument_id",
         " = ?"})));
    if (const std::string* guid = absl::get_if<std::string>(&id)) {
      s.BindString(0, *guid);
    } else {
      s.BindInt64(0, absl::get<int64_t>(id));
    }
    EXPECT_TRUE(s.Step());
    return s.ColumnInt64(0);
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const os_crypt_async::Encryptor encryptor_;
  std::unique_ptr<PaymentsAutofillTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(PaymentsAutofillTableTest, Iban) {
  // Add a valid IBAN.
  Iban iban;
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  iban.set_identifier(Iban::Guid(guid));
  iban.SetRawInfo(IBAN_VALUE, std::u16string(test::kIbanValue16));
  iban.set_nickname(u"My doctor's IBAN");

  EXPECT_TRUE(table_->AddLocalIban(iban));

  // Get the inserted IBAN.
  std::unique_ptr<Iban> db_iban = table_->GetLocalIban(iban.guid());
  ASSERT_TRUE(db_iban);
  EXPECT_EQ(guid, db_iban->guid());
  sql::Statement s_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_work.BindString(0, iban.guid());
  ASSERT_TRUE(s_work.is_valid());
  ASSERT_TRUE(s_work.Step());
  EXPECT_FALSE(s_work.Step());

  // Add another valid IBAN.
  Iban another_iban;
  std::string another_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  another_iban.set_identifier(Iban::Guid(another_guid));
  another_iban.SetRawInfo(IBAN_VALUE, u"DE91 1000 0000 0123 4567 89");
  another_iban.set_nickname(u"My brother's IBAN");

  EXPECT_TRUE(table_->AddLocalIban(another_iban));

  db_iban = table_->GetLocalIban(another_iban.guid());
  ASSERT_TRUE(db_iban);

  EXPECT_EQ(another_guid, db_iban->guid());
  sql::Statement s_target(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_target.BindString(0, another_iban.guid());
  ASSERT_TRUE(s_target.is_valid());
  ASSERT_TRUE(s_target.Step());
  EXPECT_FALSE(s_target.Step());

  // Update the another_iban.
  another_iban.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  another_iban.set_nickname(u"My teacher's IBAN");
  EXPECT_TRUE(table_->UpdateLocalIban(another_iban));
  db_iban = table_->GetLocalIban(another_iban.guid());
  ASSERT_TRUE(db_iban);
  EXPECT_EQ(another_guid, db_iban->guid());
  sql::Statement s_target_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_target_updated.BindString(0, another_iban.guid());
  ASSERT_TRUE(s_target_updated.is_valid());
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_FALSE(s_target_updated.Step());

  // Remove the 'Target' IBAN.
  EXPECT_TRUE(table_->RemoveLocalIban(another_iban.guid()));
  db_iban = table_->GetLocalIban(another_iban.guid());
  EXPECT_FALSE(db_iban);
}

// Test that masked IBANs can be added and loaded successfully.
TEST_F(PaymentsAutofillTableTest, MaskedServerIban) {
  Iban iban_0 = test::GetServerIban();
  Iban iban_1 = test::GetServerIban2();
  Iban iban_2 = test::GetServerIban3();
  std::vector<Iban> ibans = {iban_0, iban_1, iban_2};

  table_->SetServerIbansForTesting(ibans);

  std::vector<std::unique_ptr<Iban>> masked_server_ibans;
  EXPECT_TRUE(table_->GetServerIbans(masked_server_ibans));
  EXPECT_EQ(3U, masked_server_ibans.size());
  EXPECT_THAT(ibans, UnorderedElementsAre(*masked_server_ibans[0],
                                          *masked_server_ibans[1],
                                          *masked_server_ibans[2]));
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerIbansMetadata(outputs));
  ASSERT_FALSE(outputs.empty());
  EXPECT_EQ(iban_0.use_date(), outputs[0].use_date);
  EXPECT_EQ(iban_1.use_date(), outputs[1].use_date);
  EXPECT_EQ(iban_2.use_date(), outputs[2].use_date);
}

// Test that masked IBANs can be added and loaded successfully without updating
// their metadata.
TEST_F(PaymentsAutofillTableTest, MaskedServerIbanMetadataNotUpdated) {
  std::vector<Iban> ibans = {test::GetServerIban()};

  table_->SetServerIbansData(ibans);

  std::vector<std::unique_ptr<Iban>> masked_server_ibans;
  EXPECT_TRUE(table_->GetServerIbans(masked_server_ibans));
  EXPECT_EQ(1U, masked_server_ibans.size());
  EXPECT_THAT(ibans, UnorderedElementsAre(*masked_server_ibans[0]));
}

TEST_F(PaymentsAutofillTableTest, CreditCard) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a 'Work' credit card.
  CreditCard work_creditcard;
  work_creditcard.set_origin("https://www.example.com/");
  work_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  work_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  work_creditcard.SetNickname(u"Corporate card");
  work_creditcard.set_cvc(u"123");

  Time pre_creation_time = base::Time::Now();
  EXPECT_TRUE(table_->AddCreditCard(work_creditcard));
  Time post_creation_time = base::Time::Now();

  // Get the 'Work' credit card.
  std::unique_ptr<CreditCard> db_creditcard =
      table_->GetCreditCard(work_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(work_creditcard, *db_creditcard);
  // Check GetCreditCard statement
  sql::Statement s_credit_card_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_credit_card_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_credit_card_work.is_valid());
  ASSERT_TRUE(s_credit_card_work.Step());
  EXPECT_GE(s_credit_card_work.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_credit_card_work.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_credit_card_work.Step());
  // Check GetLocalStoredCvc statement
  sql::Statement s_cvc_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT value_encrypted,  last_updated_timestamp "
      "FROM local_stored_cvc WHERE guid=?"));
  s_cvc_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_cvc_work.is_valid());
  ASSERT_TRUE(s_cvc_work.Step());
  EXPECT_GE(s_cvc_work.ColumnInt64(1), pre_creation_time.ToTimeT());
  EXPECT_LE(s_cvc_work.ColumnInt64(1), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_cvc_work.Step());

  // Add a 'Target' credit card.
  CreditCard target_creditcard;
  target_creditcard.set_origin(std::string());
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  target_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1111222233334444");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"06");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2012");
  target_creditcard.SetNickname(u"Grocery card");
  target_creditcard.set_cvc(u"234");

  pre_creation_time = base::Time::Now();
  EXPECT_TRUE(table_->AddCreditCard(target_creditcard));
  post_creation_time = base::Time::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  // Check GetCreditCard statement.
  sql::Statement s_credit_card_target(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid, name_on_card, expiration_month, expiration_year, "
          "card_number_encrypted, date_modified, nickname "
          "FROM credit_cards WHERE guid=?"));
  s_credit_card_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_credit_card_target.is_valid());
  ASSERT_TRUE(s_credit_card_target.Step());
  EXPECT_GE(s_credit_card_target.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_credit_card_target.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_credit_card_target.Step());
  // Check GetLocalStoredCvc statement
  sql::Statement s_cvc_target(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT value_encrypted,  last_updated_timestamp "
      "FROM local_stored_cvc WHERE guid=?"));
  s_cvc_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_cvc_target.is_valid());
  ASSERT_TRUE(s_cvc_target.Step());
  EXPECT_GE(s_cvc_target.ColumnInt64(1), pre_creation_time.ToTimeT());
  EXPECT_LE(s_cvc_target.ColumnInt64(1), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_cvc_target.Step());

  // Update the 'Target' credit card.
  target_creditcard.set_origin("Interactive Autofill dialog");
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Charles Grady");
  target_creditcard.SetNickname(u"Supermarket");
  target_creditcard.set_cvc(u"234");
  Time pre_modification_time = base::Time::Now();
  EXPECT_TRUE(table_->UpdateCreditCard(target_creditcard));
  Time post_modification_time = base::Time::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_target_updated.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target_updated.is_valid());
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_GE(s_target_updated.ColumnInt64(5), pre_modification_time.ToTimeT());
  EXPECT_LE(s_target_updated.ColumnInt64(5), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_target_updated.Step());

  // Remove the 'Target' credit card.
  EXPECT_TRUE(table_->RemoveCreditCard(target_creditcard.guid()));
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  EXPECT_FALSE(db_creditcard);
}

TEST_F(PaymentsAutofillTableTest, AddCreditCardCvcWithFlagOff) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::WithCvc(test::GetCreditCard());
  EXPECT_TRUE(table_->AddCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"", db_card->cvc());

  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"", db_card->cvc());
}

// Tests that adding credit card with cvc, get credit card with cvc and update
// credit card with only cvc change will not update credit_card table
// modification_date.
TEST_F(PaymentsAutofillTableTest, CreditCardCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  const base::Time arbitrary_time = base::Time::Now();

  CreditCard card = test::WithCvc(test::GetCreditCard());
  EXPECT_TRUE(table_->AddCreditCard(card));

  // Get the credit card, cvc should match.
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(card.cvc(), db_card->cvc());
  // Some precision is lost when converting to time_t and back.
  EXPECT_EQ(arbitrary_time.ToTimeT(),
            db_card->cvc_modification_date().ToTimeT());

  // Verify last_updated_timestamp in local_stored_cvc table is set correctly.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            arbitrary_time.ToTimeT());

  // Set the current time to another value.
  task_environment_.FastForwardBy(base::Seconds(1000));
  const base::Time some_later_time = base::Time::Now();

  // Update the credit card but CVC is same.
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Charles Grady");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  // credit_card table date_modified should be updated.
  EXPECT_EQ(GetDateModified("credit_cards", "date_modified", card.guid()),
            some_later_time.ToTimeT());
  // local_stored_cvc table timestamp should not be updated.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            arbitrary_time.ToTimeT());

  // Set the current time to another value.
  task_environment_.FastForwardBy(base::Seconds(5000));
  const base::Time much_later_time = base::Time::Now();

  // Update the credit card and CVC is different.
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  // CVC should be updated to new CVC.
  EXPECT_EQ(u"234", db_card->cvc());
  EXPECT_EQ(much_later_time.ToTimeT(),
            db_card->cvc_modification_date().ToTimeT());
  // local_stored_cvc table timestamp should be updated.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            much_later_time.ToTimeT());

  // Remove the credit card. It should also remove cvc from local_stored_cvc
  // table.
  EXPECT_TRUE(table_->RemoveCreditCard(card.guid()));
  sql::Statement cvc_removed_statement(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM local_stored_cvc WHERE guid=?"));
  cvc_removed_statement.BindString(0, card.guid());
  ASSERT_TRUE(cvc_removed_statement.is_valid());
  EXPECT_FALSE(cvc_removed_statement.Step());
}

// Tests that update a credit card CVC that doesn't have CVC set initially
// inserts a new CVC record.
TEST_F(PaymentsAutofillTableTest, UpdateCreditCardCvc_Add) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // Update the credit card CVC, we should expect success and CVC gets updated.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());
}

// Tests that updating a credit card CVC that is different from CVC set
// initially.
TEST_F(PaymentsAutofillTableTest, UpdateCreditCardCvc_Update) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // INSERT
  // Updating a card that doesn't have a CVC is the same as inserting a new CVC
  // record.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());

  // UPDATE
  // Update the credit card CVC.
  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"234", db_card->cvc());
}

// Tests that updating a credit card CVC with empty CVC will delete CVC
// record. This is necessary because if inserting a CVC, UPDATE is chosen over
// INSERT, it will causes a crash.
TEST_F(PaymentsAutofillTableTest, UpdateCreditCardCvc_Delete) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // INSERT
  // Updating a card that doesn't have a CVC is the same as inserting a new CVC
  // record.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());

  // DELETE
  // Updating a card with empty CVC is the same as deleting the CVC record.
  card.set_cvc(u"");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  sql::Statement cvc_statement(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid FROM local_stored_cvc WHERE guid=?"));
  cvc_statement.BindString(0, card.guid());
  ASSERT_TRUE(cvc_statement.is_valid());
  EXPECT_FALSE(cvc_statement.Step());
}

TEST_F(PaymentsAutofillTableTest, LocalCvcs_ClearAll) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card_1 = test::WithCvc(test::GetCreditCard());
  CreditCard card_2 = test::WithCvc(test::GetCreditCard2());
  EXPECT_TRUE(table_->AddCreditCard(card_1));
  EXPECT_TRUE(table_->AddCreditCard(card_2));

  // Get the credit cards and the CVCs should match.
  std::unique_ptr<CreditCard> db_card_1 = table_->GetCreditCard(card_1.guid());
  std::unique_ptr<CreditCard> db_card_2 = table_->GetCreditCard(card_2.guid());
  EXPECT_EQ(card_1.cvc(), db_card_1->cvc());
  EXPECT_EQ(card_2.cvc(), db_card_2->cvc());

  // Clear all local CVCs from the web database.
  table_->ClearLocalCvcs();

  sql::Statement cvc_statement(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid FROM local_stored_cvc WHERE guid=?"));

  // Verify `card_1` CVC is deleted.
  cvc_statement.BindString(0, card_1.guid());
  ASSERT_TRUE(cvc_statement.is_valid());
  EXPECT_FALSE(cvc_statement.Step());
  cvc_statement.Reset(/*clear_bound_vars=*/true);

  // Verify `card_2` CVC is deleted.
  cvc_statement.BindString(0, card_2.guid());
  ASSERT_TRUE(cvc_statement.is_valid());
  EXPECT_FALSE(cvc_statement.Step());
}

// Tests that verify add, update and clear server cvc function working as
// expected.
TEST_F(PaymentsAutofillTableTest, ServerCvc) {
  const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
  int64_t kInstrumentId = 111111111111;
  const std::u16string kCvc = u"123";
  const ServerCvc kServerCvc{kInstrumentId, kCvc, kArbitraryTime};
  EXPECT_TRUE(table_->AddServerCvc(kServerCvc));
  // Database does not allow adding same instrument_id twice.
  EXPECT_FALSE(table_->AddServerCvc(kServerCvc));
  EXPECT_THAT(table_->GetAllServerCvcs(),
              UnorderedElementsAre(testing::Pointee(kServerCvc)));

  const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);
  const std::u16string kNewCvc = u"234";
  const ServerCvc kNewServerCvcUnderSameInstrumentId{kInstrumentId, kNewCvc,
                                                     kSomeLaterTime};
  EXPECT_TRUE(table_->UpdateServerCvc(kNewServerCvcUnderSameInstrumentId));
  EXPECT_THAT(table_->GetAllServerCvcs(),
              UnorderedElementsAre(
                  testing::Pointee(kNewServerCvcUnderSameInstrumentId)));

  // Remove the server cvc. It should also remove cvc from server_stored_cvc
  // table.
  EXPECT_TRUE(table_->RemoveServerCvc(kInstrumentId));
  EXPECT_TRUE(table_->GetAllServerCvcs().empty());

  // Remove non-exist cvc will return false.
  EXPECT_FALSE(table_->RemoveServerCvc(kInstrumentId));

  // Clear the server_stored_cvc table.
  table_->AddServerCvc(kServerCvc);
  EXPECT_TRUE(table_->ClearServerCvcs());
  EXPECT_TRUE(table_->GetAllServerCvcs().empty());

  // Clear the server_stored_cvc table when table is empty will return false.
  EXPECT_FALSE(table_->ClearServerCvcs());
}

// Tests that verify reconcile server cvc function working as expected.
TEST_F(PaymentsAutofillTableTest, ReconcileServerCvcs) {
  const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
  // Add 2 server credit cards.
  CreditCard card1 = test::WithCvc(test::GetMaskedServerCard());
  CreditCard card2 = test::WithCvc(test::GetMaskedServerCard2());
  test::SetServerCreditCards(table_.get(), {card1, card2});

  // Add 1 server cvc that doesn't have a credit card associate with. We
  // should have 3 cvcs in server_stored_cvc table.
  EXPECT_TRUE(table_->AddServerCvc(ServerCvc{3333, u"456", kArbitraryTime}));
  EXPECT_EQ(3U, table_->GetAllServerCvcs().size());

  std::vector<std::unique_ptr<ServerCvc>> deleted_cvc_list =
      table_->DeleteOrphanedServerCvcs();

  // After we reconcile server cvc, we should only see 2 cvcs in
  // server_stored_cvc table because obsolete cvc has been reconciled.
  // Additionally, `deleted_cvc_list` should contain the obsolete CVC..
  EXPECT_EQ(2U, table_->GetAllServerCvcs().size());
  ASSERT_EQ(1uL, deleted_cvc_list.size());
  EXPECT_EQ(3333, deleted_cvc_list[0]->instrument_id);
}

TEST_F(PaymentsAutofillTableTest, AddServerCreditCardForTesting) {
  CreditCard credit_card;
  credit_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  credit_card.set_server_id("server_id");
  credit_card.set_origin("https://www.example.com/");
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"3456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  credit_card.SetNetworkForMaskedCard(kVisaCard);

  EXPECT_TRUE(table_->AddServerCreditCardForTesting(credit_card));

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(0, credit_card.Compare(*outputs[0]));
}

TEST_F(PaymentsAutofillTableTest, UpdateCreditCard) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = base::Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the credit card and save the update to the database.
  // The modification date should change to reflect the update.
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the credit card's modification time.
  const time_t mock_modification_date = base::Time::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date.is_valid());
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateCreditCard()| without changing the credit card.
  // The modification date should not change.
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_unchanged(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_unchanged.is_valid());
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(PaymentsAutofillTableTest, UpdateCreditCardOriginOnly) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = base::Time::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update just the credit card's origin and save the update to the
  // database.  The modification date should change to reflect the update.
  credit_card.set_origin("https://www.example.com/");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());
}

TEST_F(PaymentsAutofillTableTest, SetGetServerCards) {
  for (bool is_cvc_storage_flag_enabled : {true, false}) {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatureState(features::kAutofillEnableCvcStorageAndFilling,
                                 is_cvc_storage_flag_enabled);

    std::vector<CreditCard> inputs;
    inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "a123");
    inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
    inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
    inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
    inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"4444");
    inputs[0].SetNetworkForMaskedCard(kVisaCard);
    inputs[0].set_card_issuer(CreditCard::Issuer::kExternalIssuer);
    inputs[0].set_instrument_id(321);
    inputs[0].set_virtual_card_enrollment_state(
        CreditCard::VirtualCardEnrollmentState::kUnenrolled);
    inputs[0].set_virtual_card_enrollment_type(
        CreditCard::VirtualCardEnrollmentType::kIssuer);
    inputs[0].set_product_description(u"Fake description");
    inputs[0].set_cvc(u"000");

    inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b456");
    inputs[1].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
    inputs[1].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
    inputs[1].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
    inputs[1].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
    inputs[1].SetNetworkForMaskedCard(kVisaCard);
    std::u16string nickname = u"Grocery card";
    inputs[1].SetNickname(nickname);
    inputs[1].set_card_issuer(CreditCard::Issuer::kExternalIssuer);
    inputs[1].set_issuer_id("amex");
    inputs[1].set_instrument_id(123);
    inputs[1].set_virtual_card_enrollment_state(
        CreditCard::VirtualCardEnrollmentState::kEnrolled);
    inputs[1].set_virtual_card_enrollment_type(
        CreditCard::VirtualCardEnrollmentType::kNetwork);
    inputs[1].set_card_art_url(GURL("https://www.example.com"));
    inputs[1].set_product_terms_url(GURL("https://www.example_term.com"));
    inputs[1].set_cvc(u"111");

    // The CVC modification dates are set to `now` during insertion.
    const time_t now = base::Time::Now().ToTimeT();
    test::SetServerCreditCards(table_.get(), inputs);

    std::vector<std::unique_ptr<CreditCard>> outputs;
    ASSERT_TRUE(table_->GetServerCreditCards(outputs));
    ASSERT_EQ(inputs.size(), outputs.size());

    // Ordering isn't guaranteed, so fix the ordering if it's backwards.
    if (outputs[1]->server_id() == inputs[0].server_id()) {
      std::swap(outputs[0], outputs[1]);
    }

    // GUIDs for server cards are dynamically generated so will be different
    // after reading from the DB. Check they're valid, but otherwise don't count
    // them in the comparison.
    inputs[0].set_guid(std::string());
    inputs[1].set_guid(std::string());
    outputs[0]->set_guid(std::string());
    outputs[1]->set_guid(std::string());

    if (!is_cvc_storage_flag_enabled) {
      // Verify that CVC values are not present on the output entries and then
      // clear the same from the input entries to allow the comparison between
      // input and output.
      EXPECT_TRUE(outputs[0]->cvc().empty());
      EXPECT_TRUE(outputs[0]->cvc_modification_date().is_null());
      EXPECT_TRUE(outputs[1]->cvc().empty());
      EXPECT_TRUE(outputs[1]->cvc_modification_date().is_null());

      inputs[0].clear_cvc();
      inputs[1].clear_cvc();
    }
    EXPECT_EQ(inputs[0], *outputs[0]);
    EXPECT_EQ(inputs[1], *outputs[1]);

    EXPECT_TRUE(outputs[0]->nickname().empty());
    EXPECT_EQ(nickname, outputs[1]->nickname());

    EXPECT_EQ(CreditCard::Issuer::kExternalIssuer, outputs[0]->card_issuer());
    EXPECT_EQ(CreditCard::Issuer::kExternalIssuer, outputs[1]->card_issuer());
    EXPECT_EQ("", outputs[0]->issuer_id());
    EXPECT_EQ("amex", outputs[1]->issuer_id());

    EXPECT_EQ(321, outputs[0]->instrument_id());
    EXPECT_EQ(123, outputs[1]->instrument_id());

    EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kUnenrolled,
              outputs[0]->virtual_card_enrollment_state());
    EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kEnrolled,
              outputs[1]->virtual_card_enrollment_state());

    EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kIssuer,
              outputs[0]->virtual_card_enrollment_type());
    EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kNetwork,
              outputs[1]->virtual_card_enrollment_type());

    EXPECT_EQ(GURL(), outputs[0]->card_art_url());
    EXPECT_EQ(GURL("https://www.example.com"), outputs[1]->card_art_url());

    EXPECT_EQ(GURL(), outputs[0]->product_terms_url());
    EXPECT_EQ(GURL("https://www.example_term.com"),
              outputs[1]->product_terms_url());

    EXPECT_EQ(u"Fake description", outputs[0]->product_description());

    if (is_cvc_storage_flag_enabled) {
      EXPECT_EQ(inputs[0].cvc(), outputs[0]->cvc());
      EXPECT_EQ(now, outputs[0]->cvc_modification_date().ToTimeT());
      EXPECT_EQ(inputs[1].cvc(), outputs[1]->cvc());
      EXPECT_EQ(now, outputs[1]->cvc_modification_date().ToTimeT());
    }
  }
}

TEST_F(PaymentsAutofillTableTest, SetGetRemoveServerCardMetadata) {
  // Create and set the metadata.
  PaymentsMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = base::Time::Now();
  input.billing_address_id = "billing id";
  EXPECT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[0]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerCardMetadata(input.id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  EXPECT_EQ(0U, outputs.size());
}

// Test that masked IBAN metadata can be added, retrieved and removed
// successfully.
TEST_F(PaymentsAutofillTableTest, SetGetRemoveServerIbanMetadata) {
  Iban iban = test::GetServerIban();
  // Set the metadata.
  iban.set_use_count(50);
  iban.set_use_date(base::Time::Now());
  EXPECT_TRUE(table_->AddOrUpdateServerIbanMetadata(iban.GetMetadata()));

  // Make sure it was added correctly.
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerIbansMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(iban.GetMetadata(), outputs[0]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerIbanMetadata(outputs[0].id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerIbansMetadata(outputs));
  EXPECT_EQ(0u, outputs.size());
}

TEST_F(PaymentsAutofillTableTest, AddUpdateServerCardMetadata) {
  // Create and set the metadata.
  PaymentsMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = base::Time::Now();
  input.billing_address_id = "billing id";
  ASSERT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
  ASSERT_EQ(input, outputs[0]);

  // Update the metadata in the table.
  input.use_count = 51;
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));

  // Make sure it was updated correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[0]);

  // Insert a new entry using update - that should also be legal.
  input.id = "another server id";
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(2U, outputs.size());
}

TEST_F(PaymentsAutofillTableTest, UpdateServerCardMetadataDoesNotChangeData) {
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "a123");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(inputs[0].server_id(), outputs[0]->server_id());

  // Update metadata in the profile.
  ASSERT_NE(outputs[0]->use_count(), 51u);
  outputs[0]->set_use_count(51);

  PaymentsMetadata input_metadata = outputs[0]->GetMetadata();
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input_metadata));

  // Make sure it was updated correctly.
  std::vector<PaymentsMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerCardsMetadata(output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(input_metadata, output_metadata[0]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<CreditCard>> outputs2;
  table_->GetServerCreditCards(outputs2);
  ASSERT_EQ(1u, outputs2.size());
  EXPECT_EQ(0, outputs[0]->Compare(*outputs2[0]));
}

// Test that updating masked IBAN metadata won't affect IBAN data.
TEST_F(PaymentsAutofillTableTest, UpdateServerIbanMetadata) {
  std::vector<Iban> inputs = {test::GetServerIban()};
  table_->SetServerIbansForTesting(inputs);

  std::vector<std::unique_ptr<Iban>> outputs;
  EXPECT_TRUE(table_->GetServerIbans(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(inputs[0].instrument_id(), outputs[0]->instrument_id());

  // Update metadata in the IBAN.
  outputs[0]->set_use_count(outputs[0]->use_count() + 1);

  EXPECT_TRUE(table_->AddOrUpdateServerIbanMetadata(outputs[0]->GetMetadata()));

  // Make sure it was updated correctly.
  std::vector<PaymentsMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerIbansMetadata(output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(outputs[0]->GetMetadata(), output_metadata[0]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<Iban>> outputs2;
  EXPECT_TRUE(table_->GetServerIbans(outputs2));
  ASSERT_EQ(1U, outputs2.size());
  EXPECT_EQ(0, outputs[0]->Compare(*outputs2[0]));
}

TEST_F(PaymentsAutofillTableTest, RemoveWrongServerCardMetadata) {
  // Crete and set some metadata.
  PaymentsMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = base::Time::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Make sure it was added correctly.
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[0]);

  // Try removing some non-existent metadata.
  EXPECT_FALSE(table_->RemoveServerCardMetadata("a_wrong_id"));

  // Make sure the metadata was not removed.
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  ASSERT_EQ(1U, outputs.size());
}

TEST_F(PaymentsAutofillTableTest, SetServerCardsData) {
  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "card1");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  inputs[0].SetNickname(u"Grocery card");
  inputs[0].set_card_issuer(CreditCard::Issuer::kExternalIssuer);
  inputs[0].set_issuer_id("amex");
  inputs[0].set_instrument_id(1);
  inputs[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  inputs[0].set_virtual_card_enrollment_type(
      CreditCard::VirtualCardEnrollmentType::kIssuer);
  inputs[0].set_card_art_url(GURL("https://www.example.com"));
  inputs[0].set_product_terms_url(GURL("https://www.example_term.com"));
  inputs[0].set_product_description(u"Fake description");

  table_->SetServerCardsData(inputs);

  // Make sure the card was added correctly.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(outputs));
  ASSERT_EQ(inputs.size(), outputs.size());

  // GUIDs for server cards are dynamically generated so will be different
  // after reading from the DB. Check they're valid, but otherwise don't count
  // them in the comparison.
  inputs[0].set_guid(std::string());
  outputs[0]->set_guid(std::string());

  EXPECT_EQ(inputs[0], *outputs[0]);

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kEnrolled,
            outputs[0]->virtual_card_enrollment_state());

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kIssuer,
            outputs[0]->virtual_card_enrollment_type());

  EXPECT_EQ(CreditCard::Issuer::kExternalIssuer, outputs[0]->card_issuer());
  EXPECT_EQ("amex", outputs[0]->issuer_id());

  EXPECT_EQ(GURL("https://www.example.com"), outputs[0]->card_art_url());
  EXPECT_EQ(GURL("https://www.example_term.com"),
            outputs[0]->product_terms_url());
  EXPECT_EQ(u"Fake description", outputs[0]->product_description());

  // Make sure no metadata was added.
  std::vector<PaymentsMetadata> metadata;
  ASSERT_TRUE(table_->GetServerCardsMetadata(metadata));
  ASSERT_EQ(0U, metadata.size());

  // Set a different card.
  inputs[0] = CreditCard(CreditCard::RecordType::kMaskedServerCard, "card2");
  table_->SetServerCardsData(inputs);

  // The original one should have been replaced.
  ASSERT_TRUE(table_->GetServerCreditCards(outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ("card2", outputs[0]->server_id());
  EXPECT_EQ(CreditCard::Issuer::kIssuerUnknown, outputs[0]->card_issuer());
  EXPECT_EQ("", outputs[0]->issuer_id());

  // Make sure no metadata was added.
  ASSERT_TRUE(table_->GetServerCardsMetadata(metadata));
  ASSERT_EQ(0U, metadata.size());
}

// Tests that adding server cards data does not delete the existing metadata.
TEST_F(PaymentsAutofillTableTest, SetServerCardsData_ExistingMetadata) {
  // Create and set some metadata.
  PaymentsMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = base::Time::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "server id");
  table_->SetServerCardsData(inputs);

  // Make sure the metadata is still intact.
  std::vector<PaymentsMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(outputs));
  EXPECT_THAT(outputs, ElementsAre(input));
}

// Calling SetServerCreditCards should replace all existing cards.
TEST_F(PaymentsAutofillTableTest, SetServerCardModify) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  // Set inputs that do not include our old card.
  CreditCard random_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  random_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  random_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  random_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  random_card.SetRawInfo(CREDIT_CARD_NUMBER, u"2222");
  random_card.SetNetworkForMaskedCard(kVisaCard);
  inputs[0] = random_card;
  test::SetServerCreditCards(table_.get(), inputs);

  // We should have only the new card, the other one should have been deleted.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() ==
              CreditCard::RecordType::kMaskedServerCard);
  EXPECT_EQ(random_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(u"2222", outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();
}

TEST_F(PaymentsAutofillTableTest, SetServerCardUpdateUsageStatsAndBillingAddress) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.set_billing_address_id("1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  // We don't track modification date for server cards. It should always be
  // base::Time().
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Update the usage stats; make sure they're reflected in GetServerProfiles.
  inputs.back().set_use_count(4U);
  inputs.back().set_use_date(base::Time());
  inputs.back().set_billing_address_id("2");
  table_->UpdateServerCardMetadata(inputs.back());
  table_->GetServerCreditCards(outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Setting the cards again shouldn't delete the usage stats.
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Set a card list where the card is missing --- this should clear metadata.
  CreditCard masked_card2(CreditCard::RecordType::kMaskedServerCard, "b456");
  inputs.back() = masked_card2;
  table_->SetServerCreditCards(inputs);

  // Back to the original card list.
  inputs.back() = masked_card;
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("1", outputs[0]->billing_address_id());
  outputs.clear();
}

// Test that we can get what we set.
TEST_F(PaymentsAutofillTableTest, SetGetPaymentsCustomerData) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(output));
  EXPECT_EQ(input, *output);
}

// We don't set anything in the table. Test that we don't crash.
TEST_F(PaymentsAutofillTableTest, GetPaymentsCustomerData_NoData) {
  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(output));
  EXPECT_FALSE(output);
}

// The latest PaymentsCustomerData that was set is returned.
TEST_F(PaymentsAutofillTableTest, SetGetPaymentsCustomerData_MultipleSet) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  PaymentsCustomerData input2{/*customer_id=*/"wallet"};
  table_->SetPaymentsCustomerData(&input2);

  PaymentsCustomerData input3{/*customer_id=*/"latest"};
  table_->SetPaymentsCustomerData(&input3);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(output));
  EXPECT_EQ(input3, *output);
}

TEST_F(PaymentsAutofillTableTest, SetGetCreditCardCloudData_OneTimeSet) {
  // Move the time to 20XX.
  task_environment_.FastForwardBy(base::Days(365) * 40);
  std::vector<CreditCardCloudTokenData> inputs;
  inputs.push_back(test::GetCreditCardCloudTokenData1());
  inputs.push_back(test::GetCreditCardCloudTokenData2());
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(outputs));
  EXPECT_EQ(outputs.size(), inputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData1()));
  EXPECT_EQ(0, outputs[1]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(PaymentsAutofillTableTest, SetGetCreditCardCloudData_MultipleSet) {
  // Move the time to 20XX.
  task_environment_.FastForwardBy(base::Days(365) * 40);
  std::vector<CreditCardCloudTokenData> inputs;
  CreditCardCloudTokenData input1 = test::GetCreditCardCloudTokenData1();
  inputs.push_back(input1);
  table_->SetCreditCardCloudTokenData(inputs);

  inputs.clear();
  CreditCardCloudTokenData input2 = test::GetCreditCardCloudTokenData2();
  inputs.push_back(input2);
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(outputs));
  EXPECT_EQ(1u, outputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(PaymentsAutofillTableTest, GetCreditCardCloudData_NoData) {
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> output;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(output));
  EXPECT_TRUE(output.empty());
}

TEST_F(PaymentsAutofillTableTest, SetAndGetCreditCardOfferData) {
  // Set Offer ID.
  int64_t offer_id_1 = 1;
  int64_t offer_id_2 = 2;
  int64_t offer_id_3 = 3;

  // Set reward amounts for card-linked offers on offer 1 and 2.
  std::string offer_reward_amount_1 = "$5";
  std::string offer_reward_amount_2 = "10%";

  // Set promo code for offer 3.
  std::string promo_code_3 = "5PCTOFFSHOES";

  // Set expiry.
  base::Time expiry_1 = base::Time::FromSecondsSinceUnixEpoch(1000);
  base::Time expiry_2 = base::Time::FromSecondsSinceUnixEpoch(2000);
  base::Time expiry_3 = base::Time::FromSecondsSinceUnixEpoch(3000);

  // Set details URL.
  GURL offer_details_url_1 = GURL("https://www.offer_1_example.com/");
  GURL offer_details_url_2 = GURL("https://www.offer_2_example.com/");
  GURL offer_details_url_3 = GURL("https://www.offer_3_example.com/");

  // Set merchant domains for offer 1.
  std::vector<GURL> merchant_origins_1;
  merchant_origins_1.emplace_back("http://www.merchant_domain_1_1.com/");
  std::vector<GURL> merchant_origins_2;
  merchant_origins_2.emplace_back("http://www.merchant_domain_1_2.com/");
  std::vector<GURL> merchant_origins_3;
  merchant_origins_3.emplace_back("http://www.merchant_domain_1_3.com/");
  // Set merchant domains for offer 2.
  merchant_origins_2.emplace_back("http://www.merchant_domain_2_1.com/");
  // Set merchant domains for offer 3.
  merchant_origins_3.emplace_back("http://www.merchant_domain_3_1.com/");
  merchant_origins_3.emplace_back("http://www.merchant_domain_3_2.com/");

  DisplayStrings display_strings_1;
  DisplayStrings display_strings_2;
  DisplayStrings display_strings_3;
  // Set display strings for all 3 offers.
  display_strings_1.value_prop_text = "$5 off your purchase";
  display_strings_2.value_prop_text = "10% off your purchase";
  display_strings_3.value_prop_text = "5% off shoes. Up to $50.";
  display_strings_1.see_details_text = "Terms apply.";
  display_strings_2.see_details_text = "Terms apply.";
  display_strings_3.see_details_text = "See details.";
  display_strings_1.usage_instructions_text =
      "Check out with this card to activate.";
  display_strings_2.usage_instructions_text =
      "Check out with this card to activate.";
  display_strings_3.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";

  std::vector<int64_t> eligible_instrument_id_1;
  std::vector<int64_t> eligible_instrument_id_2;
  std::vector<int64_t> eligible_instrument_id_3;

  // Set eligible card-linked instrument ID for offer 1.
  eligible_instrument_id_1.push_back(10);
  eligible_instrument_id_1.push_back(11);
  // Set eligible card-linked instrument ID for offer 2.
  eligible_instrument_id_2.push_back(20);
  eligible_instrument_id_2.push_back(21);
  eligible_instrument_id_2.push_back(22);

  // Create vector of offer data.
  std::vector<AutofillOfferData> autofill_offer_data;
  autofill_offer_data.push_back(AutofillOfferData::GPayCardLinkedOffer(
      offer_id_1, expiry_1, merchant_origins_1, offer_details_url_1,
      display_strings_2, eligible_instrument_id_1, offer_reward_amount_1));
  autofill_offer_data.push_back(AutofillOfferData::GPayCardLinkedOffer(
      offer_id_2, expiry_2, merchant_origins_2, offer_details_url_2,
      display_strings_2, eligible_instrument_id_2, offer_reward_amount_2));
  autofill_offer_data.push_back(AutofillOfferData::GPayPromoCodeOffer(
      offer_id_3, expiry_3, merchant_origins_3, offer_details_url_3,
      display_strings_3, promo_code_3));

  table_->SetAutofillOffers(autofill_offer_data);

  std::vector<std::unique_ptr<AutofillOfferData>> output_offer_data;

  EXPECT_TRUE(table_->GetAutofillOffers(&output_offer_data));
  EXPECT_EQ(autofill_offer_data.size(), output_offer_data.size());

  for (const auto& data : autofill_offer_data) {
    // Find output data with corresponding Offer ID.
    size_t output_index = 0;
    while (output_index < output_offer_data.size()) {
      if (data.GetOfferId() == output_offer_data[output_index]->GetOfferId()) {
        break;
      }
      output_index++;
    }

    // Expect to find matching Offer ID's.
    EXPECT_NE(output_index, output_offer_data.size());

    // All corresponding fields must be equal.
    EXPECT_EQ(data.GetOfferId(), output_offer_data[output_index]->GetOfferId());
    EXPECT_EQ(data.GetOfferRewardAmount(),
              output_offer_data[output_index]->GetOfferRewardAmount());
    EXPECT_EQ(data.GetPromoCode(),
              output_offer_data[output_index]->GetPromoCode());
    EXPECT_EQ(data.GetExpiry(), output_offer_data[output_index]->GetExpiry());
    EXPECT_EQ(data.GetOfferDetailsUrl().spec(),
              output_offer_data[output_index]->GetOfferDetailsUrl().spec());
    EXPECT_EQ(
        data.GetDisplayStrings().value_prop_text,
        output_offer_data[output_index]->GetDisplayStrings().value_prop_text);
    EXPECT_EQ(
        data.GetDisplayStrings().see_details_text,
        output_offer_data[output_index]->GetDisplayStrings().see_details_text);
    EXPECT_EQ(data.GetDisplayStrings().usage_instructions_text,
              output_offer_data[output_index]
                  ->GetDisplayStrings()
                  .usage_instructions_text);
    ASSERT_THAT(data.GetMerchantOrigins(),
                testing::UnorderedElementsAreArray(
                    output_offer_data[output_index]->GetMerchantOrigins()));
    ASSERT_THAT(
        data.GetEligibleInstrumentIds(),
        testing::UnorderedElementsAreArray(
            output_offer_data[output_index]->GetEligibleInstrumentIds()));
  }
}

TEST_F(PaymentsAutofillTableTest, SetAndGetVirtualCardUsageData) {
  // Create test data.
  VirtualCardUsageData virtual_card_usage_data_1 =
      test::GetVirtualCardUsageData1();
  VirtualCardUsageData virtual_card_usage_data_2 =
      test::GetVirtualCardUsageData2();

  // Create vector of VCN usage data.
  std::vector<VirtualCardUsageData> virtual_card_usage_data;
  virtual_card_usage_data.push_back(virtual_card_usage_data_1);
  virtual_card_usage_data.push_back(virtual_card_usage_data_2);

  table_->SetVirtualCardUsageData(virtual_card_usage_data);

  std::vector<VirtualCardUsageData> output_data;

  EXPECT_TRUE(table_->GetAllVirtualCardUsageData(output_data));
  EXPECT_EQ(virtual_card_usage_data.size(), output_data.size());

  for (const auto& data : virtual_card_usage_data) {
    // Find output data with corresponding data.
    auto it = base::ranges::find(output_data, data.instrument_id(),
                                 &VirtualCardUsageData::instrument_id);

    // Expect to find a usage data match in the vector.
    EXPECT_NE(it, output_data.end());

    // All corresponding fields must be equal.
    EXPECT_EQ(data.usage_data_id(), it->usage_data_id());
    EXPECT_EQ(data.instrument_id(), it->instrument_id());
    EXPECT_EQ(data.virtual_card_last_four(), it->virtual_card_last_four());
    EXPECT_EQ(data.merchant_origin().Serialize(),
              it->merchant_origin().Serialize());
  }
}

TEST_F(PaymentsAutofillTableTest, AddUpdateRemoveVirtualCardUsageData) {
  // Add a valid VirtualCardUsageData.
  VirtualCardUsageData virtual_card_usage_data =
      test::GetVirtualCardUsageData1();
  EXPECT_TRUE(table_->AddOrUpdateVirtualCardUsageData(virtual_card_usage_data));

  // Get the inserted VirtualCardUsageData.
  std::string usage_data_id = *virtual_card_usage_data.usage_data_id();
  std::optional<VirtualCardUsageData> usage_data =
      table_->GetVirtualCardUsageData(usage_data_id);
  ASSERT_TRUE(usage_data);
  EXPECT_EQ(virtual_card_usage_data, *usage_data);

  // Update the virtual card usage data.
  VirtualCardUsageData virtual_card_usage_data_update =
      VirtualCardUsageData(virtual_card_usage_data.usage_data_id(),
                           virtual_card_usage_data.instrument_id(),
                           VirtualCardUsageData::VirtualCardLastFour(u"4444"),
                           virtual_card_usage_data.merchant_origin());
  EXPECT_TRUE(
      table_->AddOrUpdateVirtualCardUsageData(virtual_card_usage_data_update));
  usage_data = table_->GetVirtualCardUsageData(usage_data_id);
  ASSERT_TRUE(usage_data);
  EXPECT_EQ(virtual_card_usage_data_update, *usage_data);

  // Remove the virtual card usage data.
  EXPECT_TRUE(table_->RemoveVirtualCardUsageData(usage_data_id));
  usage_data = table_->GetVirtualCardUsageData(usage_data_id);
  EXPECT_FALSE(usage_data);
}

TEST_F(PaymentsAutofillTableTest, RemoveAllVirtualCardUsageData) {
  EXPECT_TRUE(table_->AddOrUpdateVirtualCardUsageData(
      test::GetVirtualCardUsageData1()));

  EXPECT_TRUE(table_->RemoveAllVirtualCardUsageData());

  std::vector<VirtualCardUsageData> usage_data;
  EXPECT_TRUE(table_->GetAllVirtualCardUsageData(usage_data));
  EXPECT_TRUE(usage_data.empty());
}

TEST_F(PaymentsAutofillTableTest, GetMaskedBankAccounts) {
  // Populate masked_bank_accounts table.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(100, 'bank_name', 'account_number_suffix', 1, 'nickname', "
      "'http://display-icon-url.com');"
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(200, 'bank_name_2', 'account_number_suffix_2', 3, 'nickname_2', "
      "'http://display-icon-url2.com');"));

  std::vector<BankAccount> bank_accounts_from_db;
  table_->GetMaskedBankAccounts(bank_accounts_from_db);

  EXPECT_EQ(2u, bank_accounts_from_db.size());

  BankAccount bank_account_from_db_1 = bank_accounts_from_db.at(0);
  EXPECT_EQ(100, bank_account_from_db_1.payment_instrument().instrument_id());
  EXPECT_EQ(u"bank_name", bank_account_from_db_1.bank_name());
  EXPECT_EQ(u"account_number_suffix",
            bank_account_from_db_1.account_number_suffix());
  EXPECT_EQ(static_cast<BankAccount::AccountType>(1),
            bank_account_from_db_1.account_type());
  EXPECT_EQ(u"nickname",
            bank_account_from_db_1.payment_instrument().nickname());
  EXPECT_EQ(GURL("http://display-icon-url.com"),
            bank_account_from_db_1.payment_instrument().display_icon_url());

  BankAccount bank_account_from_db_2 = bank_accounts_from_db.at(1);
  EXPECT_EQ(200, bank_account_from_db_2.payment_instrument().instrument_id());
  EXPECT_EQ(u"bank_name_2", bank_account_from_db_2.bank_name());
  EXPECT_EQ(u"account_number_suffix_2",
            bank_account_from_db_2.account_number_suffix());
  EXPECT_EQ(static_cast<BankAccount::AccountType>(3),
            bank_account_from_db_2.account_type());
  EXPECT_EQ(u"nickname_2",
            bank_account_from_db_2.payment_instrument().nickname());
  EXPECT_EQ(GURL("http://display-icon-url2.com"),
            bank_account_from_db_2.payment_instrument().display_icon_url());
}

TEST_F(PaymentsAutofillTableTest,
       GetMaskedBankAccounts_BankAccountTypeOutOfBounds) {
  // Populate masked_bank_accounts table with the first row to have an invalid
  // bank account type with value 100.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(100, 'bank_name', 'account_number_suffix', 100, 'nickname', "
      "'http://display-icon-url.com');"
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(200, 'bank_name_2', 'account_number_suffix_2', 3, 'nickname_2', "
      "'http://display-icon-url2.com');"));

  std::vector<BankAccount> bank_accounts_from_db;
  table_->GetMaskedBankAccounts(bank_accounts_from_db);

  // Expect only one bank account since the other one has an invalid bank
  // account type.
  EXPECT_EQ(1u, bank_accounts_from_db.size());
  // Verify that the returned bank account maps to the second row in the table.
  BankAccount bank_account_from_db = bank_accounts_from_db.at(0);
  EXPECT_EQ(200, bank_account_from_db.payment_instrument().instrument_id());
}

TEST_F(PaymentsAutofillTableTest, SetMaskedBankAccounts) {
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(100, 'bank_name', 'account_number_suffix', 1, 'nickname', "
      "'http://display-icon-url.com');"
      "INSERT INTO masked_bank_accounts (instrument_id, bank_name, "
      "account_number_suffix, account_type, nickname, display_icon_url) "
      "VALUES(200, 'bank_name_2', 'account_number_suffix_2', 3, 'nickname_2', "
      "'http://display-icon-url2.com');"));

  // Verify that GetMaskedBankAccounts returns 2 bank accounts.
  std::vector<BankAccount> bank_accounts_from_db;
  table_->GetMaskedBankAccounts(bank_accounts_from_db);
  EXPECT_EQ(2u, bank_accounts_from_db.size());

  // Create bank account with different id from the ones above.
  BankAccount bank_account_to_store = test::CreatePixBankAccount(8000);
  std::vector<BankAccount> bank_accounts_to_store;
  bank_accounts_to_store.push_back(bank_account_to_store);
  table_->SetMaskedBankAccounts(bank_accounts_to_store);

  // Verify that GetMaskedBankAccounts returns 1 bank account.
  table_->GetMaskedBankAccounts(bank_accounts_from_db);
  EXPECT_EQ(1u, bank_accounts_from_db.size());

  // Verify that the instrument id of the returned bank account matches the one
  // that was stored.
  BankAccount bank_account_from_db = bank_accounts_from_db.at(0);
  EXPECT_EQ(8000, bank_account_from_db.payment_instrument().instrument_id());
}

TEST_F(PaymentsAutofillTableTest, GetAllCreditCardBenefits) {
  // Add benefits to the table.
  std::vector<CreditCardBenefit> input_benefits;
  input_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  input_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  input_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  EXPECT_TRUE(table_->SetCreditCardBenefits(input_benefits));

  // Check all valid benefits are added to table and are searchable.
  std::vector<CreditCardBenefit> output_benefits;
  EXPECT_TRUE(table_->GetAllCreditCardBenefits(output_benefits));
  EXPECT_EQ(input_benefits.size(), output_benefits.size());
  for (const auto& input_benefit : input_benefits) {
    // Find input benefits in outputs.
    auto output_benefit_find_result = base::ranges::find(
        output_benefits, get_benefit_id(input_benefit), get_benefit_id);
    EXPECT_NE(output_benefit_find_result, output_benefits.end());
    EXPECT_EQ(input_benefit, *output_benefit_find_result);
  }
}

TEST_F(PaymentsAutofillTableTest, AddInactiveCreditCardBenefit) {
  // Add one inactive benefit that will be available in the future to the table.
  std::vector<CreditCardBenefit> input_benefits;
  CreditCardMerchantBenefit inactive_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(inactive_benefit).SetStartTime(base::Time::Now() + base::Days(1));
  EXPECT_TRUE(inactive_benefit.IsValidForWriteFromSync());
  input_benefits.push_back(std::move(inactive_benefit));
  EXPECT_TRUE(table_->SetCreditCardBenefits(input_benefits));

  // Check the inactive benefit is added to table and is searchable.
  std::vector<CreditCardBenefit> output_benefits;
  EXPECT_TRUE(table_->GetAllCreditCardBenefits(output_benefits));
  ASSERT_EQ(1u, output_benefits.size());
  EXPECT_EQ(input_benefits[0], output_benefits[0]);
}

TEST_F(PaymentsAutofillTableTest, AddInvalidCreditCardBenefit) {
  // Attempt to add an invalid category benefit with unknown category and a
  // valid benefit.
  CreditCardFlatRateBenefit valid_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  ASSERT_TRUE(valid_benefit.IsValidForWriteFromSync());
  CreditCardCategoryBenefit invalid_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(invalid_benefit)
      .SetBenefitCategory(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory);
  ASSERT_FALSE(invalid_benefit.IsValidForWriteFromSync());

  CreditCardBenefitBase::BenefitId valid_input_benefit_id =
      valid_benefit.benefit_id();
  std::vector<CreditCardBenefit> input_benefits;
  input_benefits.push_back(std::move(valid_benefit));
  input_benefits.push_back(std::move(invalid_benefit));
  EXPECT_TRUE(table_->SetCreditCardBenefits(input_benefits));

  // Check invalid benefit will not be added to the table.
  std::vector<CreditCardBenefit> output_benefits;
  EXPECT_TRUE(table_->GetAllCreditCardBenefits(output_benefits));
  ASSERT_EQ(output_benefits.size(), 1u);
  EXPECT_EQ(valid_input_benefit_id, get_benefit_id(output_benefits[0]));
}

TEST_F(PaymentsAutofillTableTest, ClearCreditCardBenefits) {
  // Add benefits to the table.
  std::vector<CreditCardBenefit> input_benefits;
  input_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  input_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  input_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  EXPECT_TRUE(table_->SetCreditCardBenefits(input_benefits));

  std::vector<CreditCardBenefit> output_benefits;

  // Check benefits are added.
  EXPECT_TRUE(table_->GetAllCreditCardBenefits(output_benefits));
  EXPECT_EQ(input_benefits.size(), output_benefits.size());
  sql::Statement count_statement(table_->GetDbForTesting()->GetUniqueStatement(
      "SELECT COUNT(benefit_id) from benefit_merchant_domains"));
  EXPECT_TRUE(count_statement.Step());
  EXPECT_NE(0, count_statement.ColumnInt(0));

  table_->ClearAllCreditCardBenefits();

  // Check benefits and benefit merchant domains are removed.
  output_benefits.clear();
  EXPECT_TRUE(table_->GetAllCreditCardBenefits(output_benefits));
  EXPECT_TRUE(output_benefits.empty());

  count_statement.Reset(/*clear_bound_vars=*/true);
  EXPECT_TRUE(count_statement.Step());
  EXPECT_EQ(0, count_statement.ColumnInt(0));
}

TEST_F(PaymentsAutofillTableTest, GetCreditCardBenefitsForInstrumentId) {
  // Add benefits with different instrument ids to the table.
  std::vector<CreditCardBenefit> input_benefits = {
      test::GetActiveCreditCardFlatRateBenefit(),
      test::GetActiveCreditCardCategoryBenefit(),
      test::GetActiveCreditCardMerchantBenefit()};
  EXPECT_TRUE(table_->SetCreditCardBenefits(input_benefits));

  // Get the first benefit in the `input_benefits` from the table by instrument
  // id.
  std::vector<CreditCardBenefit> output_benefits;
  EXPECT_TRUE(table_->GetCreditCardBenefitsForInstrumentId(
      *absl::visit(
          [](const auto& benefit) {
            return benefit.linked_card_instrument_id();
          },
          input_benefits[0]),
      output_benefits));
  EXPECT_THAT(output_benefits, testing::ElementsAre(input_benefits[0]));
}

TEST_F(PaymentsAutofillTableTest,
       PaymentInstrument_StoresPaymentInstrumentWithBankAccount) {
  // Add a bank account payment instrument to the table.
  sync_pb::PaymentInstrument payment_instrument =
      test::CreatePaymentInstrumentWithBankAccount(1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments{
      payment_instrument};
  table_->SetPaymentInstruments(payment_instruments);

  // Retrieve the payment instruments.
  std::vector<sync_pb::PaymentInstrument> payment_instruments_from_table;
  table_->GetPaymentInstruments(payment_instruments_from_table);

  // Check that the payment instruments are equal.
  EXPECT_EQ(payment_instruments_from_table.size(), 1u);
  EXPECT_EQ(payment_instrument.instrument_id(),
            payment_instruments_from_table[0].instrument_id());
  auto table_bank_account = payment_instruments_from_table[0].bank_account();
  EXPECT_EQ(payment_instrument.bank_account().bank_name(),
            table_bank_account.bank_name());
  EXPECT_EQ(payment_instrument.bank_account().account_number_suffix(),
            table_bank_account.account_number_suffix());
  EXPECT_EQ(payment_instrument.bank_account().account_type(),
            table_bank_account.account_type());
}

TEST_F(PaymentsAutofillTableTest,
       PaymentInstrument_StoresPaymentInstrumentWithIban) {
  // Add an IBAN payment instrument to the table.
  sync_pb::PaymentInstrument payment_instrument =
      test::CreatePaymentInstrumentWithIban(1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments{
      payment_instrument};
  table_->SetPaymentInstruments(payment_instruments);

  // Retrieve the payment instruments.
  std::vector<sync_pb::PaymentInstrument> payment_instruments_from_table;
  table_->GetPaymentInstruments(payment_instruments_from_table);

  // Check that the payment instruments are equal.
  EXPECT_EQ(payment_instruments_from_table.size(), 1u);
  EXPECT_EQ(payment_instrument.instrument_id(),
            payment_instruments_from_table[0].instrument_id());
  auto table_iban = payment_instruments_from_table[0].iban();
  EXPECT_EQ(payment_instrument.iban().instrument_id(),
            table_iban.instrument_id());
  EXPECT_EQ(payment_instrument.iban().prefix(), table_iban.prefix());
  EXPECT_EQ(payment_instrument.iban().suffix(), table_iban.suffix());
  EXPECT_EQ(payment_instrument.iban().length(), table_iban.length());
  EXPECT_EQ(payment_instrument.iban().nickname(), table_iban.nickname());
}

TEST_F(PaymentsAutofillTableTest,
       PaymentInstrument_StoresPaymentInstrumentWithEwalletAccount) {
  // Add an eWallet payment instrument to the table.
  sync_pb::PaymentInstrument payment_instrument =
      test::CreatePaymentInstrumentWithEwalletAccount(1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments{
      payment_instrument};
  table_->SetPaymentInstruments(payment_instruments);

  // Retrieve the payment instruments.
  std::vector<sync_pb::PaymentInstrument> payment_instruments_from_table;
  table_->GetPaymentInstruments(payment_instruments_from_table);

  // Check that the payment instruments are equal.
  ASSERT_EQ(payment_instruments_from_table.size(), 1u);
  EXPECT_EQ(payment_instrument.instrument_id(),
            payment_instruments_from_table[0].instrument_id());
  EXPECT_EQ(
      payment_instrument.device_details().is_fido_enrolled(),
      payment_instruments_from_table[0].device_details().is_fido_enrolled());
  const sync_pb::EwalletDetails& table_ewallet =
      payment_instruments_from_table[0].ewallet_details();
  EXPECT_EQ(payment_instrument.ewallet_details().ewallet_name(),
            table_ewallet.ewallet_name());
  EXPECT_EQ(payment_instrument.ewallet_details().account_display_name(),
            table_ewallet.account_display_name());
  EXPECT_EQ(
      payment_instrument.ewallet_details().supported_payment_link_uris().size(),
      table_ewallet.supported_payment_link_uris().size());
  for (int i = 0; i < payment_instrument.ewallet_details()
                          .supported_payment_link_uris()
                          .size();
       ++i) {
    EXPECT_EQ(
        payment_instrument.ewallet_details().supported_payment_link_uris()[i],
        table_ewallet.supported_payment_link_uris()[i]);
  }
}

TEST_F(PaymentsAutofillTableTest,
       PaymentInstrument_StoresMultiplePaymentInstruments) {
  // Add multiple payment instruments with details to the table.
  sync_pb::PaymentInstrument bank_account_payment_instrument =
      test::CreatePaymentInstrumentWithBankAccount(1234);
  sync_pb::PaymentInstrument iban_payment_instrument =
      test::CreatePaymentInstrumentWithIban(5678);
  std::vector<sync_pb::PaymentInstrument> payment_instruments{
      bank_account_payment_instrument, iban_payment_instrument};
  table_->SetPaymentInstruments(payment_instruments);

  // Retrieve the payment instruments.
  std::vector<sync_pb::PaymentInstrument> payment_instruments_from_table;
  table_->GetPaymentInstruments(payment_instruments_from_table);

  // Check that both payment instruments exist in the table.
  EXPECT_EQ(payment_instruments_from_table.size(), 2u);
  EXPECT_TRUE(std::ranges::any_of(
      payment_instruments_from_table,
      [&bank_account_payment_instrument](sync_pb::PaymentInstrument& p) {
        return p.instrument_id() ==
               bank_account_payment_instrument.instrument_id();
      }));
  EXPECT_TRUE(std::ranges::any_of(
      payment_instruments_from_table,
      [&iban_payment_instrument](sync_pb::PaymentInstrument& p) {
        return p.instrument_id() == iban_payment_instrument.instrument_id();
      }));
}

TEST_F(PaymentsAutofillTableTest,
       PaymentInstrument_SetPaymentInstrumentsOverwritesExistingValues) {
  // Add the first payment instrument to the table.
  sync_pb::PaymentInstrument payment_instrument_1 =
      test::CreatePaymentInstrumentWithBankAccount(1234);
  std::vector<sync_pb::PaymentInstrument> payment_instruments{
      payment_instrument_1};
  table_->SetPaymentInstruments(payment_instruments);
  // Overwrite the existing payment instrument with a new instrument.
  sync_pb::PaymentInstrument payment_instrument_2 =
      test::CreatePaymentInstrumentWithBankAccount(5678);
  payment_instruments[0] = payment_instrument_2;
  table_->SetPaymentInstruments(payment_instruments);

  // Retrieve the payment instruments.
  std::vector<sync_pb::PaymentInstrument> payment_instruments_from_table;
  table_->GetPaymentInstruments(payment_instruments_from_table);

  // Check that the first payment instrument does not exist.
  EXPECT_FALSE(std::ranges::any_of(
      payment_instruments_from_table,
      [&payment_instrument_1](sync_pb::PaymentInstrument& p) {
        return p.instrument_id() == payment_instrument_1.instrument_id();
      }));
  // Check that the second payment instruments exists.
  EXPECT_TRUE(std::ranges::any_of(
      payment_instruments_from_table,
      [&payment_instrument_2](sync_pb::PaymentInstrument& p) {
        return p.instrument_id() == payment_instrument_2.instrument_id();
      }));
}

}  // namespace
}  // namespace autofill
