// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_H_

#include <iterator>
#include <map>

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"
#include "components/browsing_data/content/shared_worker_info.h"
#include "components/webid/federated_identity_data_model.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/cdm_storage_data_model.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "net/cookies/canonical_cookie.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class StoragePartition;
}

// Provides a model interface into a collection of Browsing Data for use in the
// UI. Exposes a uniform view into browsing data based on the concept of
// "data owners", which denote which entity the data should be closely
// associated with in UI surfaces.
class BrowsingDataModel {
 public:
  // The entity that logically owns a set of data. All browsing data will be
  // grouped by its owner.
  using DataOwner = absl::variant<std::string,  // Hostname
                                  url::Origin>;

  // Storage types which are represented by the model. Some types have
  // incomplete implementations, and are marked as such.
  // TODO(crbug.com/40205603): Complete implementations for all browsing data.
  enum class StorageType {
    kTrustTokens = 1,  // Only issuance information considered.
    kSharedStorage = 2,
    kLocalStorage,
    kSessionStorage,
    kInterestGroup,
    kAttributionReporting,
    kPrivateAggregation,
    kQuotaStorage,
    kSharedDictionary,
    kSharedWorker,
    kCookie,
    kCdmStorage,

    kFirstType = kTrustTokens,
    kLastType = kCdmStorage,
    kExtendedDelegateRange =
        63,  // This is needed to include delegate values when adding delegate
             // browsing data to the model.
  };
  using StorageTypeSet = base::EnumSet<StorageType,
                                       StorageType::kFirstType,
                                       StorageType::kExtendedDelegateRange>;

  // The information which uniquely identifies this browsing data. The set of
  // data an entry represents can be pulled from the relevant storage backends
  // using this information.
  typedef absl::variant<url::Origin,        // Single origin, e.g. Trust Tokens
                        blink::StorageKey,  // Partitioned JS storage
                        content::InterestGroupManager::InterestGroupDataKey,
                        content::AttributionDataModel::DataKey,
                        content::PrivateAggregationDataModel::DataKey,
                        content::SessionStorageUsageInfo,
                        net::SharedDictionaryIsolationKey,
                        browsing_data::SharedWorkerInfo,
                        net::CanonicalCookie,
                        webid::FederatedIdentityDataModel::DataKey
                        // TODO(crbug.com/40205603): Additional backend keys.
                        >
      DataKey;

  // Information about the data pointed at by a DataKey.
  struct DataDetails {
    ~DataDetails();
    bool operator==(const DataDetails& other) const;

    // An EnumSet of storage types for this data.
    StorageTypeSet storage_types;

    // The on-disk size of this storage.
    uint64_t storage_size = 0;

    // The number of cookies included in this storage. This is only included to
    // support legacy UI surfaces.
    // TODO(crbug.com/40862729): Remove this when UI no longer requires it.
    uint64_t cookie_count = 0;

    // Flag indicating if the data was blocked in a third-party context.
    bool blocked_third_party = false;
  };

  // A view of a single "unit" of browsing data. Considered a "view" as it holds
  // references to data contained within the model.
  struct BrowsingDataEntryView {
    ~BrowsingDataEntryView();
    BrowsingDataEntryView(const BrowsingDataEntryView& other) = delete;

    // Returns true if |origin| is within this browsing data's  owning entity.
    bool Matches(const url::Origin& origin) const;

    // Returns the non-1P SchemefulSite this data is partitioned on. Returns
    // base::nullopt if the data is not partitioned, or is the 1P partition.
    std::optional<net::SchemefulSite> GetThirdPartyPartitioningSite() const;

    // The logical owner of this browsing data. This is the entity which this
    // information will be most strongly associated with in UX surfaces.
    const raw_ref<const DataOwner, DanglingUntriaged> data_owner;

    // The unique identifier for the data represented by this entry.
    const raw_ref<const DataKey, DanglingUntriaged> data_key;

    // Information about the data represented by this entry.
    const raw_ref<const DataDetails, DanglingUntriaged> data_details;

   private:
    friend class BrowsingDataModel;

    BrowsingDataEntryView(const DataOwner& data_owner,
                          const DataKey& data_key,
                          const DataDetails& data_details);
  };

