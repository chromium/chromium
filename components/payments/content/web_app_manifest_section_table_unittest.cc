// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_app_manifest_section_table.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/webdata/common/web_database.h"
#include "sql/init_status.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class WebAppManifestSectionTableTest : public testing::Test {
 public:
  WebAppManifestSectionTableTest() = default;

  WebAppManifestSectionTableTest(const WebAppManifestSectionTableTest&) =
      delete;
  WebAppManifestSectionTableTest& operator=(
      const WebAppManifestSectionTableTest&) = delete;

  ~WebAppManifestSectionTableTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<WebAppManifestSectionTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override {}

  std::vector<uint8_t> GenerateFingerprint(uint8_t seed) {
    std::vector<uint8_t> fingerprint;
    // Note that the fingerprint is calculated with SHA-256, so the length is
    // 32.
    for (size_t i = 0; i < 32U; i++) {
      fingerprint.push_back((seed + i) % 256U);
    }
    return fingerprint;
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<WebAppManifestSectionTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(WebAppManifestSectionTableTest, GetNonExistManifest) {
  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());
  std::vector<WebAppManifestSection> retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test",
                                                        "com.bobpay");
  ASSERT_TRUE(retrieved_manifest.empty());
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetManifest) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);

  // create a bobpay web app manifest.
  std::vector<WebAppManifestSection> manifest;
  WebAppManifestSection section;
  section.id = "com.bobpay";
  section.min_version = static_cast<int64_t>(1);
  section.fingerprints.push_back(fingerprint_one);
  section.fingerprints.push_back(fingerprint_two);
  manifest.emplace_back(std::move(section));

  // Adds the manifest to the table.
  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(
      "https://bobpay.test", manifest));

  // Gets and verifys the manifest.
  std::vector<WebAppManifestSection> retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test",
                                                        "com.bobpay");
  ASSERT_EQ(retrieved_manifest.size(), 1U);
  ASSERT_EQ(retrieved_manifest[0].id, "com.bobpay");
  ASSERT_EQ(retrieved_manifest[0].min_version, 1);
  ASSERT_EQ(retrieved_manifest[0].fingerprints.size(), 2U);

  // Verify the two fingerprints.
  ASSERT_TRUE(retrieved_manifest[0].fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(retrieved_manifest[0].fingerprints[1] == fingerprint_two);
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetMultipleManifests) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);
  std::vector<uint8_t> fingerprint_three = GenerateFingerprint(2);
  std::vector<uint8_t> fingerprint_four = GenerateFingerprint(30);

  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());

  // Adds bobpay manifest to the table.
  std::vector<WebAppManifestSection> manifest_1;
  WebAppManifestSection manifest_1_section;
  manifest_1_section.id = "com.bobpay";
  manifest_1_section.min_version = static_cast<int64_t>(1);
  // Adds two finger prints.
  manifest_1_section.fingerprints.push_back(fingerprint_one);
  manifest_1_section.fingerprints.push_back(fingerprint_two);
  manifest_1.emplace_back(std::move(manifest_1_section));
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(
      "https://bobpay.test", manifest_1));

  // Adds alicepay manifest to the table.
  std::vector<WebAppManifestSection> manifest_2;
  WebAppManifestSection manifest_2_section;
  manifest_2_section.id = "com.alicepay";
  manifest_2_section.min_version = static_cast<int64_t>(2);
  // Adds two finger prints.
  manifest_2_section.fingerprints.push_back(fingerprint_three);
  manifest_2_section.fingerprints.push_back(fingerprint_four);
  manifest_2.emplace_back(std::move(manifest_2_section));
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(
      "https://alicepay.test", manifest_2));

  // Verifys bobpay manifest.
  std::vector<WebAppManifestSection> bobpay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test",
                                                        "com.bobpay");
  ASSERT_EQ(bobpay_manifest.size(), 1U);
  ASSERT_EQ(bobpay_manifest[0].id, "com.bobpay");
  ASSERT_EQ(bobpay_manifest[0].min_version, 1);
  ASSERT_EQ(bobpay_manifest[0].fingerprints.size(), 2U);
  ASSERT_TRUE(bobpay_manifest[0].fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(bobpay_manifest[0].fingerprints[1] == fingerprint_two);

  // Verifys alicepay manifest.
  std::vector<WebAppManifestSection> alicepay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://alicepay.test",
                                                        "com.alicepay");
  ASSERT_EQ(alicepay_manifest.size(), 1U);
  ASSERT_EQ(alicepay_manifest[0].id, "com.alicepay");
  ASSERT_EQ(alicepay_manifest[0].min_version, 2);
  ASSERT_EQ(alicepay_manifest[0].fingerprints.size(), 2U);
  ASSERT_TRUE(alicepay_manifest[0].fingerprints[0] == fingerprint_three);
  ASSERT_TRUE(alicepay_manifest[0].fingerprints[1] == fingerprint_four);
}

