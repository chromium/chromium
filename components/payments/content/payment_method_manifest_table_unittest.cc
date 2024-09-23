// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_method_manifest_table.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webdata/common/web_database.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

// Creates one identifier (for a credential or user) for testing.
std::vector<uint8_t> CreateIdentifier(uint8_t first_byte) {
  std::vector<uint8_t> identifier;
  identifier.push_back(first_byte++);
  identifier.push_back(first_byte++);
  identifier.push_back(first_byte++);
  identifier.push_back(first_byte);
  return identifier;
}

// Creates one credential identifier for testing. Equivalent to calling
// CreateIdentifier, used for test readability.
std::vector<uint8_t> CreateCredentialId(uint8_t first_byte = 0) {
  return CreateIdentifier(first_byte);
}

// Creates a list of one credential identifier for testing.
std::vector<std::vector<uint8_t>> CreateCredentialIdList(
    uint8_t first_byte = 0) {
  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(first_byte));
  return credential_ids;
}

// Creates one user identifier for testing. Equivalent to calling
// CreateIdentifier, used for test readability.
std::vector<uint8_t> CreateUserId(uint8_t first_byte = 4) {
  return CreateIdentifier(first_byte);
}

void ExpectOneValidCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials) {
  ASSERT_EQ(1U, credentials.size());
  ASSERT_NE(nullptr, credentials.back());
  ASSERT_TRUE(credentials.back()->IsValidNewCredential());
  EXPECT_EQ(credential_id, credentials.back()->credential_id);
  EXPECT_EQ(relying_party_id, credentials.back()->relying_party_id);
  EXPECT_EQ(user_id, credentials.back()->user_id);
}

class PaymentMethodManifestTableTest : public testing::Test {
 public:
  PaymentMethodManifestTableTest() = default;
  ~PaymentMethodManifestTableTest() override = default;

  PaymentMethodManifestTableTest(const PaymentMethodManifestTableTest& other) =
      delete;
  PaymentMethodManifestTableTest& operator=(
      const PaymentMethodManifestTableTest& other) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<PaymentMethodManifestTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PaymentMethodManifestTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(PaymentMethodManifestTableTest, GetNonExistManifest) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::vector<std::string> web_app_ids =
      payment_method_manifest_table->GetManifest("https://bobpay.test");
  ASSERT_TRUE(web_app_ids.empty());
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetSingleManifest) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  std::string method_name("https://bobpay.test");
  std::vector<std::string> web_app_ids = {"com.bobpay"};
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name, web_app_ids));

  std::vector<std::string> retrieved_web_app_ids =
      payment_method_manifest_table->GetManifest(method_name);
  ASSERT_EQ(web_app_ids.size(), retrieved_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], retrieved_web_app_ids[0]);

  web_app_ids.emplace_back("com.alicepay");
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name, web_app_ids));

  retrieved_web_app_ids =
      payment_method_manifest_table->GetManifest("https://bobpay.test");
  ASSERT_EQ(web_app_ids.size(), retrieved_web_app_ids.size());
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[1]));
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetMultipleManifest) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  std::string method_name_1("https://bobpay.test");
  std::string method_name_2("https://alicepay.test");
  std::vector<std::string> web_app_ids = {"com.bobpay"};
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name_1, web_app_ids));
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name_2, web_app_ids));

  std::vector<std::string> bobpay_web_app_ids =
      payment_method_manifest_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), bobpay_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], bobpay_web_app_ids[0]);

  std::vector<std::string> alicepay_web_app_ids =
      payment_method_manifest_table->GetManifest(method_name_2);
  ASSERT_EQ(web_app_ids.size(), alicepay_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], alicepay_web_app_ids[0]);

  web_app_ids.emplace_back("com.alicepay");
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name_1, web_app_ids));
  ASSERT_TRUE(
      payment_method_manifest_table->AddManifest(method_name_2, web_app_ids));

  bobpay_web_app_ids =
      payment_method_manifest_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), bobpay_web_app_ids.size());
  ASSERT_TRUE(base::Contains(bobpay_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(bobpay_web_app_ids, web_app_ids[1]));

  alicepay_web_app_ids =
      payment_method_manifest_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), alicepay_web_app_ids.size());
  ASSERT_TRUE(base::Contains(alicepay_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(alicepay_web_app_ids, web_app_ids[1]));
}