  // A delegate to handle non components/ data type retrieval and deletion.
  class Delegate {
   public:
    struct DelegateEntry {
      DelegateEntry(const DataKey& data_key,
                    StorageType storage_type,
                    uint64_t storage_size);
      DelegateEntry(const DelegateEntry& other);
      ~DelegateEntry();
      DataKey data_key;
      StorageType storage_type;
      uint64_t storage_size;
    };

    // Retrieves all possible data keys with its associated storage size.
    virtual void GetAllDataKeys(
        base::OnceCallback<void(std::vector<DelegateEntry>)> callback) = 0;

    // Removes all data that matches the data key.
    virtual void RemoveDataKey(const DataKey& data_key,
                               StorageTypeSet storage_types,
                               base::OnceClosure callback) = 0;

    // Returns the owner of the data identified by the given DataKey and
    // StorageType, or nullopt if the delegate does not manage the entity that
    // owns the given data.
    virtual std::optional<DataOwner> GetDataOwner(
        const DataKey& data_key,
        StorageType storage_type) const = 0;

    // Returns true if storage type is Cookie-like i.e. non kAPI type.
    virtual std::optional<bool> IsStorageTypeCookieLike(
        StorageType storage_type) const = 0;

    // Returns whether the delegate considers `storage_type` to be blocked by
    // third party cookie blocking, utilizing `data_key` to exclude partitioned
    // data. Returns nullopt if the delegate does not manage the storage type.
    // This method isn't aware of the context in which the data key is being
    // accessed and may return false positive in case it was called for a first
    // party key in a first party context.
    virtual std::optional<bool> IsBlockedByThirdPartyCookieBlocking(
        const DataKey& data_key,
        StorageType storage_type) const = 0;

    // Returns whether cookie deletion for a given `url` is disabled.
    virtual bool IsCookieDeletionDisabled(const GURL& url) = 0;

    // Get a WeakPtr to the instance.
    virtual base::WeakPtr<Delegate> AsWeakPtr() = 0;

    virtual ~Delegate() = default;
  };

  // The model provides a single interface for retrieving browsing data, in the
  // form of an Input iterator (read-only, increment only, no random access)
  // over BrowsingDataEntryViews.
  // Iterators are invalidated whenever the model is updated.
  using DataKeyEntries = std::map<DataKey, DataDetails>;
  using BrowsingDataEntries = std::map<DataOwner, DataKeyEntries>;
  struct Iterator {
    ~Iterator();
    Iterator(const Iterator& iterator);
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

    // Input iterator functionality. These declarations allow STL functions to
    // make use of the iterator interface.
    // More details: https://en.cppreference.com/w/cpp/iterator/iterator_tags
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = BrowsingDataEntryView;
    using pointer = BrowsingDataEntryView*;
    using reference = BrowsingDataEntryView&;
    BrowsingDataEntryView operator*() const;
    Iterator& operator++();

   private:
    friend class BrowsingDataModel;

    Iterator(BrowsingDataEntries::const_iterator outer_iterator,
             BrowsingDataEntries::const_iterator end_outer_iterator);

    BrowsingDataEntries::const_iterator outer_iterator_;
    DataKeyEntries::const_iterator inner_iterator_;
    const BrowsingDataEntries::const_iterator outer_end_iterator_;
  };

  Iterator begin() const;
  Iterator end() const;

  virtual ~BrowsingDataModel();

  // Returns number of entries within the Model.
  size_t size() const { return browsing_data_entries_.size(); }

  // Retrieves the host from the data owner.
  static const std::string GetHost(const DataOwner& data_owner);

  // Retrieves the owning origin for a specific data key.
  static const url::Origin GetOriginForDataKey(
      const BrowsingDataModel::DataKey& data_key);

  // Consults supported storage backends to create and populate a Model based
  // on the current state of `browser_context`.
  static void BuildFromDisk(
      content::BrowserContext* browser_context,
      std::unique_ptr<Delegate> delegate,
      base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
          complete_callback);

  // Consults supported storage backends to create and populate a Model based
  // on the current state of `storage_partition`.
  static void BuildFromNonDefaultStoragePartition(
      content::StoragePartition* storage_partition,
      std::unique_ptr<Delegate> delegate,
      base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
          complete_callback);

  // Creates and returns an empty model, for population via AddBrowsingData().
  static std::unique_ptr<BrowsingDataModel> BuildEmpty(
      content::StoragePartition* storage_partition,
      std::unique_ptr<Delegate> delegate);