// A single manifest can have multiple package names, e.g., one for developer
// and one for production version of the app. A package name is unique among all
// the apps on Android, so this means we can define multiple apps in a single
// manifest.
TEST_F(WebAppManifestSectionTableTest, AddAndGetSingleManifestWithTwoIds) {
  std::vector<uint8_t> fingerprint_dev = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_prod = GenerateFingerprint(32);

  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());

  std::vector<WebAppManifestSection> manifest;
  {
    // Adds dev version to the manifest.
    WebAppManifestSection manifest_dev_section;
    manifest_dev_section.id = "com.bobpay.dev";
    manifest_dev_section.min_version = static_cast<int64_t>(2);
    manifest_dev_section.fingerprints.push_back(fingerprint_dev);
    manifest.emplace_back(std::move(manifest_dev_section));
  }
  {
    // Adds prod version to the manifest.
    WebAppManifestSection manifest_prod_section;
    manifest_prod_section.id = "com.bobpay.prod";
    manifest_prod_section.min_version = static_cast<int64_t>(1);
    manifest_prod_section.fingerprints.push_back(fingerprint_prod);
    manifest.emplace_back(std::move(manifest_prod_section));
  }
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(
      "https://bobpay.test", manifest));

  {
    // Verify the dev manifest.
    std::vector<WebAppManifestSection> actual_manifest =
        web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test",
                                                          "com.bobpay.dev");
    ASSERT_EQ(actual_manifest.size(), 1U);
    EXPECT_EQ(actual_manifest[0].id, "com.bobpay.dev");
    EXPECT_EQ(actual_manifest[0].min_version, 2);
    ASSERT_EQ(actual_manifest[0].fingerprints.size(), 1U);
    EXPECT_TRUE(actual_manifest[0].fingerprints[0] == fingerprint_dev);
  }

  {
    // Verify the prod manifest.
    std::vector<WebAppManifestSection> actual_manifest =
        web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test",
                                                          "com.bobpay.prod");
    ASSERT_EQ(actual_manifest.size(), 1U);
    EXPECT_EQ(actual_manifest[0].id, "com.bobpay.prod");
    EXPECT_EQ(actual_manifest[0].min_version, 1);
    ASSERT_EQ(actual_manifest[0].fingerprints.size(), 1U);
    EXPECT_TRUE(actual_manifest[0].fingerprints[0] == fingerprint_prod);
  }
}

