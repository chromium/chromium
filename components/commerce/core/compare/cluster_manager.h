// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "url/gurl.h"

namespace commerce {
class ClusterServerProxy;
class ProductSpecificationsService;
struct CandidateProduct;
struct ProductGroup;

// Class for clustering product information.
class ClusterManager : public ProductSpecificationsSet::Observer {
 public:
  using GetProductInfoCallback =
      base::RepeatingCallback<void(const GURL&, ProductInfoCallback)>;
  using GetOpenUrlInfosCallback =
      base::RepeatingCallback<const std::vector<UrlInfo>()>;
  using GetEntryPointInfoCallback =
      base::OnceCallback<void(std::optional<EntryPointInfo>)>;

  class Observer : public base::CheckedObserver {
   public:
    // Notifies that ClusterManager has finished clustering for a recent
    // navigation with `url`.
    virtual void OnClusterFinishedForNavigation(const GURL& url) {}
  };

  ClusterManager(ProductSpecificationsService* product_specification_service,
                 std::unique_ptr<ClusterServerProxy> cluster_server_proxy,
                 const GetProductInfoCallback& get_product_info_cb,
                 const GetOpenUrlInfosCallback& get_open_url_infos_cb);
  ~ClusterManager() override;
  ClusterManager(const ClusterManager&) = delete;
  ClusterManager& operator=(const ClusterManager&) = delete;

  // ProductSpecificationsSet::Observe Implementation.
  void OnProductSpecificationsSetAdded(
      const ProductSpecificationsSet& product_specifications_set) override;
  void OnProductSpecificationsSetUpdate(
      const ProductSpecificationsSet& before,
      const ProductSpecificationsSet& product_specifications_set) override;
  void OnProductSpecificationsSetRemoved(
      const ProductSpecificationsSet& set) override;

  // A notification that a WebWrapper with `url` has been destroyed. This
  // signals that the web page backing the provided WebWrapper is about to be
  // destroyed. Typically corresponds to a user closing a tab.
  void WebWrapperDestroyed(const GURL& url);
  // A notification that a web wrapper with `url` finished a navigation in the
  // primary main frame.
  void DidNavigatePrimaryMainFrame(const GURL& url);
  // A notification that the user navigated away from `from_url`.
  void DidNavigateAway(const GURL& from_url);

  // Gets a product group that the given product can be clustered into. If
  // this candidate product is already in a product group, empty result
  // is returned.
  virtual std::optional<ProductGroup> GetProductGroupForCandidateProduct(
      const GURL& product_url);

  // Gets information to decide if entry point should show on navivation to
  // `url` and return it. The returned EntryPointInfo will include `url`
  // if it can be clustered into a group.
  virtual void GetEntryPointInfoForNavigation(
      const GURL& url,
      GetEntryPointInfoCallback callback);

  // Gets information to decide if entry point should show on selection and
  // return it. `old_url` is the URL of the tab before selection.
  // `new_url` is the URL of the tab after selection.
  virtual void GetEntryPointInfoForSelection(
      const GURL& old_url,
      const GURL& new_url,
      GetEntryPointInfoCallback callback);

  // Finds similar candidate products for a product group.
  std::vector<GURL> FindSimilarCandidateProductsForProductGroup(
      const base::Uuid& uuid);

  // Finds comparable products from an EntryPointInfo.
  virtual void GetComparableProducts(const EntryPointInfo& entry_point_info,
                                     GetEntryPointInfoCallback callback);

  // Registers an observer for cluster manager.
  void AddObserver(Observer* observer);

  // Removes an observer for cluster manager.
  void RemoveObserver(Observer* observer);

 private:
  friend class ClusterManagerTest;

  void OnGetAllProductSpecificationsSets(
      const std::vector<ProductSpecificationsSet> sets);

  // Called when information about a product is retrieved.
  void OnProductInfoRetrieved(
      const GURL& url,
      const std::optional<const ProductInfo>& product_info);

  // Called when category data for a list of URLs are retrieved.
  void OnAllCategoryDataRetrieved(
      const base::Uuid& uuid,
      const std::set<GURL>& urls,
      const std::vector<CategoryData>& category_data);

  // Adds a candidate product to `candidate_product_map_`.
  void AddCandidateProduct(
      const GURL& url,
      const std::optional<const ProductInfo>& product_info);

  // Removes a candidate product URL if it is not open in any tabs.
  void RemoveCandidateProductURLIfNotOpen(const GURL& url);

  // Finds similar candidate products for a candidate product. The returned
  // URLs doesn't include the `product_url`.
  std::set<GURL> FindSimilarCandidateProducts(const GURL& product_url);

  void OnGetComparableProducts(
      const EntryPointInfo& entry_point_info,
      GetEntryPointInfoCallback callback,
      const std::vector<uint64_t>& cluster_product_ids);

  void OnProductInfoFetchedForSimilarUrls(
      GetEntryPointInfoCallback callback,
      const std::vector<std::pair<GURL, const ProductInfo>>& product_infos);

  // Check if product set is still eligible for clustering recommendations given
  // its uuid and last updated time. Please note that this method can be used
  // for both `ProductSpecificationSet` and `ProductGroup`.
  bool IsSetEligibleForClustering(const base::Uuid& uuid,
                                  const base::Time& update_time);

  // A ProductGroup might become ineligible for clustering because it hasn't
  // been updated for a long time. This method will be scheduled to run every
  // day after ClusterManager is constructed to remove the ineiligible
  // ProductGroups.
  //
  // Please note that this doesn't mean the underlying ProductSpecificationsSet
  // will be removed; only the ProductGroup is removed in ClusterManager so that
  // we'll stop making clustering recommendations for these ProductGroups.
  void RemoveIneligibleGroupsForClustering();

  std::unique_ptr<ClusterServerProxy> cluster_server_proxy_;

  // Callback to get product info.
  GetProductInfoCallback get_product_info_cb_;

  // Callback to get currently opened urls.
  GetOpenUrlInfosCallback get_open_url_infos_cb_;

  // A map storing info of existing product groups, keyed by product group ID.
  std::map<base::Uuid, std::unique_ptr<ProductGroup>> product_group_map_;

  // A map storing info of candidate products, keyed by product page URL.
  std::map<GURL, std::unique_ptr<CandidateProduct>> candidate_product_map_;

  base::ScopedObservation<ProductSpecificationsService,
                          ProductSpecificationsSet::Observer>
      obs_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ClusterManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