  // Directly add browsing data to the Model. The appropriate BrowsingDataEntry
  // will be created or modified. Typically this should only be used when the
  // model was created using BuildEmpty().
  void AddBrowsingData(const DataKey& data_key,
                       StorageType storage_type,
                       uint64_t storage_size,
                       // TODO(crbug.com/40862729): Deprecate cookie count.
                       uint64_t cookie_count = 0,
                       bool blocked_third_party = false);

  // Removes all browsing data associated with `data_owner`, reaches out to
  // all supported storage backends to remove the data, and updates the model.
  // The in-memory representation of the model is updated immediately, while
  // actual deletion from disk occurs async, completion reported by `completed`.
  // Invalidates any iterators.
  // Virtual to allow an in-memory only fake to be created.
  virtual void RemoveBrowsingData(const DataOwner& data_owner,
                                  base::OnceClosure completed);

  // Removes data for `data_owner` partitioned on `top_level_site`.
  // This supports more granular data deletion needed by UI surfaces.
  // The in-memory representation of the model is updated immediately, while
  // actual deletion from disk occurs async, completion reported by `completed`.
  // Invalidates any iterators.
  // Virtual to allow an in-memory only fake to be created.
  virtual void RemovePartitionedBrowsingData(
      const DataOwner& data_owner,
      const net::SchemefulSite& top_level_site,
      base::OnceClosure completed);

  // Removes data for `data_owner` which is not partitioned, or is the 1P
  // partition. This supports more granular data deletion needed by UI surfaces.
  // Virtual to allow an in-memory only fake to be created.
  virtual void RemoveUnpartitionedBrowsingData(const DataOwner& data_owner,
                                               base::OnceClosure completed);

  // Returns true if storage type is Cookie-like i.e. non kAPI type.
  // This can't be static as it requires to consult the delegate.
  bool IsStorageTypeCookieLike(StorageType storage_type) const;

  // Returns whether the provided `storage_type` is blocked when third party
  // cookies are blocked, utilizing `data_key` to exclude partitioned data.
  // This method isn't aware of the context in which the data key is being
  // accessed and may return false positive in case it was called for a first
  // party key in a first party context.
  bool IsBlockedByThirdPartyCookieBlocking(const DataKey& data_key,
                                           StorageType storage_type) const;

 protected:
  friend class BrowsingDataModelTest;

  static void BuildFromStoragePartition(
      content::StoragePartition* storage_partition,
      std::unique_ptr<Delegate> delegate,
      base::OnceCallback<void(std::unique_ptr<BrowsingDataModel>)>
          complete_callback);

  // Takes a list of `browsing_data_entries` to remove from disk and runs
  // `completed` callback on completion.
  virtual void RemoveBrowsingDataEntriesFromDisk(
      const BrowsingDataModel::DataKeyEntries& browsing_data_entries,
      base::OnceClosure completed);

  // Private as one of the static BuildX functions should be used instead.
  explicit BrowsingDataModel(
      content::StoragePartition* storage_partition,
      std::unique_ptr<Delegate> delegate
      // TODO(crbug.com/40205603): Inject other dependencies.
  );

  void GetAffectedDataKeyEntriesForRemovePartitionedBrowsingData(
      const DataOwner& data_owner,
      const net::SchemefulSite& top_level_site,
      DataKeyEntries& affected_data_key_entries);

  // Pulls information from disk and populate the model.
  // Virtual to allow an in-memory only fake to be created.
  virtual void PopulateFromDisk(base::OnceClosure finished_callback);

  // Backing data structure for this model. Is a map from data owners to a
  // list of tuples (stored as a map) of <DataKey, DataDetails>. Building the
  // model requires updating existing entries as data becomes available, so
  // fast lookup is required. Similarly, keying the outer map on data owner
  // supports removal by data owner performantly.
  BrowsingDataEntries browsing_data_entries_;

  // Non-owning pointers to storage backends. All derivable from a browser
  // context, but broken out to allow easier injection in tests.
  // TODO(crbug.com/40205603): More backends to come, they should all be broken
  // out from the browser context at the appropriate level.
  raw_ptr<content::StoragePartition, DanglingUntriaged> storage_partition_;

  // Used to handle quota managed data on IO thread.
  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;

  // Owning pointer to the delegate responsible for non components/ data
  // retrieval and removal.
  std::unique_ptr<Delegate> delegate_;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_H_
