// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/containers/enum_set.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {

// A number of bytes used to represent data which takes up a practically
// imperceptible, but non-0 amount of space, such as Trust Tokens.
constexpr int kSmallAmountOfDataInBytes = 100;

// An estimate of storage size of an Interest Group object.
constexpr int kModerateAmountOfDataInBytes = 1024;

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

  if (storage_type_ == BrowsingDataModel::StorageType::kSharedStorage) {
    return data_key.origin().host();
  }
  NOTREACHED();
  return "";
}

template <>
std::string
GetPrimaryHost::operator()<content::InterestGroupManager::InterestGroupDataKey>(
    const content::InterestGroupManager::InterestGroupDataKey& data_key) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kInterestGroup, storage_type_);
  return data_key.owner.host();
}

// Helper which allows the lifetime management of a deletion action to occur
// separately from the BrowsingDataModel itself.
struct StorageRemoverHelper {
  explicit StorageRemoverHelper(
      content::StoragePartition* storage_partition
      // TODO(crbug.com/1271155): Inject other dependencies.
      )
      : storage_partition_(storage_partition) {}

  void RemoveByPrimaryHost(
      const std::string& primary_host,
      const BrowsingDataModel::DataKeyEntries& data_key_entries,
      base::OnceClosure completed);

 private:
  // Visitor struct to hold information used for deletion. absl::visit doesn't
  // support multiple arguments elegently.
  struct Visitor {
    raw_ptr<StorageRemoverHelper> helper;
    BrowsingDataModel::StorageTypeSet types;

    template <class T>
    void operator()(const T& data_key);
  };

  // Returns a OnceClosure which can be passed to a storage backend for calling
  // on deletion completion.
  base::OnceClosure GetCompleteCallback();

  void BackendFinished();

  bool removing_ = false;
  base::OnceClosure completed_;
  size_t callbacks_expected_ = 0;
  size_t callbacks_seen_ = 0;

  raw_ptr<content::StoragePartition> storage_partition_;
  base::WeakPtrFactory<StorageRemoverHelper> weak_ptr_factory_{this};
};

void StorageRemoverHelper::RemoveByPrimaryHost(
    const std::string& primary_host,
    const BrowsingDataModel::DataKeyEntries& data_key_entries,
    base::OnceClosure completed) {
  // At a helper level, only a single deletion may occur at a time. However
  // multiple helpers may be associated with a single model.
  DCHECK(!removing_);
  removing_ = true;

  completed_ = std::move(completed);

  // Creating a synchronous callback to hold off running `completed_` callback
  // until the loop has completed visiting all its entries whether deletion is
  // synchronous or asynchronous.
  auto sync_completion = GetCompleteCallback();
  for (const auto& [key, details] : data_key_entries)
    absl::visit(Visitor{this, details.storage_types}, key);

  std::move(sync_completion).Run();
}

template <>
void StorageRemoverHelper::Visitor::operator()<url::Origin>(
    const url::Origin& origin) {
  if (types.Has(BrowsingDataModel::StorageType::kTrustTokens)) {
    helper->storage_partition_->GetNetworkContext()->DeleteStoredTrustTokens(
        origin, base::BindOnce(
                    [](base::OnceClosure complete_callback,
                       ::network::mojom::DeleteStoredTrustTokensStatus status) {
                      std::move(complete_callback).Run();
                    },
                    helper->GetCompleteCallback()));
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<blink::StorageKey>(
    const blink::StorageKey& storage_key) {
  if (types.Has(BrowsingDataModel::StorageType::kSharedStorage)) {
    helper->storage_partition_->GetSharedStorageManager()->Clear(
        storage_key.origin(),
        base::BindOnce(
            [](base::OnceClosure complete_callback,
               storage::SharedStorageDatabase::OperationResult result) {
              std::move(complete_callback).Run();
            },
            helper->GetCompleteCallback()));

  } else {
    // TODO(crbug.com/1271155): Implement for quota managed storage.
    NOTREACHED();
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    content::InterestGroupManager::InterestGroupDataKey>(
    const content::InterestGroupManager::InterestGroupDataKey& data_key) {
  if (types.Has(BrowsingDataModel::StorageType::kInterestGroup)) {
    helper->storage_partition_->GetInterestGroupManager()
        ->RemoveInterestGroupsByDataKey(
            data_key, base::BindOnce(
                          [](base::OnceClosure complete_callback) {
                            std::move(complete_callback).Run();
                          },
                          helper->GetCompleteCallback()));
  } else {
    NOTREACHED();
  }
}

base::OnceClosure StorageRemoverHelper::GetCompleteCallback() {
  callbacks_expected_++;
  return base::BindOnce(&StorageRemoverHelper::BackendFinished,
                        weak_ptr_factory_.GetWeakPtr());
}

void StorageRemoverHelper::BackendFinished() {
  DCHECK(callbacks_expected_ > callbacks_seen_);
  callbacks_seen_++;

  if (callbacks_seen_ == callbacks_expected_)
    std::move(completed_).Run();
}

void OnTrustTokenIssuanceInfoLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::vector<::network::mojom::StoredTrustTokensForIssuerPtr> tokens) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& token : tokens) {
    if (token->count == 0)
      continue;

    model->AddBrowsingData(token->issuer,
                           BrowsingDataModel::StorageType::kTrustTokens,
                           kSmallAmountOfDataInBytes);
  }
  std::move(loaded_callback).Run();
}

void OnSharedStorageLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::vector<::storage::mojom::StorageUsageInfoPtr> storage_usage_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& info : storage_usage_info) {
    model->AddBrowsingData(info->storage_key,
                           BrowsingDataModel::StorageType::kSharedStorage,
                           info->total_size_bytes);
  }
  std::move(loaded_callback).Run();
}

void OnInterestGroupsLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::vector<content::InterestGroupManager::InterestGroupDataKey>
        interest_groups) {
  for (const auto& data_key : interest_groups) {
    model->AddBrowsingData(data_key,
                           BrowsingDataModel::StorageType::kInterestGroup,
                           kModerateAmountOfDataInBytes);
  }
  std::move(loaded_callback).Run();
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
    content::BrowserContext* browser_context,
    base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
        complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto model = BuildEmpty(browser_context);
  auto* model_pointer = model.get();

  // This functor will own the unique_ptr for the model during construction,
  // after which it hands it to the initial caller. This ownership semantic
  // ensures that raw `this` pointers provided to backends for fetching remain
  // valid.
  base::OnceClosure completion = base::BindOnce(
      [](std::unique_ptr<BrowsingDataModel> model,
         base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
             callback) { std::move(callback).Run(std::move(model)); },
      std::move(model), std::move(complete_callback));

  model_pointer->PopulateFromDisk(std::move(completion));
}

std::unique_ptr<BrowsingDataModel> BrowsingDataModel::BuildEmpty(
    content::BrowserContext* browser_context) {
  return base::WrapUnique(new BrowsingDataModel(
      browser_context->GetDefaultStoragePartition()));  // Private constructor
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Bind the lifetime of the helper to the lifetime of the callback.
  auto helper = std::make_unique<StorageRemoverHelper>(storage_partition_);
  auto* helper_pointer = helper.get();

  base::OnceClosure wrapped_completed = base::BindOnce(
      [](std::unique_ptr<StorageRemoverHelper> storage_remover,
         base::OnceClosure completed) { std::move(completed).Run(); },
      std::move(helper), std::move(completed));

  helper_pointer->RemoveByPrimaryHost(primary_host,
                                      browsing_data_entries_[primary_host],
                                      std::move(wrapped_completed));

  // Immediately remove the affected entries from the in-memory model. Different
  // UI elements have different sync vs. async expectations. Exposing a
  // completed callback, but updating the model synchronously, serves both.
  browsing_data_entries_.erase(primary_host);
}

void BrowsingDataModel::PopulateFromDisk(base::OnceClosure finished_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool is_shared_storage_enabled =
      base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI);
  bool is_interest_group_enabled =
      base::FeatureList::IsEnabled(blink::features::kAdInterestGroupAPI);
  // TODO(crbug.com/1271155): Derive this from the StorageTypeSet directly.
  int storage_backend_count = 1;
  if (is_shared_storage_enabled)
    storage_backend_count++;
  if (is_interest_group_enabled)
    storage_backend_count++;

  base::RepeatingClosure completion =
      base::BarrierClosure(storage_backend_count, std::move(finished_callback));

  // The public build interfaces for the model ensure that `this` remains valid
  // until `finished_callback` has been run. Thus, it's safe to pass raw `this`
  // to backend callbacks.

  // Issued Trust Tokens:
  storage_partition_->GetNetworkContext()->GetStoredTrustTokenCounts(
      base::BindOnce(&OnTrustTokenIssuanceInfoLoaded, this, completion));

  // Shared storage origins
  if (is_shared_storage_enabled) {
    storage_partition_->GetSharedStorageManager()->FetchOrigins(
        base::BindOnce(&OnSharedStorageLoaded, this, completion));
  }

  // Interest Groups
  if (is_interest_group_enabled) {
    storage_partition_->GetInterestGroupManager()->GetAllInterestGroupDataKeys(
        base::BindOnce(&OnInterestGroupsLoaded, this, completion));
  }
}

BrowsingDataModel::BrowsingDataModel(
    content::StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {}
