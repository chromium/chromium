// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// For testing purposes, it's useful to have a non-view version of a
// BrowsingDataEntry, so they can be put in vectors etc.
struct BrowsingDataEntry {
  BrowsingDataEntry(const std::string& primary_host,
                    BrowsingDataModel::DataKey data_key,
                    BrowsingDataModel::DataDetails data_details)
      : primary_host(primary_host),
        data_key(data_key),
        data_details(data_details) {}
  explicit BrowsingDataEntry(
      const BrowsingDataModel::BrowsingDataEntryView& view)
      : primary_host(view.primary_host),
        data_key(view.data_key),
        data_details(view.data_details) {}
  bool operator==(const BrowsingDataEntry& other) const {
    return primary_host == other.primary_host && data_key == other.data_key &&
           data_details == other.data_details;
  }
  std::string primary_host;
  BrowsingDataModel::DataKey data_key;
  BrowsingDataModel::DataDetails data_details;
};

// Check that the entries returned by `model` are a permutation of those in
// `expected_entries`, e.g. lists are equal without considering order.
void ValidateBrowsingDataEntries(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;

  for (const auto& entry : *model)
    model_entries.emplace_back(entry);

  EXPECT_THAT(expected_entries,
              testing::UnorderedElementsAreArray(model_entries));
}

}  // namespace

class BrowsingDataModelTest : public testing::Test {
 public:
  BrowsingDataModelTest() { model_ = BrowsingDataModel::BuildEmpty(); }
  ~BrowsingDataModelTest() override = default;

 protected:
  BrowsingDataModel* model() { return model_.get(); }

  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://subsite.example.com"));
  const std::string kSubdomainOriginHost = "subsite.example.com";
  const std::string kSubdomainOriginSite = "example.com";

  const url::Origin kSiteOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const std::string kSiteOriginHost = "example.com";

  const url::Origin kAnotherSiteOrigin =
      url::Origin::Create(GURL("https://another-example.com"));
  const std::string kAnotherSiteOriginHost = "another-example.com";

 private:
  std::unique_ptr<BrowsingDataModel> model_;
};

TEST_F(BrowsingDataModelTest, PrimaryHostMapping) {
  model()->AddBrowsingData(kSubdomainOrigin,
                           BrowsingDataModel::StorageType::kTrustTokens, 0, 1);
  model()->AddBrowsingData(
      blink::StorageKey(kSubdomainOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 123, 0);
  model()->AddBrowsingData(
      blink::StorageKey(kSubdomainOrigin),
      BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage, 456, 0);

  ValidateBrowsingDataEntries(
      model(),
      {{kSubdomainOriginHost,
        kSubdomainOrigin,
        {BrowsingDataModel::StorageType::kTrustTokens, 0, 1}},
       {kSubdomainOriginSite,
        blink::StorageKey(kSubdomainOrigin),
        {BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 123, 0}},
       {kSubdomainOriginHost,
        blink::StorageKey(kSubdomainOrigin),
        {BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage, 456, 0}}});
}

TEST_F(BrowsingDataModelTest, EntryCoalescense) {
  // Check that multiple entries are correctly coalesced.
  // Browsing data with the same primary_host + data_key pair should update the
  // same entry's details.
  model()->AddBrowsingData(
      blink::StorageKey(kSiteOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 123, 0);
  model()->AddBrowsingData(
      blink::StorageKey(kSiteOrigin),
      BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage, 234, 5);

  auto expected_entries = std::vector<BrowsingDataEntry>(
      {{kSiteOriginHost,
        blink::StorageKey(kSiteOrigin),
        {{BrowsingDataModel::StorageType::kPartitionedQuotaStorage,
          BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage},
         123 + 234,
         5}}});

  ValidateBrowsingDataEntries(model(), expected_entries);

  // Entries related to the same primary_host, but different data_keys, should
  // create a new entry.
  model()->AddBrowsingData(
      blink::StorageKey(kAnotherSiteOrigin),
      BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 345, 0);
  model()->AddBrowsingData(
      kAnotherSiteOrigin, BrowsingDataModel::StorageType::kTrustTokens, 456, 6);

  expected_entries.push_back(
      {kAnotherSiteOriginHost,
       blink::StorageKey(kAnotherSiteOrigin),
       {BrowsingDataModel::StorageType::kPartitionedQuotaStorage, 345}});
  expected_entries.push_back(
      {kAnotherSiteOriginHost,
       kAnotherSiteOrigin,
       {BrowsingDataModel::StorageType::kTrustTokens, 456, 6}});

  ValidateBrowsingDataEntries(model(), expected_entries);
}
