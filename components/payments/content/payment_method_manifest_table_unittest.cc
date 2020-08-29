// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_method_manifest_table.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

// Creates an icon for testing.
std::vector<uint8_t> CreateIcon(uint8_t first_byte = 0) {
  std::vector<uint8_t> icon;
  icon.push_back(first_byte++);
  icon.push_back(first_byte++);
  icon.push_back(first_byte++);
  icon.push_back(first_byte);
  return icon;
}

// Creates one credential identifier for testing.
std::vector<uint8_t> CreateCredentialId(uint8_t first_byte = 0) {
  std::vector<uint8_t> credential_id;
  credential_id.push_back(first_byte++);
  credential_id.push_back(first_byte++);
  credential_id.push_back(first_byte++);
  credential_id.push_back(first_byte);
  return credential_id;
}

// Creates a list of one credential identifier for testing.
std::vector<std::vector<uint8_t>> CreateCredentialIdList(
    uint8_t first_byte = 0) {
  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(first_byte));
  return credential_ids;
}

void ExpectOneValidInstrument(
    const std::vector<uint8_t>& credential_id,
    const std::string& relying_party_id,
    const std::string& label,
    const std::vector<uint8_t>& icon,
    std::vector<std::unique_ptr<SecurePaymentConfirmationInstrument>>
        instruments) {
  ASSERT_EQ(1U, instruments.size());
  ASSERT_NE(nullptr, instruments.back());
  ASSERT_TRUE(instruments.back()->IsValid());
  EXPECT_EQ(credential_id, instruments.back()->credential_id);
  EXPECT_EQ(relying_party_id, instruments.back()->relying_party_id);
  EXPECT_EQ(base::ASCIIToUTF16(label), instruments.back()->label);
  EXPECT_EQ(icon, instruments.back()->icon);
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
      payment_method_manifest_table->GetManifest("https://bobpay.com");
  ASSERT_TRUE(web_app_ids.empty());
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetSingleManifest) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  std::string method_name("https://bobpay.com");
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
      payment_method_manifest_table->GetManifest("https://bobpay.com");
  ASSERT_EQ(web_app_ids.size(), retrieved_web_app_ids.size());
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[0]));
  ASSERT_TRUE(base::Contains(retrieved_web_app_ids, web_app_ids[1]));
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetMultipleManifest) {
  PaymentMethodManifestTable* payment_method_manifest_table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  std::string method_name_1("https://bobpay.com");
  std::string method_name_2("https://alicepay.com");
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

TEST_F(PaymentMethodManifestTableTest, GetNonExistingInstrument) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(
      table->GetSecurePaymentConfirmationInstruments(CreateCredentialIdList())
          .empty());

  EXPECT_TRUE(table->GetSecurePaymentConfirmationInstruments({}).empty());

  EXPECT_TRUE(
      table->GetSecurePaymentConfirmationInstruments({std::vector<uint8_t>()})
          .empty());
}

TEST_F(PaymentMethodManifestTableTest, AddAndGetOneValidInstrument) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(), "relying-party.example",
          base::ASCIIToUTF16("Instrument label"), CreateIcon())));

  auto instruments =
      table->GetSecurePaymentConfirmationInstruments(CreateCredentialIdList());

  ExpectOneValidInstrument({0, 1, 2, 3}, "relying-party.example",
                           "Instrument label", {0, 1, 2, 3},
                           std::move(instruments));

  EXPECT_TRUE(table->GetSecurePaymentConfirmationInstruments({}).empty());
  EXPECT_TRUE(
      table->GetSecurePaymentConfirmationInstruments({std::vector<uint8_t>()})
          .empty());
}

TEST_F(PaymentMethodManifestTableTest, AddingInvalidInstrumentReturnsFalse) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  // An empty credential.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          /*credential_id=*/{}, "relying-party.example",
          base::ASCIIToUTF16("Instrument label"), CreateIcon())));

  // Empty relying party identifier.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(), /*relying_party_id=*/"",
          base::ASCIIToUTF16("Instrument label"), CreateIcon())));

  // Empty label.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(), "relying-party.example",
          /*label=*/base::string16(), CreateIcon())));

  // Empty icon.
  EXPECT_FALSE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(), "relying-party.example",
          base::ASCIIToUTF16("Instrument label"), {})));
}