TEST_F(PaymentMethodManifestTableTest, GetNonExistingCredential) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      CreateCredentialIdList(), relying_party_id)
                  .empty());

  EXPECT_TRUE(
      table->GetSecurePaymentConfirmationCredentials({}, relying_party_id)
          .empty());

  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      {std::vector<uint8_t>()}, relying_party_id)
                  .empty());
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetOneValidCredential) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  std::string relying_party_id("relying-party.example");
  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(),
                                          relying_party_id, CreateUserId())));

  auto credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(), relying_party_id);

  ExpectOneValidCredential({0, 1, 2, 3}, relying_party_id, {4, 5, 6, 7},
                           std::move(credentials));

  EXPECT_TRUE(
      table->GetSecurePaymentConfirmationCredentials({}, relying_party_id)
          .empty());
  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      {std::vector<uint8_t>()}, relying_party_id)
                  .empty());
  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      CreateCredentialIdList(), /*relying_party_id=*/"")
                  .empty());
}

TEST_F(PaymentMethodManifestTableTest, AddingInvalidCredentialReturnsFalse) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  // An empty credential.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(
          /*credential_id=*/{}, "relying-party.example", CreateUserId())));

  // Empty relying party identifier.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(),
                                          /*relying_party_id=*/"",
                                          CreateUserId())));

  // Empty user id.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(),
                                          "relying-party.example",
                                          /*user_id=*/{})));
}

TEST_F(PaymentMethodManifestTableTest, UpdatingCredentialReturnsTrue) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  auto credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(/*first_byte=*/0), relying_party_id);
  ExpectOneValidCredential({0, 1, 2, 3}, relying_party_id, {4, 5, 6, 7},
                           std::move(credentials));
}

TEST_F(PaymentMethodManifestTableTest,
       DifferentRelyingPartiesCannotUseSameCredentialIdentifier) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          "relying-party-1.example",
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_FALSE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          "relying-party-2.example",
                                          CreateUserId(/*first_byte=*/5))));

  auto credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(/*first_byte=*/0), "relying-party-1.example");
  ExpectOneValidCredential({0, 1, 2, 3}, "relying-party-1.example",
                           {4, 5, 6, 7}, std::move(credentials));
  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      CreateCredentialIdList(/*first_byte=*/0),
                      "relying-party-2.example")
                  .empty());
}

TEST_F(PaymentMethodManifestTableTest, RelyingPartyCanHaveMultipleCredentials) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/4),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/8))));

  auto credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(/*first_byte=*/0), relying_party_id);

  ExpectOneValidCredential({0, 1, 2, 3}, relying_party_id, {4, 5, 6, 7},
                           std::move(credentials));

  credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(/*first_byte=*/4), relying_party_id);

  ExpectOneValidCredential({4, 5, 6, 7}, relying_party_id, {8, 9, 10, 11},
                           std::move(credentials));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/4));

  credentials = table->GetSecurePaymentConfirmationCredentials(
      std::move(credential_ids), relying_party_id);

  ASSERT_EQ(2U, credentials.size());

  ASSERT_NE(nullptr, credentials.front());
  ASSERT_TRUE(credentials.front()->IsValidNewCredential());
  std::vector<uint8_t> expected_credential_id = {0, 1, 2, 3};
  EXPECT_EQ(expected_credential_id, credentials.front()->credential_id);
  EXPECT_EQ("relying-party.example", credentials.front()->relying_party_id);
  std::vector<uint8_t> expected_user_id = {4, 5, 6, 7};
  EXPECT_EQ(expected_user_id, credentials.front()->user_id);

  ASSERT_NE(nullptr, credentials.back());
  ASSERT_TRUE(credentials.back()->IsValidNewCredential());
  expected_credential_id = {4, 5, 6, 7};
  EXPECT_EQ(expected_credential_id, credentials.back()->credential_id);
  EXPECT_EQ("relying-party.example", credentials.back()->relying_party_id);
  expected_user_id = {8, 9, 10, 11};
  EXPECT_EQ(expected_user_id, credentials.back()->user_id);
}

TEST_F(PaymentMethodManifestTableTest,
       SameRelyingPartyAndUserIdOverwritesCredential) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/4),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(
      table
          ->GetSecurePaymentConfirmationCredentials(
              CreateCredentialIdList(/*first_byte=*/0), relying_party_id)
          .empty());

  auto credentials = table->GetSecurePaymentConfirmationCredentials(
      CreateCredentialIdList(/*first_byte=*/4), relying_party_id);

  ExpectOneValidCredential({4, 5, 6, 7}, relying_party_id, {4, 5, 6, 7},
                           std::move(credentials));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/4));

  credentials = table->GetSecurePaymentConfirmationCredentials(
      std::move(credential_ids), relying_party_id);

  ASSERT_EQ(1U, credentials.size());

  ASSERT_NE(nullptr, credentials.front());
  ASSERT_TRUE(credentials.front()->IsValidNewCredential());
  std::vector<uint8_t> expected_credential_id = {4, 5, 6, 7};
  EXPECT_EQ(expected_credential_id, credentials.front()->credential_id);
  EXPECT_EQ(relying_party_id, credentials.front()->relying_party_id);
  std::vector<uint8_t> expected_user_id = {4, 5, 6, 7};
  EXPECT_EQ(expected_user_id, credentials.front()->user_id);
}

