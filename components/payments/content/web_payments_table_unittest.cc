// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_payments_table.h"

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

class WebPaymentsTableTest : public testing::Test {
 public:
  WebPaymentsTableTest() = default;
  ~WebPaymentsTableTest() override = default;

  WebPaymentsTableTest(const WebPaymentsTableTest& other) = delete;
  WebPaymentsTableTest& operator=(const WebPaymentsTableTest& other) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<WebPaymentsTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<WebPaymentsTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(WebPaymentsTableTest, GetNonExistManifest) {
  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());
  std::vector<std::string> web_app_ids =
      web_payments_table->GetManifest("https://bobpay.test");
  ASSERT_TRUE(web_app_ids.empty());
}

TEST_F(WebPaymentsTableTest, AddAndGetSingleManifest) {
  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());

  std::string method_name("https://bobpay.test");
  std::vector<std::string> web_app_ids = {"com.bobpay"};
  ASSERT_TRUE(web_payments_table->AddManifest(method_name, web_app_ids));

  std::vector<std::string> retrieved_web_app_ids =
      web_payments_table->GetManifest(method_name);
  ASSERT_EQ(web_app_ids.size(), retrieved_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], retrieved_web_app_ids[0]);

  web_app_ids.emplace_back("com.alicepay");
  ASSERT_TRUE(web_payments_table->AddManifest(method_name, web_app_ids));

  retrieved_web_app_ids =
      web_payments_table->GetManifest("https://bobpay.test");
  ASSERT_EQ(web_app_ids.size(), retrieved_web_app_ids.size());
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[1]));
}

TEST_F(WebPaymentsTableTest, AddAndGetMultipleManifest) {
  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());

  std::string method_name_1("https://bobpay.test");
  std::string method_name_2("https://alicepay.test");
  std::vector<std::string> web_app_ids = {"com.bobpay"};
  ASSERT_TRUE(web_payments_table->AddManifest(method_name_1, web_app_ids));
  ASSERT_TRUE(web_payments_table->AddManifest(method_name_2, web_app_ids));

  std::vector<std::string> bobpay_web_app_ids =
      web_payments_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), bobpay_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], bobpay_web_app_ids[0]);

  std::vector<std::string> alicepay_web_app_ids =
      web_payments_table->GetManifest(method_name_2);
  ASSERT_EQ(web_app_ids.size(), alicepay_web_app_ids.size());
  ASSERT_EQ(web_app_ids[0], alicepay_web_app_ids[0]);

  web_app_ids.emplace_back("com.alicepay");
  ASSERT_TRUE(web_payments_table->AddManifest(method_name_1, web_app_ids));
  ASSERT_TRUE(web_payments_table->AddManifest(method_name_2, web_app_ids));

  bobpay_web_app_ids = web_payments_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), bobpay_web_app_ids.size());
  ASSERT_TRUE(base::Contains(bobpay_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(bobpay_web_app_ids, web_app_ids[1]));

  alicepay_web_app_ids = web_payments_table->GetManifest(method_name_1);
  ASSERT_EQ(web_app_ids.size(), alicepay_web_app_ids.size());
  ASSERT_TRUE(base::Contains(alicepay_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(alicepay_web_app_ids, web_app_ids[1]));
}

TEST_F(WebPaymentsTableTest, GetNonExistingCredential) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, AddAndGetOneValidCredential) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());

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

TEST_F(WebPaymentsTableTest, AddingInvalidCredentialReturnsFalse) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());

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

TEST_F(WebPaymentsTableTest, UpdatingCredentialReturnsTrue) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest,
       DifferentRelyingPartiesCannotUseSameCredentialIdentifier) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, RelyingPartyCanHaveMultipleCredentials) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, SameRelyingPartyAndUserIdOverwritesCredential) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, ClearCredentials) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, ClearCredentials_NotDeleteOutOfTimeRange) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

