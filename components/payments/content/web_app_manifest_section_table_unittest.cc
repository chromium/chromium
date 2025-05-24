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
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.test");
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
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest));

  // Gets and verifys the manifest.
  std::vector<WebAppManifestSection> retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
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
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest_1));

  // Adds alicepay manifest to the table.
  std::vector<WebAppManifestSection> manifest_2;
  WebAppManifestSection manifest_2_section;
  manifest_2_section.id = "com.alicepay";
  manifest_2_section.min_version = static_cast<int64_t>(2);
  // Adds two finger prints.
  manifest_2_section.fingerprints.push_back(fingerprint_three);
  manifest_2_section.fingerprints.push_back(fingerprint_four);
  manifest_2.emplace_back(std::move(manifest_2_section));
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest_2));

  // Verifys bobpay manifest.
  std::vector<WebAppManifestSection> bobpay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
  ASSERT_EQ(bobpay_manifest.size(), 1U);
  ASSERT_EQ(bobpay_manifest[0].id, "com.bobpay");
  ASSERT_EQ(bobpay_manifest[0].min_version, 1);
  ASSERT_EQ(bobpay_manifest[0].fingerprints.size(), 2U);
  ASSERT_TRUE(bobpay_manifest[0].fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(bobpay_manifest[0].fingerprints[1] == fingerprint_two);

  // Verifys alicepay manifest.
  std::vector<WebAppManifestSection> alicepay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.alicepay");
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
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest));

  {
    // Verify the dev manifest.
    std::vector<WebAppManifestSection> actual_manifest =
        web_app_manifest_section_table->GetWebAppManifest("com.bobpay.dev");
    ASSERT_EQ(actual_manifest.size(), 1U);
    EXPECT_EQ(actual_manifest[0].id, "com.bobpay.dev");
    EXPECT_EQ(actual_manifest[0].min_version, 2);
    ASSERT_EQ(actual_manifest[0].fingerprints.size(), 1U);
    EXPECT_TRUE(actual_manifest[0].fingerprints[0] == fingerprint_dev);
  }

  {
    // Verify the prod manifest.
    std::vector<WebAppManifestSection> actual_manifest =
        web_app_manifest_section_table->GetWebAppManifest("com.bobpay.prod");
    ASSERT_EQ(actual_manifest.size(), 1U);
    EXPECT_EQ(actual_manifest[0].id, "com.bobpay.prod");
    EXPECT_EQ(actual_manifest[0].min_version, 1);
    ASSERT_EQ(actual_manifest[0].fingerprints.size(), 1U);
    EXPECT_TRUE(actual_manifest[0].fingerprints[0] == fingerprint_prod);
  }
}

}  // namespace

}  // namespace payments