TEST_F(PaymentMethodManifestTableTest, ClearCredentials) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/1),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/8))));

  table->ClearSecurePaymentConfirmationCredentials(
      base::Time::Now() - base::Minutes(1),
      base::Time::Now() + base::Minutes(1));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/1));
  EXPECT_TRUE(table
                  ->GetSecurePaymentConfirmationCredentials(
                      std::move(credential_ids), relying_party_id)
                  .empty());
}

TEST_F(PaymentMethodManifestTableTest,
       ClearCredentials_NotDeleteOutOfTimeRange) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/0),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/4))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationCredential(
      SecurePaymentConfirmationCredential(CreateCredentialId(/*first_byte=*/1),
                                          relying_party_id,
                                          CreateUserId(/*first_byte=*/8))));

  table->ClearSecurePaymentConfirmationCredentials(
      base::Time(), base::Time::Now() - base::Minutes(1));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/1));
  EXPECT_EQ(2u, table
                    ->GetSecurePaymentConfirmationCredentials(
                        std::move(credential_ids), std::move(relying_party_id))
                    .size());
}

TEST_F(PaymentMethodManifestTableTest,
       CredentialTableAddDateCreatedAndUserIdColumn) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(payment_method_manifest_table->RazeForTest());
  EXPECT_TRUE(payment_method_manifest_table->ExecuteForTest(
      "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
      "credential_id BLOB NOT NULL PRIMARY KEY, "
      "relying_party_id VARCHAR NOT NULL, "
      "label VARCHAR NOT NULL, "
      "icon BLOB NOT NULL)"));
  EXPECT_FALSE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "date_created"));
  EXPECT_FALSE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
  EXPECT_TRUE(payment_method_manifest_table->CreateTablesIfNecessary());
  EXPECT_TRUE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "date_created"));
  EXPECT_TRUE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
}

// Test migrating an existing credential table that didn't have the user ID.
TEST_F(PaymentMethodManifestTableTest, CredentialTableUserIdMigration) {
  SecurePaymentConfirmationCredential valid_legacy_credential(
      CreateCredentialId(/*first_byte=*/0), "relying-party.example", {});
  EXPECT_TRUE(valid_legacy_credential.IsValid());
  EXPECT_FALSE(valid_legacy_credential.IsValidNewCredential());
  SecurePaymentConfirmationCredential valid_new_credential(
      CreateCredentialId(/*first_byte=*/1), "relying-party.example",
      CreateUserId());
  EXPECT_TRUE(valid_new_credential.IsValid());
  EXPECT_TRUE(valid_new_credential.IsValidNewCredential());

  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(payment_method_manifest_table->RazeForTest());
  // Create the SPC table as it would have been prior to storing the user ID.
  EXPECT_TRUE(payment_method_manifest_table->ExecuteForTest(
      "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
      "credential_id BLOB NOT NULL PRIMARY KEY, "
      "relying_party_id VARCHAR NOT NULL, "
      "label VARCHAR NOT NULL, "
      "icon BLOB NOT NULL, "
      "date_created INTEGER NOT NULL DEFAULT 0)"));

  // Insert the legacy credential.
  EXPECT_TRUE(payment_method_manifest_table->ExecuteForTest(
      ("INSERT INTO secure_payment_confirmation_instrument "
       "(credential_id, relying_party_id, label, icon, date_created) "
       "VALUES ("
       " x'00010203',"
       " 'relying-party.example',"
       " '',"
       " x''," +
       base::NumberToString(
           base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds()) +
       ")")));

  EXPECT_FALSE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
  EXPECT_TRUE(payment_method_manifest_table->CreateTablesIfNecessary());
  EXPECT_TRUE(payment_method_manifest_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));

  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(
      table->AddSecurePaymentConfirmationCredential(valid_new_credential));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/1));
  EXPECT_EQ(2u, table
                    ->GetSecurePaymentConfirmationCredentials(
                        std::move(credential_ids), "relying-party.example")
                    .size());
}

}  // namespace
}  // namespace payments
