// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include <set>
#include <string>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "components/attribution_reporting/features.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"
#include "components/browsing_data/content/shared_worker_info.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace {

// A number of bytes used to represent data which takes up a practically
// imperceptible, but non-0 amount of space, such as Trust Tokens.
constexpr int kSmallAmountOfDataInBytes = 100;

// An estimate of storage size of an Interest Group object.
constexpr int kModerateAmountOfDataInBytes = 1024;

// Visitor which returns the appropriate data owner for a given `data_key`
// and `storage_type`.
struct GetDataOwner {
  GetDataOwner(BrowsingDataModel::Delegate* delegate,
               BrowsingDataModel::StorageType storage_type)
      : delegate_(delegate), storage_type_(storage_type) {}

  template <class T>
  BrowsingDataModel::DataOwner operator()(const T& data_key) const {
    if (delegate_) {
      std::optional<BrowsingDataModel::DataOwner> owner =
          delegate_->GetDataOwner(data_key, storage_type_);
      if (owner.has_value()) {
        return *owner;
      }
    }

    return GetOwningOriginOrHost(data_key);
  }

 private:
  template <class T>
  BrowsingDataModel::DataOwner GetOwningOriginOrHost(const T& data_key) const;

  // Returns the origin's host if the URL scheme is `http` or `https` otherwise
  // returns the origin.
  BrowsingDataModel::DataOwner GetOwnerBasedOnScheme(
      const url::Origin origin) const {
    if (origin.GetURL().SchemeIsHTTPOrHTTPS()) {
      return origin.host();
    }
    return origin;
  }

  raw_ptr<BrowsingDataModel::Delegate> delegate_;
  BrowsingDataModel::StorageType storage_type_;
};

template <>
BrowsingDataModel::DataOwner GetDataOwner::GetOwningOriginOrHost<url::Origin>(
    const url::Origin& data_key) const {
  if (storage_type_ == BrowsingDataModel::StorageType::kTrustTokens) {
    return GetOwnerBasedOnScheme(data_key);
  }

  NOTREACHED_IN_MIGRATION()
      << "Unexpected StorageType: " << static_cast<int>(storage_type_);
  return "";
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<blink::StorageKey>(
    const blink::StorageKey& data_key) const {
  // TODO(crbug.com/40205603): This logic is useful for testing during the
  // implementation of the model, but ultimately these storage types may not
  // coexist.
  switch (storage_type_) {
    case BrowsingDataModel::StorageType::kQuotaStorage:
    case BrowsingDataModel::StorageType::kSharedStorage:
    case BrowsingDataModel::StorageType::kLocalStorage:
    case BrowsingDataModel::StorageType::kCdmStorage:
      return GetOwnerBasedOnScheme(data_key.origin());
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected StorageType: " << static_cast<int>(storage_type_);
      return "";
  }
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<content::SessionStorageUsageInfo>(
    const content::SessionStorageUsageInfo& session_storage_usage_info) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kSessionStorage, storage_type_);
  return GetOwnerBasedOnScheme(session_storage_usage_info.storage_key.origin());
}

template <>
BrowsingDataModel::DataOwner GetDataOwner::GetOwningOriginOrHost<
    content::InterestGroupManager::InterestGroupDataKey>(
    const content::InterestGroupManager::InterestGroupDataKey& data_key) const {
  CHECK_EQ(BrowsingDataModel::StorageType::kInterestGroup, storage_type_);
  return GetOwnerBasedOnScheme(data_key.owner);
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<content::AttributionDataModel::DataKey>(
    const content::AttributionDataModel::DataKey& data_key) const {
  CHECK_EQ(BrowsingDataModel::StorageType::kAttributionReporting,
           storage_type_);
  return GetOwnerBasedOnScheme(data_key.reporting_origin());
}

template <>
BrowsingDataModel::DataOwner GetDataOwner::GetOwningOriginOrHost<
    content::PrivateAggregationDataModel::DataKey>(
    const content::PrivateAggregationDataModel::DataKey& data_key) const {
  CHECK_EQ(BrowsingDataModel::StorageType::kPrivateAggregation, storage_type_);
  return GetOwnerBasedOnScheme(data_key.reporting_origin());
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<net::SharedDictionaryIsolationKey>(
    const net::SharedDictionaryIsolationKey& isolation_key) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kSharedDictionary, storage_type_);
  return GetOwnerBasedOnScheme(isolation_key.frame_origin());
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<browsing_data::SharedWorkerInfo>(
    const browsing_data::SharedWorkerInfo& shared_worker_info) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kSharedWorker, storage_type_);
  return GetOwnerBasedOnScheme(shared_worker_info.storage_key.origin());
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<net::CanonicalCookie>(
    const net::CanonicalCookie& cookie) const {
  DCHECK_EQ(BrowsingDataModel::StorageType::kCookie, storage_type_);
  return cookie.DomainWithoutDot();
}