TEST_F(WebPaymentsTableTest, CredentialTableAddDateCreatedAndUserIdColumn) {
  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(web_payments_table->RazeForTest());
  EXPECT_TRUE(web_payments_table->ExecuteForTest(
      "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
      "credential_id BLOB NOT NULL PRIMARY KEY, "
      "relying_party_id VARCHAR NOT NULL, "
      "label VARCHAR NOT NULL, "
      "icon BLOB NOT NULL)"));
  EXPECT_FALSE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "date_created"));
  EXPECT_FALSE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
  EXPECT_TRUE(web_payments_table->CreateTablesIfNecessary());
  EXPECT_TRUE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "date_created"));
  EXPECT_TRUE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
}

// Test migrating an existing credential table that didn't have the user ID.
TEST_F(WebPaymentsTableTest, CredentialTableUserIdMigration) {
  SecurePaymentConfirmationCredential valid_legacy_credential(
      CreateCredentialId(/*first_byte=*/0), "relying-party.example", {});
  EXPECT_TRUE(valid_legacy_credential.IsValid());
  EXPECT_FALSE(valid_legacy_credential.IsValidNewCredential());
  SecurePaymentConfirmationCredential valid_new_credential(
      CreateCredentialId(/*first_byte=*/1), "relying-party.example",
      CreateUserId());
  EXPECT_TRUE(valid_new_credential.IsValid());
  EXPECT_TRUE(valid_new_credential.IsValidNewCredential());

  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(web_payments_table->RazeForTest());
  // Create the SPC table as it would have been prior to storing the user ID.
  EXPECT_TRUE(web_payments_table->ExecuteForTest(
      "CREATE TABLE IF NOT EXISTS secure_payment_confirmation_instrument ( "
      "credential_id BLOB NOT NULL PRIMARY KEY, "
      "relying_party_id VARCHAR NOT NULL, "
      "label VARCHAR NOT NULL, "
      "icon BLOB NOT NULL, "
      "date_created INTEGER NOT NULL DEFAULT 0)"));

  // Insert the legacy credential.
  EXPECT_TRUE(web_payments_table->ExecuteForTest(
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

  EXPECT_FALSE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));
  EXPECT_TRUE(web_payments_table->CreateTablesIfNecessary());
  EXPECT_TRUE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_instrument", "user_id"));

  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
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

// Test migrating an existing browser bound key table that didn't have the
// `last_used` column.
TEST_F(WebPaymentsTableTest, BrowserBoundKeyTableLastUsedMigration) {
  WebPaymentsTable* web_payments_table =
      WebPaymentsTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(web_payments_table->RazeForTest());
  // Create the BBK table as it would have been prior to storing the last used.
  EXPECT_TRUE(web_payments_table->ExecuteForTest(
      "CREATE TABLE IF NOT EXISTS "
      "secure_payment_confirmation_browser_bound_key ( "
      "credential_id BLOB NOT NULL, "
      "relying_party_id TEXT NOT NULL, "
      "browser_bound_key_id BLOB, "
      "PRIMARY KEY (credential_id, relying_party_id))"));

  // Insert a legacy BBK.
  EXPECT_TRUE(web_payments_table->ExecuteForTest(
      ("INSERT INTO secure_payment_confirmation_browser_bound_key "
       "(credential_id, relying_party_id, browser_bound_key_id) "
       "VALUES ("
       " x'00012345',"
       " 'relying-party.example',"
       " x'00054321')")));

  EXPECT_FALSE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_browser_bound_key", "last_used"));
  EXPECT_TRUE(web_payments_table->CreateTablesIfNecessary());
  EXPECT_TRUE(web_payments_table->DoesColumnExistForTest(
      "secure_payment_confirmation_browser_bound_key", "last_used"));
}

// Tests that a browser bound key can be added and retrieved using the
// credential id and relying party id.
TEST_F(WebPaymentsTableTest, SetBrowserBoundKey) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                        browser_bound_key_id,
                                        /*last_used=*/std::nullopt));
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id =
      table->GetBrowserBoundKey(credential_id, relying_party_id);

  EXPECT_EQ(browser_bound_key_id, actual_browser_bound_key_id);
}

// Tests that no result is returned for a credential id and relying party id
// that do not exist in the table.
TEST_F(WebPaymentsTableTest, GetBrowserBoundKeyWhenNotFound) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});

  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id =
      table->GetBrowserBoundKey(credential_id, relying_party_id);

  EXPECT_EQ(std::nullopt, actual_browser_bound_key_id);
}

