// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include "base/callback.h"
#include "base/containers/enum_set.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {

// Visitor which returns the appropriate primary host for a given `data_key`
// and `storage_type`.
struct GetPrimaryHost {
  explicit GetPrimaryHost(BrowsingDataModel::StorageType storage_type)
      : storage_type_(storage_type) {}

  template <class T>
  std::string operator()(const T& data_key) const;

 private:
  BrowsingDataModel::StorageType storage_type_;
};

template <>
std::string GetPrimaryHost::operator()<url::Origin>(
    const url::Origin& data_key) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kTrustTokens, storage_type_);
  return data_key.host();
}

template <>
std::string GetPrimaryHost::operator()<blink::StorageKey>(
    const blink::StorageKey& data_key) const {
  // TODO(crbug.com/1271155): This logic is useful for testing during the
  // implementation of the model, but ultimately these storage types may not
  // coexist.
  if (storage_type_ == BrowsingDataModel::StorageType::kPartitionedQuotaStorage)
    return data_key.top_level_site().GetURL().host();

  if (storage_type_ ==
      BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage) {
    return data_key.origin().host();
  }

  NOTREACHED();
  return "";
}

}  // namespace

BrowsingDataModel::DataDetails::~DataDetails() = default;
bool BrowsingDataModel::DataDetails::operator==(
    const DataDetails& other) const {
  return storage_types == other.storage_types &&
         storage_size == other.storage_size &&
         cookie_count == other.cookie_count;
}

BrowsingDataModel::BrowsingDataEntryView::BrowsingDataEntryView(
    const std::string& primary_host,
    const DataKey& data_key,
    const DataDetails& data_details)
    : primary_host(primary_host),
      data_key(data_key),
      data_details(data_details) {}
BrowsingDataModel::BrowsingDataEntryView::~BrowsingDataEntryView() = default;

BrowsingDataModel::Iterator::Iterator(const Iterator& iterator) = default;
BrowsingDataModel::Iterator::~Iterator() = default;

bool BrowsingDataModel::Iterator::operator==(const Iterator& other) const {
  if (outer_iterator_ == outer_end_iterator_ &&
      other.outer_iterator_ == other.outer_end_iterator_) {
    // Special case the == .end() scenario, because the inner iterators may
    // not have been set.
    return outer_iterator_ == other.outer_iterator_;
  }
  return outer_iterator_ == other.outer_iterator_ &&
         inner_iterator_ == other.inner_iterator_;
}

bool BrowsingDataModel::Iterator::operator!=(const Iterator& other) const {
  return !operator==(other);
}

BrowsingDataModel::BrowsingDataEntryView
BrowsingDataModel::Iterator::operator*() const {
  DCHECK(outer_iterator_ != outer_end_iterator_);
  DCHECK(inner_iterator_ != outer_iterator_->second.end());
  return BrowsingDataEntryView(outer_iterator_->first, inner_iterator_->first,
                               inner_iterator_->second);
}

BrowsingDataModel::Iterator& BrowsingDataModel::Iterator::operator++() {
  inner_iterator_++;
  if (inner_iterator_ == outer_iterator_->second.end()) {
    outer_iterator_++;
    if (outer_iterator_ != outer_end_iterator_)
      inner_iterator_ = outer_iterator_->second.begin();
  }
  return *this;
}

BrowsingDataModel::Iterator::Iterator(
    BrowsingDataEntries::const_iterator outer_iterator,
    BrowsingDataEntries::const_iterator outer_end_iterator)
    : outer_iterator_(outer_iterator), outer_end_iterator_(outer_end_iterator) {
  if (outer_iterator_ != outer_end_iterator_) {
    inner_iterator_ = outer_iterator_->second.begin();
  }
}

BrowsingDataModel::Iterator BrowsingDataModel::begin() const {
  return Iterator(browsing_data_entries_.begin(), browsing_data_entries_.end());
}

BrowsingDataModel::Iterator BrowsingDataModel::end() const {
  return Iterator(browsing_data_entries_.end(), browsing_data_entries_.end());
}

BrowsingDataModel::~BrowsingDataModel() = default;

void BrowsingDataModel::BuildFromDisk(
    content::BrowserContext* browsing_context,
    base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
        complete_callback) {
  // TODO(crbug.com/1271155): Implement.
  NOTREACHED();
}

std::unique_ptr<BrowsingDataModel> BrowsingDataModel::BuildEmpty() {
  return base::WrapUnique(new BrowsingDataModel());  // Private constructor
}

void BrowsingDataModel::AddBrowsingData(const DataKey& data_key,
                                        StorageType storage_type,
                                        uint64_t storage_size,
                                        uint64_t cookie_count) {
  std::string primary_host =
      absl::visit(GetPrimaryHost(storage_type), data_key);

  // Find the existing entry if it exists, constructing any missing components.
  auto& entry = browsing_data_entries_[primary_host][data_key];

  entry.storage_size += storage_size;
  entry.cookie_count += cookie_count;
  entry.storage_types.Put(storage_type);
}

void BrowsingDataModel::RemoveBrowsingData(const std::string& primary_host,
                                           base::OnceClosure completed) {
  // TODO(crbug.com/1271155): Implement.
  NOTREACHED();
}

BrowsingDataModel::BrowsingDataModel() = default;
