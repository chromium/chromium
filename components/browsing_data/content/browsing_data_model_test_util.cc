// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model_test_util.h"

#include "components/browsing_data/content/browsing_data_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browsing_data_model_test_util {

namespace {

struct DataKeyDebugStringVisitor {
  template <class T>
  std::string operator()(const T& data_key);
};

template <>
std::string DataKeyDebugStringVisitor::operator()<url::Origin>(
    const url::Origin& origin) {
  std::stringstream debug_string;
  debug_string << "Origin: " << origin.Serialize();
  return debug_string.str();
}

template <>
std::string DataKeyDebugStringVisitor::operator()<blink::StorageKey>(
    const blink::StorageKey& storage_key) {
  std::stringstream debug_string;
  debug_string << "blink::StorageKey: " << storage_key.GetDebugString();
  return debug_string.str();
}

template <>
std::string DataKeyDebugStringVisitor::operator()<
    content::InterestGroupManager::InterestGroupDataKey>(
    const content::InterestGroupManager::InterestGroupDataKey&
        interest_group_data_key) {
  std::stringstream debug_string;
  debug_string << "InterestGroupDataKey: ";
  debug_string << "{owner: " << interest_group_data_key.owner.Serialize();
  debug_string << " joining_origin: ";
  debug_string << interest_group_data_key.joining_origin.Serialize() << "}";
  return debug_string.str();
}

template <>
std::string
DataKeyDebugStringVisitor::operator()<content::AttributionDataModel::DataKey>(
    const content::AttributionDataModel::DataKey& attribution_data_key) {
  std::stringstream debug_string;
  debug_string << "AttributionDataKey: ";
  debug_string << attribution_data_key.reporting_origin();
  return debug_string.str();
}

template <>
std::string DataKeyDebugStringVisitor::operator()<
    content::PrivateAggregationDataModel::DataKey>(
    const content::PrivateAggregationDataModel::DataKey&
        private_aggregation_data_key) {
  std::stringstream debug_string;
  debug_string << "PrivateAggregationDataKey: ";
  debug_string << private_aggregation_data_key.reporting_origin();
  return debug_string.str();
}

template <>
std::string
DataKeyDebugStringVisitor::operator()<net::SharedDictionaryIsolationKey>(
    const net::SharedDictionaryIsolationKey& shared_dictionary_isolation_key) {
  std::stringstream debug_string;
  debug_string << "SharedDictionaryIsolationKey: ";
  debug_string << "{frame_origin: ";
  debug_string << shared_dictionary_isolation_key.frame_origin().Serialize();
  debug_string << " top_frame_site: ";
  debug_string
      << shared_dictionary_isolation_key.top_frame_site().GetDebugString()
      << "}";
  return debug_string.str();
}

template <>
std::string
DataKeyDebugStringVisitor::operator()<browsing_data::SharedWorkerInfo>(
    const browsing_data::SharedWorkerInfo& shared_worker_info) {
  std::stringstream debug_string;
  debug_string << "SharedWorkerInfo: ";
  debug_string << "{worker: " << shared_worker_info.worker;
  debug_string << " name: " << shared_worker_info.name;
  debug_string << " blink::StorageKey: ";
  debug_string << shared_worker_info.storage_key.GetDebugString() << "}";
  return debug_string.str();
}

template <>
std::string
DataKeyDebugStringVisitor::operator()<content::SessionStorageUsageInfo>(
    const content::SessionStorageUsageInfo& session_storage_usage_info) {
  std::stringstream debug_string;
  debug_string << "SessionStorageUsageInfo: ";
  debug_string << "{namespace_id: " << session_storage_usage_info.namespace_id;
  debug_string << " blink::StorageKey: ";
  debug_string << session_storage_usage_info.storage_key.GetDebugString()
               << "}";
  return debug_string.str();
}

template <>
std::string DataKeyDebugStringVisitor::operator()<net::CanonicalCookie>(
    const net::CanonicalCookie& cookie) {
  std::stringstream debug_string;
  debug_string << "CanonicalCookie: {" << cookie.DebugString();
  debug_string << " Partitioned: " << cookie.IsPartitioned();
  if (cookie.IsPartitioned()) {
    debug_string << " Partitioning site: "
                 << cookie.PartitionKey()->site().Serialize();
  }
  debug_string << "}";
  return debug_string.str();
}

template <>
std::string DataKeyDebugStringVisitor::operator()<
    webid::FederatedIdentityDataModel::DataKey>(
    const webid::FederatedIdentityDataModel::DataKey&
        federated_identity_data_key) {
  std::stringstream debug_string;
  debug_string << "FederatedIdentityDataKey: ";
  debug_string << "{relying_party_requester: "
               << federated_identity_data_key.relying_party_requester();
  debug_string << " relying_party_embedder: "
               << federated_identity_data_key.relying_party_embedder();
  debug_string << " identity_provider: "
               << federated_identity_data_key.identity_provider();
  debug_string << " account_id: " << federated_identity_data_key.account_id()
               << "}";
  return debug_string.str();
}

struct DataOwnerDebugStringVisitor {
  template <class T>
  std::string operator()(const T& data_owner);
};

template <>
std::string DataOwnerDebugStringVisitor::operator()<std::string>(
    const std::string& host) {
  return host;
}

template <>
std::string DataOwnerDebugStringVisitor::operator()<url::Origin>(
    const url::Origin& origin) {
  return origin.Serialize();
}

}  // namespace