// Tests that no result is returned when either the credential id or relying
// party id arguments were empty.
TEST_F(WebPaymentsTableTest, GetBrowserBoundKeyWhenEmptyArguments) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});

  EXPECT_EQ(std::nullopt,
            table->GetBrowserBoundKey(/*credential_id=*/{}, relying_party_id));
  EXPECT_EQ(std::nullopt,
            table->GetBrowserBoundKey(credential_id, /*relying_party_id=*/{}));
}

// Tests that no entry is stored when empty credential id, relying party id, or
// browser bound key id are provided.
TEST_F(WebPaymentsTableTest, SetBrowserBoundKeyWhenEmptyArguments) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});

  EXPECT_FALSE(table->SetBrowserBoundKey(/*credential_id=*/{}, relying_party_id,
                                         browser_bound_key_id,
                                         /*last_used=*/std::nullopt));
  EXPECT_EQ(std::nullopt,
            table->GetBrowserBoundKey(/*credential_id=*/{}, relying_party_id));
  EXPECT_FALSE(table->SetBrowserBoundKey(credential_id, /*relying_party_id=*/"",
                                         browser_bound_key_id,
                                         /*last_used=*/std::nullopt));
  EXPECT_EQ(std::nullopt,
            table->GetBrowserBoundKey(credential_id, /*relying_party_id=*/{}));
  EXPECT_FALSE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                         /*browser_bound_key_id=*/{},
                                         /*last_used=*/std::nullopt));
  EXPECT_EQ(std::nullopt,
            table->GetBrowserBoundKey(credential_id, relying_party_id));
}

// Tests that two browser bound key ids can be set for two different relying
// party ids and the same credential id.
TEST_F(WebPaymentsTableTest, SetBrowserBoundKeyWhenSameCredentialId) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id_1("relying-party-1.example");
  std::string relying_party_id_2("relying-party-2.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id_1({0x11, 0x12, 0x13, 0x14});
  std::vector<uint8_t> browser_bound_key_id_2({0x21, 0x22, 0x23, 0x24});

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id_1,
                                        browser_bound_key_id_1,
                                        /*last_used=*/std::nullopt));
  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id_2,
                                        browser_bound_key_id_2,
                                        /*last_used=*/std::nullopt));
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id_1 =
      table->GetBrowserBoundKey(credential_id, relying_party_id_1);
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id_2 =
      table->GetBrowserBoundKey(credential_id, relying_party_id_2);

  EXPECT_EQ(browser_bound_key_id_1, actual_browser_bound_key_id_1);
  EXPECT_EQ(browser_bound_key_id_2, actual_browser_bound_key_id_2);
}

// Test that two browser bound key ids can be set for two different credential
// ids and the same relying party.
TEST_F(WebPaymentsTableTest, SetBrowserBoundKeyWhenSameRelyingParty) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id_1({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> credential_id_2({0x11, 0x12, 0x13, 0x14});
  std::vector<uint8_t> browser_bound_key_id_1({0x21, 0x22, 0x23, 0x24});
  std::vector<uint8_t> browser_bound_key_id_2({0x31, 0x32, 0x33, 0x34});

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id_1, relying_party_id,
                                        browser_bound_key_id_1,
                                        /*last_used=*/std::nullopt));
  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id_2, relying_party_id,
                                        browser_bound_key_id_2,
                                        /*last_used=*/std::nullopt));
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id_1 =
      table->GetBrowserBoundKey(credential_id_1, relying_party_id);
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id_2 =
      table->GetBrowserBoundKey(credential_id_2, relying_party_id);

  EXPECT_EQ(browser_bound_key_id_1, actual_browser_bound_key_id_1);
  EXPECT_EQ(browser_bound_key_id_2, actual_browser_bound_key_id_2);
}