TEST_F(PaymentMethodManifestTableTest, UpdatingInstrumentReturnsTrue) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/0), "relying-party.example",
          base::ASCIIToUTF16("Instrument label 1"),
          CreateIcon(/*first_byte=*/0))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/0), "relying-party.example",
          base::ASCIIToUTF16("Instrument label 2"),
          CreateIcon(/*first_byte=*/4))));

  auto instruments = table->GetSecurePaymentConfirmationInstruments(
      CreateCredentialIdList(/*first_byte=*/0));
  ExpectOneValidInstrument({0, 1, 2, 3}, "relying-party.example",
                           "Instrument label 2", {4, 5, 6, 7},
                           std::move(instruments));
}

TEST_F(PaymentMethodManifestTableTest,
       DifferentRelyingPartiesCannotUseSameCredentialIdentifier) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());
  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/0), "relying-party-1.example",
          base::ASCIIToUTF16("Instrument label 1"),
          CreateIcon(/*first_byte=*/0))));

  EXPECT_FALSE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/0), "relying-party-2.example",
          base::ASCIIToUTF16("Instrument label 2"),
          CreateIcon(/*first_byte=*/4))));

  auto instruments = table->GetSecurePaymentConfirmationInstruments(
      CreateCredentialIdList(/*first_byte=*/0));
  ExpectOneValidInstrument({0, 1, 2, 3}, "relying-party-1.example",
                           "Instrument label 1", {0, 1, 2, 3},
                           std::move(instruments));
}

TEST_F(PaymentMethodManifestTableTest, RelyingPartyCanHaveMultipleCredentials) {
  PaymentMethodManifestTable* table =
      PaymentMethodManifestTable::FromWebDatabase(db_.get());

  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/0), "relying-party.example",
          base::ASCIIToUTF16("Instrument label 1"),
          CreateIcon(/*first_byte=*/0))));

  EXPECT_TRUE(table->AddSecurePaymentConfirmationInstrument(
      SecurePaymentConfirmationInstrument(
          CreateCredentialId(/*first_byte=*/4), "relying-party.example",
          base::ASCIIToUTF16("Instrument label 2"),
          CreateIcon(/*first_byte=*/4))));

  auto instruments = table->GetSecurePaymentConfirmationInstruments(
      CreateCredentialIdList(/*first_byte=*/0));

  ExpectOneValidInstrument({0, 1, 2, 3}, "relying-party.example",
                           "Instrument label 1", {0, 1, 2, 3},
                           std::move(instruments));

  instruments = table->GetSecurePaymentConfirmationInstruments(
      CreateCredentialIdList(/*first_byte=*/4));

  ExpectOneValidInstrument({4, 5, 6, 7}, "relying-party.example",
                           "Instrument label 2", {4, 5, 6, 7},
                           std::move(instruments));

  std::vector<std::vector<uint8_t>> credential_ids;
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/0));
  credential_ids.push_back(CreateCredentialId(/*first_byte=*/4));

  instruments =
      table->GetSecurePaymentConfirmationInstruments(std::move(credential_ids));

  ASSERT_EQ(2U, instruments.size());

  ASSERT_NE(nullptr, instruments.front());
  ASSERT_TRUE(instruments.front()->IsValid());
  std::vector<uint8_t> expected_credential_id = {0, 1, 2, 3};
  EXPECT_EQ(expected_credential_id, instruments.front()->credential_id);
  EXPECT_EQ("relying-party.example", instruments.front()->relying_party_id);
  EXPECT_EQ(base::ASCIIToUTF16("Instrument label 1"),
            instruments.front()->label);
  std::vector<uint8_t> expected_icon = {0, 1, 2, 3};
  EXPECT_EQ(expected_icon, instruments.front()->icon);

  ASSERT_NE(nullptr, instruments.back());
  ASSERT_TRUE(instruments.back()->IsValid());
  expected_credential_id = {4, 5, 6, 7};
  EXPECT_EQ(expected_credential_id, instruments.back()->credential_id);
  EXPECT_EQ("relying-party.example", instruments.back()->relying_party_id);
  EXPECT_EQ(base::ASCIIToUTF16("Instrument label 2"),
            instruments.back()->label);
  expected_icon = {4, 5, 6, 7};
  EXPECT_EQ(expected_icon, instruments.back()->icon);
}

}  // namespace
}  // namespace payments