template <>
BrowsingDataModel::DataOwner
GetDataOwner::GetOwningOriginOrHost<webid::FederatedIdentityDataModel::DataKey>(
    const webid::FederatedIdentityDataModel::DataKey& data_key) const {
  // Getting owning origin or host also handled by GetDataOwner in
  // ChromeBrowsingDataModelDelegate.
  return GetOwnerBasedOnScheme(data_key.relying_party_embedder());
}

// Helper which allows the lifetime management of a deletion action to occur
// separately from the BrowsingDataModel itself.
struct StorageRemoverHelper {
  explicit StorageRemoverHelper(
      content::StoragePartition* storage_partition,
      scoped_refptr<BrowsingDataQuotaHelper> quota_helper,
      BrowsingDataModel::Delegate* delegate
      // TODO(crbug.com/40205603): Inject other dependencies.
      )
      : storage_partition_(storage_partition),
        quota_helper_(quota_helper),
        delegate_(delegate ? delegate->AsWeakPtr() : nullptr) {}

  void RemoveDataKeyEntries(
      const BrowsingDataModel::DataKeyEntries& data_key_entries,
      base::OnceClosure completed);

 private:
  // Visitor struct to hold information used for deletion. absl::visit doesn't
  // support multiple arguments elegantly.
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
  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;
  base::WeakPtr<BrowsingDataModel::Delegate> delegate_;
  base::WeakPtrFactory<StorageRemoverHelper> weak_ptr_factory_{this};
};

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
  }

  if (types.Has(BrowsingDataModel::StorageType::kQuotaStorage)) {
    const blink::mojom::StorageType quota_types[] = {
        blink::mojom::StorageType::kTemporary,
        blink::mojom::StorageType::kSyncable};
    for (auto type : quota_types) {
      helper->quota_helper_->DeleteStorageKeyData(
          storage_key, type, helper->GetCompleteCallback());
    }
  }

  if (types.Has(BrowsingDataModel::StorageType::kLocalStorage)) {
    helper->storage_partition_->GetDOMStorageContext()->DeleteLocalStorage(
        storage_key, helper->GetCompleteCallback());
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (types.Has(BrowsingDataModel::StorageType::kCdmStorage)) {
    helper->storage_partition_->GetCdmStorageDataModel()
        ->DeleteDataForStorageKey(storage_key,
                                  base::BindOnce(base::IgnoreArgs<bool>(
                                      helper->GetCompleteCallback())));
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    content::SessionStorageUsageInfo>(
    const content::SessionStorageUsageInfo& session_storage_usage_info) {
  if (types.Has(BrowsingDataModel::StorageType::kSessionStorage)) {
    helper->storage_partition_->GetDOMStorageContext()->DeleteSessionStorage(
        session_storage_usage_info, helper->GetCompleteCallback());
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<browsing_data::SharedWorkerInfo>(
    const browsing_data::SharedWorkerInfo& shared_worker_info) {
  if (types.Has(BrowsingDataModel::StorageType::kSharedWorker)) {
    helper->storage_partition_->GetSharedWorkerService()->TerminateWorker(
        shared_worker_info.worker, shared_worker_info.name,
        shared_worker_info.storage_key, shared_worker_info.same_site_cookies);
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    content::InterestGroupManager::InterestGroupDataKey>(
    const content::InterestGroupManager::InterestGroupDataKey& data_key) {
  CHECK(types.Has(BrowsingDataModel::StorageType::kInterestGroup));
  helper->storage_partition_->GetInterestGroupManager()
      ->RemoveInterestGroupsByDataKey(
          data_key, base::BindOnce(
                        [](base::OnceClosure complete_callback) {
                          std::move(complete_callback).Run();
                        },
                        helper->GetCompleteCallback()));
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    content::AttributionDataModel::DataKey>(
    const content::AttributionDataModel::DataKey& data_key) {
  CHECK(types.Has(BrowsingDataModel::StorageType::kAttributionReporting));
  helper->storage_partition_->GetAttributionDataModel()
      ->RemoveAttributionDataByDataKey(data_key, helper->GetCompleteCallback());
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    content::PrivateAggregationDataModel::DataKey>(
    const content::PrivateAggregationDataModel::DataKey& data_key) {
  CHECK(types.Has(BrowsingDataModel::StorageType::kPrivateAggregation));
  helper->storage_partition_->GetPrivateAggregationDataModel()
      ->RemovePendingDataKey(data_key, helper->GetCompleteCallback());
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    net::SharedDictionaryIsolationKey>(
    const net::SharedDictionaryIsolationKey& isolation_key) {
  if (types.Has(BrowsingDataModel::StorageType::kSharedDictionary)) {
    helper->storage_partition_->GetNetworkContext()
        ->ClearSharedDictionaryCacheForIsolationKey(
            isolation_key, helper->GetCompleteCallback());
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<net::CanonicalCookie>(
    const net::CanonicalCookie& cookie) {
  if (types.Has(BrowsingDataModel::StorageType::kCookie)) {
    if (helper->delegate_ &&
        helper->delegate_->IsCookieDeletionDisabled(
            net::cookie_util::CookieOriginToURL(cookie.Domain(),
                                                cookie.SecureAttribute()))) {
      // TODO(crbug.com/40940636): Expand test coverage for this block.
      return;
    }
    helper->storage_partition_->GetCookieManagerForBrowserProcess()
        ->DeleteCanonicalCookie(
            cookie,
            base::BindOnce([](base::OnceClosure callback,
                              bool deleted) { std::move(callback).Run(); },
                           helper->GetCompleteCallback()));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

template <>
void StorageRemoverHelper::Visitor::operator()<
    webid::FederatedIdentityDataModel::DataKey>(
    const webid::FederatedIdentityDataModel::DataKey& data_key) {
  // Storage removal handled by RemoveDataKey in
  // ChromeBrowsingDataModelDelegate.
}

void StorageRemoverHelper::RemoveDataKeyEntries(
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
  for (const auto& [key, details] : data_key_entries) {
    absl::visit(Visitor{this, details.storage_types}, key);
    if (delegate_) {
      delegate_->RemoveDataKey(key, details.storage_types,
                               GetCompleteCallback());
    }
  }

  std::move(sync_completion).Run();
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

// Only websafe state is considered browsing data.
bool HasStorageScheme(const url::Origin& origin) {
  return base::Contains(url::GetWebStorageSchemes(), origin.scheme());
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

void OnAttributionReportingLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::set<content::AttributionDataModel::DataKey> attribution_reporting) {
  for (const auto& data_key : attribution_reporting) {
    model->AddBrowsingData(
        data_key, BrowsingDataModel::StorageType::kAttributionReporting,
        kSmallAmountOfDataInBytes);
  }
  std::move(loaded_callback).Run();
}

void OnPrivateAggregationLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::set<content::PrivateAggregationDataModel::DataKey>
        private_aggregation) {
  for (const auto& data_key : private_aggregation) {
    model->AddBrowsingData(data_key,
                           BrowsingDataModel::StorageType::kPrivateAggregation,
                           kSmallAmountOfDataInBytes);
  }
  std::move(loaded_callback).Run();
}

void OnQuotaStorageLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    const std::list<BrowsingDataQuotaHelper::QuotaInfo>& quota_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& entry : quota_info) {
    model->AddBrowsingData(entry.storage_key,
                           BrowsingDataModel::StorageType::kQuotaStorage,
                           entry.syncable_usage + entry.temporary_usage);
  }
  std::move(loaded_callback).Run();
}

void OnLocalStorageLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    const std::vector<content::StorageUsageInfo>& storage_usage_info) {
  for (const auto& info : storage_usage_info) {
    if (HasStorageScheme(info.storage_key.origin())) {
      model->AddBrowsingData(info.storage_key,
                             BrowsingDataModel::StorageType::kLocalStorage,
                             info.total_size_bytes);
    }
  }
  std::move(loaded_callback).Run();
}

void OnSharedDictionaryUsageLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    const std::vector<net::SharedDictionaryUsageInfo>& usage_info) {
  for (const auto& info : usage_info) {
    model->AddBrowsingData(info.isolation_key,
                           BrowsingDataModel::StorageType::kSharedDictionary,
                           info.total_size_bytes);
  }
  std::move(loaded_callback).Run();
}

void OnCookiesLoaded(BrowsingDataModel* model,
                     base::OnceClosure loaded_callback,
                     const net::CookieList& cookie_list) {
  for (const auto& cookie : cookie_list) {
    model->AddBrowsingData(cookie, BrowsingDataModel::StorageType::kCookie, 0,
                           1);
  }
  std::move(loaded_callback).Run();
}

void OnDelegateDataLoaded(
    BrowsingDataModel* model,
    base::OnceClosure loaded_callback,
    std::vector<BrowsingDataModel::Delegate::DelegateEntry> delegated_entries) {
  for (const auto& entry : delegated_entries) {
    model->AddBrowsingData(entry.data_key, entry.storage_type,
                           entry.storage_size);
  }
  std::move(loaded_callback).Run();
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
void OnCdmStorageLoaded(BrowsingDataModel* model,
                        base::OnceClosure loaded_callback,
                        const CdmStorageKeyUsageSize& usage_for_storage_keys) {
  for (const auto& [storage_key, usage] : usage_for_storage_keys) {
    model->AddBrowsingData(storage_key,
                           BrowsingDataModel::StorageType::kCdmStorage, usage);
  }
  std::move(loaded_callback).Run();
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

// If `data_key` represents a non-1P partition, returns the site on which it
// is partitioned, std::nullopt otherwise.
std::optional<net::SchemefulSite> GetThirdPartyPartitioningSite(
    const BrowsingDataModel::DataKey& data_key) {
  std::optional<net::SchemefulSite> top_level_site = std::nullopt;
  absl::visit(
      base::Overloaded{
          [&](const url::Origin&) {},
          [&](const content::InterestGroupManager::InterestGroupDataKey) {},
          [&](const content::AttributionDataModel::DataKey) {},
          [&](const content::PrivateAggregationDataModel::DataKey) {},
          [&](const blink::StorageKey& storage_key) {
            if (storage_key.IsThirdPartyContext()) {
              top_level_site = storage_key.top_level_site();
            }
          },
          [&](const content::SessionStorageUsageInfo& info) {
            if (info.storage_key.IsThirdPartyContext()) {
              top_level_site = info.storage_key.top_level_site();
            }
          },
          [&](const browsing_data::SharedWorkerInfo& info) {
            if (info.storage_key.IsThirdPartyContext()) {
              top_level_site = info.storage_key.top_level_site();
            }
          },
          [&](const net::SharedDictionaryIsolationKey& key) {
            if (net::SchemefulSite(key.frame_origin()) !=
                key.top_frame_site()) {
              top_level_site = key.top_frame_site();
            }
          },
          [&](const net::CanonicalCookie& cookie) {
            if (cookie.IsThirdPartyPartitioned()) {
              top_level_site = cookie.PartitionKey()->site();
            }
          },
          [&](const webid::FederatedIdentityDataModel::DataKey& data_key) {
            if (data_key.relying_party_requester() !=
                data_key.relying_party_embedder()) {
              top_level_site =
                  net::SchemefulSite(data_key.relying_party_embedder());
            }
          },
      },
      data_key);

  return top_level_site;
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
    const DataOwner& data_owner,
    const DataKey& data_key,
    const DataDetails& data_details)
    : data_owner(data_owner), data_key(data_key), data_details(data_details) {}
BrowsingDataModel::BrowsingDataEntryView::~BrowsingDataEntryView() = default;

// static
const std::string BrowsingDataModel::GetHost(const DataOwner& data_owner) {
  return absl::visit(
      base::Overloaded{
          [&](const std::string& host) { return host; },
          [&](const url::Origin& origin) { return origin.host(); }},
      data_owner);
}

const url::Origin BrowsingDataModel::GetOriginForDataKey(
    const BrowsingDataModel::DataKey& data_key) {
  return absl::visit(
      base::Overloaded{
          [](const url::Origin& origin) { return origin; },
          [](const content::InterestGroupManager::InterestGroupDataKey
                 interest_group_key) { return interest_group_key.owner; },
          [](const content::AttributionDataModel::DataKey
                 attribution_reporting_key) {
            return attribution_reporting_key.reporting_origin();
          },
          [](const content::PrivateAggregationDataModel::DataKey
                 private_aggregation_key) {
            return private_aggregation_key.reporting_origin();
          },
          [](const blink::StorageKey& storage_key) {
            return storage_key.origin();
          },
          [](const content::SessionStorageUsageInfo& info) {
            return info.storage_key.origin();
          },
          [](const browsing_data::SharedWorkerInfo& info) {
            return info.storage_key.origin();
          },
          [](const net::SharedDictionaryIsolationKey& key) {
            return key.frame_origin();
          },
          [](const net::CanonicalCookie& cookie) {
            GURL cookie_url = net::cookie_util::CookieOriginToURL(
                cookie.Domain(), browsing_data::IsHttpsCookieSourceScheme(
                                     cookie.SourceScheme()));
            return url::Origin::Create(cookie_url);
          },
          [](const webid::FederatedIdentityDataModel::DataKey& data_key) {
            return data_key.relying_party_embedder();
          },
      },
      data_key);
}

bool BrowsingDataModel::BrowsingDataEntryView::Matches(
    const url::Origin& origin) const {
  return absl::visit(base::Overloaded{[&](const std::string& entry_host) {
                                        return entry_host == origin.host();
                                      },
                                      [&](const url::Origin& entry_origin) {
                                        return entry_origin == origin;
                                      }},
                     *data_owner);
}

std::optional<net::SchemefulSite>
BrowsingDataModel::BrowsingDataEntryView::GetThirdPartyPartitioningSite()
    const {
  // Partition information is only dependent on it's `data_key`.
  return ::GetThirdPartyPartitioningSite(data_key.get());
}

BrowsingDataModel::Delegate::DelegateEntry::DelegateEntry(
    const DataKey& data_key,
    StorageType storage_type,
    uint64_t storage_size)
    : data_key(data_key),
      storage_type(storage_type),
      storage_size(storage_size) {}
BrowsingDataModel::Delegate::DelegateEntry::DelegateEntry(
    const DelegateEntry& other) = default;
BrowsingDataModel::Delegate::DelegateEntry::~DelegateEntry() = default;

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
  CHECK(inner_iterator_ != outer_iterator_->second.end(),
        base::NotFatalUntil::M130);
  return BrowsingDataEntryView(outer_iterator_->first, inner_iterator_->first,
                               inner_iterator_->second);
}

BrowsingDataModel::Iterator& BrowsingDataModel::Iterator::operator++() {
  if (inner_iterator_ != outer_iterator_->second.end()) {
    inner_iterator_++;
  }
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
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
        complete_callback) {
  BuildFromStoragePartition(browser_context->GetDefaultStoragePartition(),
                            std::move(delegate), std::move(complete_callback));
}

void BrowsingDataModel::BuildFromNonDefaultStoragePartition(
    content::StoragePartition* storage_partition,
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
        complete_callback) {
  DCHECK(!storage_partition->GetConfig().is_default());
  BuildFromStoragePartition(storage_partition, std::move(delegate),
                            std::move(complete_callback));
}

void BrowsingDataModel::BuildFromStoragePartition(
    content::StoragePartition* storage_partition,
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
        complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto model = BuildEmpty(storage_partition, std::move(delegate));
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

void BrowsingDataModel::RemoveBrowsingDataEntriesFromDisk(
    const BrowsingDataModel::DataKeyEntries& browsing_data_entries,
    base::OnceClosure completed) {
  // Bind the lifetime of the helper to the lifetime of the callback.
  auto helper = std::make_unique<StorageRemoverHelper>(
      storage_partition_, quota_helper_, delegate_.get());
  auto* helper_pointer = helper.get();

  base::OnceClosure wrapped_completed = base::BindOnce(
      [](std::unique_ptr<StorageRemoverHelper> storage_remover,
         base::OnceClosure completed) { std::move(completed).Run(); },
      std::move(helper), std::move(completed));

  helper_pointer->RemoveDataKeyEntries(browsing_data_entries,
                                       std::move(wrapped_completed));
}

std::unique_ptr<BrowsingDataModel> BrowsingDataModel::BuildEmpty(
    content::StoragePartition* storage_partition,
    std::unique_ptr<Delegate> delegate) {
  return base::WrapUnique(new BrowsingDataModel(
      storage_partition, std::move(delegate)));  // Private constructor
}

void BrowsingDataModel::AddBrowsingData(const DataKey& data_key,
                                        StorageType storage_type,
                                        uint64_t storage_size,
                                        uint64_t cookie_count,
                                        bool blocked_third_party) {
  DataOwner data_owner =
      absl::visit(GetDataOwner(delegate_.get(), storage_type), data_key);

  // Find the existing entry if it exists, constructing any missing components.
  auto& entry = browsing_data_entries_[data_owner][data_key];

  entry.storage_size += storage_size;
  // Per canonical cookie the count should always be 1, otherwise this count is
  // irrelevant.
  entry.cookie_count = cookie_count;
  entry.blocked_third_party = blocked_third_party;
  entry.storage_types.Put(storage_type);
}

void BrowsingDataModel::RemoveBrowsingData(const DataOwner& data_owner,
                                           base::OnceClosure completed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RemoveBrowsingDataEntriesFromDisk(browsing_data_entries_[data_owner],
                                    std::move(completed));

  // Immediately remove the affected entries from the in-memory model. Different
  // UI elements have different sync vs. async expectations. Exposing a
  // completed callback, but updating the model synchronously, serves both.
  browsing_data_entries_.erase(data_owner);
}

void BrowsingDataModel::RemovePartitionedBrowsingData(
    const DataOwner& data_owner,
    const net::SchemefulSite& top_level_site,
    base::OnceClosure completed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DataKeyEntries affected_data_key_entries;

  GetAffectedDataKeyEntriesForRemovePartitionedBrowsingData(
      data_owner, top_level_site, affected_data_key_entries);

  RemoveBrowsingDataEntriesFromDisk(affected_data_key_entries,
                                    std::move(completed));

  // Immediately remove the affected entries from the in-memory model.
  // Different UI elements have different sync vs. async expectations.
  // Exposing a completed callback, but updating the model synchronously,
  // serves both.
  auto& data_owner_entries = browsing_data_entries_[data_owner];
  for (auto& entry : affected_data_key_entries) {
    data_owner_entries.erase(entry.first);
  }
  if (data_owner_entries.empty()) {
    browsing_data_entries_.erase(data_owner);
  }
}

void BrowsingDataModel::RemoveUnpartitionedBrowsingData(
    const DataOwner& data_owner,
    base::OnceClosure completed) {
  DataKeyEntries affected_data_key_entries;

  for (const auto& entry : browsing_data_entries_[data_owner]) {
    if (!GetThirdPartyPartitioningSite(entry.first).has_value()) {
      affected_data_key_entries.insert(entry);
    }
  }

  RemoveBrowsingDataEntriesFromDisk(affected_data_key_entries,
                                    std::move(completed));

  auto& data_owner_entries = browsing_data_entries_[data_owner];
  for (auto& entry : affected_data_key_entries) {
    data_owner_entries.erase(entry.first);
  }
  if (data_owner_entries.empty()) {
    browsing_data_entries_.erase(data_owner);
  }
}

bool BrowsingDataModel::IsStorageTypeCookieLike(
    StorageType storage_type) const {
  if (delegate_) {
    auto delegate_response = delegate_->IsStorageTypeCookieLike(storage_type);
    if (delegate_response.has_value()) {
      return delegate_response.value();
    }
  }

  switch (storage_type) {
    case BrowsingDataModel::StorageType::kTrustTokens:
    case BrowsingDataModel::StorageType::kInterestGroup:
    case BrowsingDataModel::StorageType::kAttributionReporting:
    case BrowsingDataModel::StorageType::kPrivateAggregation:
    case BrowsingDataModel::StorageType::kSharedDictionary:
      return false;
    case BrowsingDataModel::StorageType::kSharedStorage:
    case BrowsingDataModel::StorageType::kLocalStorage:
    case BrowsingDataModel::StorageType::kQuotaStorage:
    case BrowsingDataModel::StorageType::kSessionStorage:
    case BrowsingDataModel::StorageType::kSharedWorker:
    case BrowsingDataModel::StorageType::kCookie:
    case BrowsingDataModel::StorageType::kCdmStorage:
      return true;
    case BrowsingDataModel::StorageType::kExtendedDelegateRange:
      NOTREACHED();
  }
}

bool BrowsingDataModel::IsBlockedByThirdPartyCookieBlocking(
    const DataKey& data_key,
    StorageType storage_type) const {
  if (GetThirdPartyPartitioningSite(data_key).has_value()) {
    return false;
  }

  if (delegate_) {
    auto delegate_response =
        delegate_->IsBlockedByThirdPartyCookieBlocking(data_key, storage_type);
    if (delegate_response.has_value()) {
      return delegate_response.value();
    }
  }

  return IsStorageTypeCookieLike(storage_type);
}

void BrowsingDataModel::PopulateFromDisk(base::OnceClosure finished_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool is_shared_storage_enabled =
      base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI);
  bool is_shared_dictionary_enabled = base::FeatureList::IsEnabled(
      network::features::kCompressionDictionaryTransportBackend);
  bool is_interest_group_enabled =
      base::FeatureList::IsEnabled(blink::features::kInterestGroupStorage);
  bool is_attribution_reporting_enabled = base::FeatureList::IsEnabled(
      attribution_reporting::features::kConversionMeasurement);
  bool is_private_aggregation_enabled =
      base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi);

  base::RepeatingClosure completion =
      base::BindRepeating([](const base::OnceClosure&) {},
                          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                              std::move(finished_callback)));

  // The public build interfaces for the model ensure that `this` remains valid
  // until `finished_callback` has been run. Thus, it's safe to pass raw `this`
  // to backend callbacks.

  // Issued Trust Tokens
  storage_partition_->GetNetworkContext()->GetStoredTrustTokenCounts(
      base::BindOnce(&OnTrustTokenIssuanceInfoLoaded, this, completion));

  // Quota Storage
  quota_helper_->StartFetching(
      base::BindOnce(&OnQuotaStorageLoaded, this, completion));
  storage_partition_->GetDOMStorageContext()->GetLocalStorageUsage(
      base::BindOnce(&OnLocalStorageLoaded, this, completion));

  // Cookies
  storage_partition_->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(&OnCookiesLoaded, this, completion));

  // Shared storage origins
  if (is_shared_storage_enabled) {
    storage_partition_->GetSharedStorageManager()->FetchOrigins(
        base::BindOnce(&OnSharedStorageLoaded, this, completion));
  }

  // Shared Dictionaries
  if (is_shared_dictionary_enabled) {
    storage_partition_->GetNetworkContext()->GetSharedDictionaryUsageInfo(
        base::BindOnce(&OnSharedDictionaryUsageLoaded, this, completion));
  }

  // Interest Groups
  if (is_interest_group_enabled) {
    content::InterestGroupManager* manager =
        storage_partition_->GetInterestGroupManager();
    if (manager) {
      manager->GetAllInterestGroupDataKeys(
          base::BindOnce(&OnInterestGroupsLoaded, this, completion));
    }
  }

  // Attribution Reporting
  if (is_attribution_reporting_enabled) {
    storage_partition_->GetAttributionDataModel()->GetAllDataKeys(
        base::BindOnce(&OnAttributionReportingLoaded, this, completion));
  }

  // Private Aggregation
  if (is_private_aggregation_enabled) {
    storage_partition_->GetPrivateAggregationDataModel()->GetAllDataKeys(
        base::BindOnce(&OnPrivateAggregationLoaded, this, completion));
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  storage_partition_->GetCdmStorageDataModel()->GetUsagePerAllStorageKeys(
      base::BindOnce(&OnCdmStorageLoaded, this, completion), base::Time::Min(),
      base::Time::Max());
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  // Data loaded from non-components storage types via the delegate.
  if (delegate_) {
    delegate_->GetAllDataKeys(
        base::BindOnce(&OnDelegateDataLoaded, this, completion));
  }
}

BrowsingDataModel::BrowsingDataModel(
    content::StoragePartition* storage_partition,
    std::unique_ptr<Delegate> delegate)
    : storage_partition_(storage_partition), delegate_(std::move(delegate)) {
  if (storage_partition_) {
    quota_helper_ = BrowsingDataQuotaHelper::Create(storage_partition_);
  }
}

void BrowsingDataModel::
    GetAffectedDataKeyEntriesForRemovePartitionedBrowsingData(
        const DataOwner& data_owner,
        const net::SchemefulSite& top_level_site,
        DataKeyEntries& affected_data_key_entries) {
  for (const auto& entry : browsing_data_entries_[data_owner]) {
    if (GetThirdPartyPartitioningSite(entry.first) == top_level_site) {
      affected_data_key_entries.insert(entry);
    }
  }
}