// Tests that when multiple payment methods reference the same Android package
// name with different requirements (e.g. min_version and fingerprints), each
// payment method's cached web app manifest section remains isolated without
// cache collision or thrashing.
TEST_F(WebAppManifestSectionTableTest,
       MultiplePaymentMethodsSamePackageNameIsolation) {
  std::vector<uint8_t> fingerprint_v1 = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_v2 = GenerateFingerprint(2);

  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());

  const std::string method_a = "https://example.com/pay_v1";
  const std::string method_b = "https://example.com/pay_v2";
  const std::string package_name = "com.example.pay";

  // Method A requires min_version = 1.
  std::vector<WebAppManifestSection> manifest_a;
  WebAppManifestSection section_a;
  section_a.id = package_name;
  section_a.min_version = 1;
  section_a.fingerprints.push_back(fingerprint_v1);
  manifest_a.emplace_back(std::move(section_a));

  ASSERT_TRUE(
      web_app_manifest_section_table->AddWebAppManifest(method_a, manifest_a));

  // Verify Method A can be retrieved.
  std::vector<WebAppManifestSection> retrieved_a =
      web_app_manifest_section_table->GetWebAppManifest(method_a, package_name);
  ASSERT_EQ(retrieved_a.size(), 1U);
  EXPECT_EQ(retrieved_a[0].id, package_name);
  EXPECT_EQ(retrieved_a[0].min_version, 1);
  ASSERT_EQ(retrieved_a[0].fingerprints.size(), 1U);
  EXPECT_TRUE(retrieved_a[0].fingerprints[0] == fingerprint_v1);

  // Method B requires min_version = 500.
  std::vector<WebAppManifestSection> manifest_b;
  WebAppManifestSection section_b;
  section_b.id = package_name;
  section_b.min_version = 500;
  section_b.fingerprints.push_back(fingerprint_v2);
  manifest_b.emplace_back(std::move(section_b));

  ASSERT_TRUE(
      web_app_manifest_section_table->AddWebAppManifest(method_b, manifest_b));

  // Verify Method B has min_version = 500.
  std::vector<WebAppManifestSection> retrieved_b =
      web_app_manifest_section_table->GetWebAppManifest(method_b, package_name);
  ASSERT_EQ(retrieved_b.size(), 1U);
  EXPECT_EQ(retrieved_b[0].id, package_name);
  EXPECT_EQ(retrieved_b[0].min_version, 500);
  ASSERT_EQ(retrieved_b[0].fingerprints.size(), 1U);
  EXPECT_TRUE(retrieved_b[0].fingerprints[0] == fingerprint_v2);

  // Verify Method A was not overwritten or affected by Method B.
  retrieved_a =
      web_app_manifest_section_table->GetWebAppManifest(method_a, package_name);
  ASSERT_EQ(retrieved_a.size(), 1U);
  EXPECT_EQ(retrieved_a[0].id, package_name);
  EXPECT_EQ(retrieved_a[0].min_version, 1);
  ASSERT_EQ(retrieved_a[0].fingerprints.size(), 1U);
  EXPECT_TRUE(retrieved_a[0].fingerprints[0] == fingerprint_v1);
}

// Tests migration from version 154 (legacy table without method_name) to 155.
TEST_F(WebAppManifestSectionTableTest, MigrationVersion154ToCurrent) {
  // Reset and create legacy table without method_name column.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "DROP TABLE IF EXISTS web_app_manifest_section"));
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "CREATE TABLE web_app_manifest_section ( "
      "expire_date INTEGER NOT NULL DEFAULT 0, "
      "id VARCHAR, "
      "min_version INTEGER NOT NULL DEFAULT 0, "
      "fingerprints BLOB) "));
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO web_app_manifest_section (expire_date, id, min_version, "
      "fingerprints) VALUES (9999999, 'com.legacy', 1, X'0102')"));
  ASSERT_FALSE(db_->GetSQLConnection()->DoesColumnExist(
      "web_app_manifest_section", "method_name"));

  bool update_compatible_version = false;
  ASSERT_TRUE(table_->MigrateToVersion(155, &update_compatible_version));
  EXPECT_TRUE(db_->GetSQLConnection()->DoesColumnExist(
      "web_app_manifest_section", "method_name"));

  // Verify legacy ambiguous entry was cleared during migration.
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT COUNT(*) FROM web_app_manifest_section"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(s.ColumnInt(0), 0);

  // Verify table operations work post-migration.
  std::vector<uint8_t> fingerprint = GenerateFingerprint(1);
  std::vector<WebAppManifestSection> manifest;
  WebAppManifestSection section;
  section.id = "com.bobpay";
  section.min_version = 1;
  section.fingerprints.push_back(fingerprint);
  manifest.emplace_back(std::move(section));

  EXPECT_TRUE(table_->AddWebAppManifest("https://bobpay.test", manifest));
  std::vector<WebAppManifestSection> retrieved =
      table_->GetWebAppManifest("https://bobpay.test", "com.bobpay");
  ASSERT_EQ(retrieved.size(), 1U);
  EXPECT_EQ(retrieved[0].id, "com.bobpay");
  EXPECT_EQ(retrieved[0].min_version, 1);
}

}  // namespace

}  // namespace payments