BrowsingDataEntry::BrowsingDataEntry(
    const BrowsingDataModel::DataOwner& data_owner,
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::DataDetails data_details)
    : data_owner(data_owner), data_key(data_key), data_details(data_details) {}

BrowsingDataEntry::BrowsingDataEntry(
    const BrowsingDataModel::BrowsingDataEntryView& view)
    : data_owner(*view.data_owner),
      data_key(*view.data_key),
      data_details(*view.data_details) {}

BrowsingDataEntry::~BrowsingDataEntry() = default;
BrowsingDataEntry::BrowsingDataEntry(const BrowsingDataEntry& other) = default;

bool BrowsingDataEntry::operator==(const BrowsingDataEntry& other) const {
  return data_owner == other.data_owner && data_key == other.data_key &&
         data_details == other.data_details;
}

std::string BrowsingDataEntry::ToDebugString() const {
  std::stringstream debug_string;
  debug_string << "Data Owner: ";
  debug_string << absl::visit(DataOwnerDebugStringVisitor(), data_owner);

  debug_string << " Data Key: ";
  debug_string << absl::visit(DataKeyDebugStringVisitor(), data_key);

  debug_string << " Storage Types: ";
  debug_string << data_details.storage_types.ToEnumBitmask();

  debug_string << " Storage Size: ";
  debug_string << data_details.storage_size;

  debug_string << " Cookie count: ";
  debug_string << data_details.cookie_count;

  return debug_string.str();
}

void ValidateBrowsingDataEntries(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;

  for (const auto& entry : *model) {
    model_entries.emplace_back(entry);
  }

  std::string model_entries_debug_string = "[";
  for (const auto& entry : model_entries) {
    model_entries_debug_string += "\n{";
    model_entries_debug_string += entry.ToDebugString();
    model_entries_debug_string += "},";
  }
  model_entries_debug_string += "]";

  std::string expected_entries_debug_string = "[";
  for (const auto& entry : expected_entries) {
    expected_entries_debug_string += "\n{";
    expected_entries_debug_string += entry.ToDebugString();
    expected_entries_debug_string += "},";
  }
  expected_entries_debug_string += "]";

  SCOPED_TRACE("\nModel Entries:\n" + model_entries_debug_string +
               "\nExpected Entries:\n" + expected_entries_debug_string);

  EXPECT_THAT(model_entries,
              testing::UnorderedElementsAreArray(expected_entries));
}

void ValidateBrowsingDataEntriesNonZeroUsage(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;
  for (const auto& entry : *model) {
    model_entries.emplace_back(entry);
  }
  EXPECT_EQ(model_entries.size(), expected_entries.size());

  for (size_t i = 0; i < expected_entries.size(); i++) {
    EXPECT_EQ(expected_entries[i].data_owner, model_entries[i].data_owner);
    EXPECT_EQ(expected_entries[i].data_key, model_entries[i].data_key);
    EXPECT_EQ(expected_entries[i].data_details.storage_types,
              model_entries[i].data_details.storage_types);
    EXPECT_EQ(expected_entries[i].data_details.cookie_count,
              model_entries[i].data_details.cookie_count);
    EXPECT_TRUE(model_entries[i].data_details.storage_size > 0);
  }
}
}  // namespace browsing_data_model_test_util