// Tests that setting another browser bound key id for the same credential id
// and relying party id does not replace the first browser bound key id.
TEST_F(WebPaymentsTableTest, SetBrowserBoundKeyWhenDuplicateEntry) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id_1({0x11, 0x12, 0x13, 0x14});
  std::vector<uint8_t> browser_bound_key_id_2({0x21, 0x22, 0x23, 0x24});

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                        browser_bound_key_id_1,
                                        /*last_used=*/std::nullopt));
  // Expect a false return value since the primary key already exists.
  EXPECT_FALSE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                         browser_bound_key_id_2,
                                         /*last_used=*/std::nullopt));
  std::optional<std::vector<uint8_t>> actual_browser_bound_key_id =
      table->GetBrowserBoundKey(credential_id, relying_party_id);

  // Expect the first browser bound key id stored to be unaffected.
  EXPECT_EQ(browser_bound_key_id_1, actual_browser_bound_key_id);
}

// Tests that setting a browser bound key with the last used timestamp will be
// returned when getting all browser bound keys.
TEST_F(WebPaymentsTableTest, GetAllBrowserBoundKeysWithLastUsedTimestamp) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});
  base::Time last_used;
  ASSERT_TRUE(base::Time::FromUTCString("24 Oct 2025 10:30", &last_used));

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                        browser_bound_key_id, last_used));
  std::vector<BrowserBoundKeyMetadata> browser_bound_keys =
      table->GetAllBrowserBoundKeys();

  EXPECT_EQ(browser_bound_keys.size(), 1U);
  EXPECT_EQ(browser_bound_keys[0].passkey.credential_id, credential_id);
  EXPECT_EQ(browser_bound_keys[0].passkey.relying_party_id, relying_party_id);
  EXPECT_EQ(browser_bound_keys[0].browser_bound_key_id, browser_bound_key_id);
  EXPECT_EQ(browser_bound_keys[0].last_used, last_used);
}

// Tests that setting a browser bound key without the last used timestamp will
// be returned when getting all browser bound keys.
TEST_F(WebPaymentsTableTest, GetAllBrowserBoundKeysWithoutLastUsedTimestamp) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});

  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                        browser_bound_key_id, std::nullopt));
  std::vector<BrowserBoundKeyMetadata> browser_bound_keys =
      table->GetAllBrowserBoundKeys();

  EXPECT_EQ(browser_bound_keys.size(), 1U);
  EXPECT_EQ(browser_bound_keys[0].passkey.credential_id, credential_id);
  EXPECT_EQ(browser_bound_keys[0].passkey.relying_party_id, relying_party_id);
  EXPECT_EQ(browser_bound_keys[0].browser_bound_key_id, browser_bound_key_id);
  // If no last used timestamp was provided, the value should be null.
  EXPECT_TRUE(browser_bound_keys[0].last_used.is_null());
}

// Tests that the `last_used` column on the browser bound key can be updated.
TEST_F(WebPaymentsTableTest, UpdateBrowserBoundKeyLastUsedColumn) {
  WebPaymentsTable* table = WebPaymentsTable::FromWebDatabase(db_.get());
  std::string relying_party_id("relying-party.example");
  std::vector<uint8_t> credential_id({0x01, 0x02, 0x03, 0x04});
  std::vector<uint8_t> browser_bound_key_id({0x11, 0x12, 0x13, 0x14});
  base::Time initial_last_used;
  ASSERT_TRUE(
      base::Time::FromUTCString("24 Oct 2025 10:30", &initial_last_used));
  EXPECT_TRUE(table->SetBrowserBoundKey(credential_id, relying_party_id,
                                        browser_bound_key_id,
                                        initial_last_used));

  base::Time updated_last_used;
  ASSERT_TRUE(
      base::Time::FromUTCString("04 Dec 2025 10:30", &updated_last_used));
  EXPECT_TRUE(table->UpdateBrowserBoundKeyLastUsedColumn(
      credential_id, relying_party_id, updated_last_used));

  std::vector<BrowserBoundKeyMetadata> browser_bound_keys =
      table->GetAllBrowserBoundKeys();

  EXPECT_EQ(browser_bound_keys.size(), 1U);
  EXPECT_EQ(browser_bound_keys[0].last_used, updated_last_used);
}

}  // namespace
}  // namespace payments
