// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/compare/candidate_product.h"
#include "components/commerce/core/compare/cluster_server_proxy.h"
#include "components/commerce/core/compare/product_group.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"

namespace commerce {
namespace {
// Check if a URL is currently open.
bool IsUrlOpen(
    GURL url,
    const ClusterManager::GetOpenUrlInfosCallback& get_open_url_infos_cb) {
  const std::vector<UrlInfo> url_infos = get_open_url_infos_cb.Run();
  for (const auto& info : url_infos) {
    if (url == info.url) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> GetShortLabelForCategory(
    const CategoryLabel& category_label) {
  return category_label.category_short_label().empty()
             ? category_label.category_default_label()
             : category_label.category_short_label();
}

// Gets product label from the bottom of a product category. If
// `level_from_bottom` is 0, this returns the last level of the category. If
// `level_from_bottom` is 1, this returns the second to last level of the
// category, and so on.
std::optional<std::string> GetLabelFromBottom(const ProductCategory& category,
                                              int level_from_bottom) {
  int label_size = category.category_labels_size();
  if (label_size <= level_from_bottom) {
    return std::nullopt;
  }
  return GetShortLabelForCategory(
      category.category_labels(label_size - 1 - level_from_bottom));
}

std::string FindTitleForSimilarProducts(
    const std::vector<std::pair<GURL, const ProductInfo>>& product_infos) {
  // Calculate for each label (both bottom and second-to-bottom level), how
  // many products contain this label.
  //
  // Please note that a product can have multiple categories
  // and contain one label for multiple times (e.g. a product can belong to
  // category Car > Blue Car and Car > Race Car, second-to-bottom level label
  // `Car` appearing twice). In this case, we'll only count one product once for
  // one label.
  base::flat_set<std::string> bottom_labels;
  std::map<std::string, int> label_count;

  for (auto pair : product_infos) {
    base::flat_set<std::string> counted_labels;
    for (const auto& product_category :
         pair.second.category_data.product_categories()) {
      std::optional<std::string> bottom_label =
          GetLabelFromBottom(product_category, 0);
      if (bottom_label) {
        std::string bottom_label_string = bottom_label.value();
        bottom_labels.insert(bottom_label_string);
        if (!base::Contains(counted_labels, bottom_label_string)) {
          counted_labels.insert(bottom_label_string);
          label_count[bottom_label_string]++;
        }
      }
      std::optional<std::string> second_to_bottom_label =
          GetLabelFromBottom(product_category, 1);
      if (second_to_bottom_label) {
        std::string second_to_bottom_label_string =
            second_to_bottom_label.value();
        if (!base::Contains(counted_labels, second_to_bottom_label)) {
          counted_labels.insert(second_to_bottom_label_string);
          label_count[second_to_bottom_label_string]++;
        }
      }
    }
  }

  // Get the most common bottom label. When tie, picking the shorter label.
  int max_bottom_count = 0;
  std::string common_bottom_label;
  for (const std::string& bottom_label : bottom_labels) {
    if (label_count[bottom_label] < max_bottom_count) {
      continue;
    }
    if (common_bottom_label.size() == 0 ||
        bottom_label.size() < common_bottom_label.size()) {
      common_bottom_label = bottom_label;
      max_bottom_count = label_count[bottom_label];
    }
  }

  // Early return if we can find a bottom label shared by all products.
  if ((size_t)max_bottom_count == product_infos.size()) {
    return common_bottom_label;
  }

  // If we cannot find a bottom label shared by all products, see if we can find
  // a good second-to-bottom label. For a second-to-bottom label to be good
  // enough to become title, it has to be shared by all products.
  std::string alternate_title;
  for (auto pair : label_count) {
    if ((size_t)pair.second != product_infos.size()) {
      continue;
    }
    if (alternate_title.size() == 0 ||
        alternate_title.size() > pair.first.size()) {
      alternate_title = pair.first;
    }
  }

  return alternate_title.size() == 0 ? common_bottom_label : alternate_title;
}

bool ShouldCluster(const CategoryData& category_data) {
  for (const auto& product_category : category_data.product_categories()) {
    for (const auto& category_label : product_category.category_labels()) {
      if (!category_label.should_trigger_clustering()) {
        return false;
      }
    }
  }
  return true;
}

// Determines if two CategoryData are similar. If the bottom label from one of
// the categories in the first CategoryData matches either the bottom or the
// second to bottom label from one of the categories in the second CategoryData,
// they are considered similar.
bool AreCategoriesSimilar(const CategoryData& first,
                          const CategoryData& second) {
  if (!ShouldCluster(first) || !ShouldCluster(second)) {
    return false;
  }
  for (const auto& first_category : first.product_categories()) {
    std::optional<std::string> bottom_1 = GetLabelFromBottom(first_category, 0);
    std::optional<std::string> second_to_bottom_1 =
        GetLabelFromBottom(first_category, 1);
    for (const auto& second_category : second.product_categories()) {
      std::optional<std::string> bottom_2 =
          GetLabelFromBottom(second_category, 0);
      std::optional<std::string> second_to_bottom_2 =
          GetLabelFromBottom(second_category, 1);
      if (bottom_1) {
        if (bottom_1 == bottom_2 || bottom_1 == second_to_bottom_2) {
          return true;
        }
      }
      if (second_to_bottom_1 && second_to_bottom_1 == bottom_2) {
        return true;
      }
    }
  }
  return false;
}

bool IsProductSimilarToGroup(
    const CategoryData& category,
    const std::vector<CategoryData>& group_categories) {
  for (const auto& member : group_categories) {
    if (AreCategoriesSimilar(category, member)) {
      return true;
    }
  }
  return false;
}

void OnGetCategoryDataDone(
    base::OnceCallback<void(const CategoryData&)> callback,
    const GURL& url,
    const std::optional<const ProductInfo>& product_info) {
  std::move(callback).Run(product_info ? product_info->category_data
                                       : CategoryData());
}

void GetCategoryData(
    const GURL& url,
    const ClusterManager::GetProductInfoCallback& get_product_info_cb,
    base::OnceCallback<void(const CategoryData&)> callback) {
  get_product_info_cb.Run(
      url, base::BindOnce(&OnGetCategoryDataDone, std::move(callback)));
}

void OnGetProductInfoDone(
    base::OnceCallback<void(std::pair<GURL, const ProductInfo>)> callback,
    const GURL& url,
    const std::optional<const ProductInfo>& product_info) {
  std::pair<GURL, const ProductInfo> pair(
      url, product_info.has_value() ? product_info.value() : ProductInfo());
  std::move(callback).Run(std::move(pair));
}

void GetProductInfo(
    const GURL& url,
    const ClusterManager::GetProductInfoCallback& get_product_info_cb,
    base::OnceCallback<void(std::pair<GURL, const ProductInfo>)> callback) {
  get_product_info_cb.Run(
      url, base::BindOnce(&OnGetProductInfoDone, std::move(callback)));
}

bool IsCandidateProductInProductGroup(
    const GURL& candidate_product_url,
    const std::map<base::Uuid, std::unique_ptr<ProductGroup>>&
        product_group_map) {
  for (const auto& product_group : product_group_map) {
    if (product_group.second->member_products.find(candidate_product_url) !=
        product_group.second->member_products.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace

ClusterManager::ClusterManager(
    ProductSpecificationsService* product_specification_service,
    std::unique_ptr<ClusterServerProxy> cluster_server_proxy,
    const GetProductInfoCallback& get_product_info_cb,
    const GetOpenUrlInfosCallback& get_open_url_infos_cb)
    : cluster_server_proxy_(std::move(cluster_server_proxy)),
      get_product_info_cb_(get_product_info_cb),
      get_open_url_infos_cb_(get_open_url_infos_cb) {
  product_specification_service->GetAllProductSpecifications(
      base::BindOnce(&ClusterManager::OnGetAllProductSpecificationsSets,
                     weak_ptr_factory_.GetWeakPtr()));
  obs_.Observe(product_specification_service);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClusterManager::RemoveIneligibleGroupsForClustering,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Days(1));
}

ClusterManager::~ClusterManager() {
  observers_.Clear();
}

void ClusterManager::OnGetAllProductSpecificationsSets(
    const std::vector<ProductSpecificationsSet> sets) {
  for (const auto& set : sets) {
    OnProductSpecificationsSetAdded(set);
  }
}

void ClusterManager::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& product_specifications_set) {
  if (!IsSetEligibleForClustering(product_specifications_set.uuid(),
                                  product_specifications_set.update_time())) {
    return;
  }
  base::Uuid uuid = product_specifications_set.uuid();
  product_group_map_[uuid] =
      std::make_unique<ProductGroup>(uuid, product_specifications_set.name(),
                                     product_specifications_set.urls(),
                                     product_specifications_set.update_time());
  const std::set<GURL>& urls = product_group_map_[uuid]->member_products;

  auto barrier_callback = base::BarrierCallback<const CategoryData&>(
      urls.size(), base::BindOnce(&ClusterManager::OnAllCategoryDataRetrieved,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  product_specifications_set.uuid(), urls));
  for (const auto& url : urls) {
    GetCategoryData(url, get_product_info_cb_, barrier_callback);
  }
}

void ClusterManager::OnProductSpecificationsSetUpdate(
    const ProductSpecificationsSet& before,
    const ProductSpecificationsSet& product_specifications_set) {
  OnProductSpecificationsSetAdded(product_specifications_set);
}

void ClusterManager::OnProductSpecificationsSetRemoved(
    const ProductSpecificationsSet& set) {
  // TODO(qinmin): Check if we still want to keep candidate product from
  // the removed product group in `candidate_product_map_` if tab is still
  // open.
  product_group_map_.erase(set.uuid());
}

void ClusterManager::WebWrapperDestroyed(const GURL& url) {
  RemoveCandidateProductURLIfNotOpen(url);
}

void ClusterManager::DidNavigatePrimaryMainFrame(const GURL& url) {
  if (candidate_product_map_.find(url) == candidate_product_map_.end()) {
    get_product_info_cb_.Run(
        url, base::BindOnce(&ClusterManager::OnProductInfoRetrieved,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ClusterManager::DidNavigateAway(const GURL& from_url) {
  RemoveCandidateProductURLIfNotOpen(from_url);
}

std::optional<ProductGroup> ClusterManager::GetProductGroupForCandidateProduct(
    const GURL& product_url) {
  if (candidate_product_map_.find(product_url) ==
      candidate_product_map_.end()) {
    return std::nullopt;
  }

  if (IsCandidateProductInProductGroup(product_url, product_group_map_)) {
    return std::nullopt;
  }

  CandidateProduct* candidate = candidate_product_map_[product_url].get();
  base::Time max_time = base::Time::Min();
  base::Uuid uuid;
  for (const auto& product_group : product_group_map_) {
    if (IsProductSimilarToGroup(candidate->category_data,
                                product_group.second->categories) &&
        product_group.second->update_time >= max_time) {
      max_time = product_group.second->update_time;
      uuid = product_group.first;
    }
  }
  if (uuid.is_valid()) {
    return *(product_group_map_[uuid]);
  }
  return std::nullopt;
}

void ClusterManager::OnProductInfoRetrieved(
    const GURL& url,
    const std::optional<const ProductInfo>& product_info) {
  if (!product_info) {
    return;
  }

  if (!IsUrlOpen(url, get_open_url_infos_cb_)) {
    return;
  }

  // If this candidate product already exists, nothing needs to be done.
  if (candidate_product_map_.find(url) != candidate_product_map_.end()) {
    return;
  }

  AddCandidateProduct(url, product_info);
  for (auto& observer : observers_) {
    observer.OnClusterFinishedForNavigation(url);
  }
}

void ClusterManager::OnAllCategoryDataRetrieved(
    const base::Uuid& uuid,
    const std::set<GURL>& urls,
    const std::vector<CategoryData>& category_data) {
  if (!product_group_map_[uuid]) {
    return;
  }

  // Check whether product group has changed.
  if (product_group_map_[uuid]->member_products == urls) {
    product_group_map_[uuid]->categories = category_data;
  }
}

void ClusterManager::AddCandidateProduct(
    const GURL& url,
    const std::optional<const ProductInfo>& product_info) {
  std::set<GURL> similar_products;
  for (const auto& product : candidate_product_map_) {
    if (AreCategoriesSimilar(product_info->category_data,
                             product.second->category_data)) {
      similar_products.emplace(product.first);
      product.second->similar_candidate_products_urls.emplace(url);
    }
  }
  candidate_product_map_[url] =
      std::make_unique<CandidateProduct>(url, product_info.value());
  candidate_product_map_[url]->similar_candidate_products_urls =
      similar_products;
}


void ClusterManager::RemoveCandidateProductURLIfNotOpen(const GURL& url) {
  if (candidate_product_map_.find(url) != candidate_product_map_.end() &&
      !IsUrlOpen(url, get_open_url_infos_cb_)) {
    candidate_product_map_.erase(url);
    for (const auto& product : candidate_product_map_) {
      product.second->similar_candidate_products_urls.erase(url);
    }
  }
}

std::vector<GURL> ClusterManager::FindSimilarCandidateProductsForProductGroup(
    const base::Uuid& uuid) {
  std::vector<GURL> candidate_products;
  if (product_group_map_.find(uuid) == product_group_map_.end()) {
    return candidate_products;
  }

  ProductGroup* group = product_group_map_[uuid].get();
  for (const auto& candidate_product : candidate_product_map_) {
    // If the candidate product is in any product group, ignore it.
    if (IsCandidateProductInProductGroup(candidate_product.first,
                                         product_group_map_)) {
      continue;
    }
    if (IsProductSimilarToGroup(candidate_product.second->category_data,
                                group->categories)) {
      candidate_products.emplace_back(candidate_product.first);
    }
  }
  return candidate_products;
}

void ClusterManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ClusterManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ClusterManager::GetComparableProducts(
    const EntryPointInfo& entry_point_info,
    GetEntryPointInfoCallback callback) {
  std::vector<uint64_t> product_cluster_ids;
  for (const auto& kv : entry_point_info.similar_candidate_products) {
    product_cluster_ids.push_back(kv.second);
  }
  cluster_server_proxy_->GetComparableProducts(
      std::move(product_cluster_ids),
      base::BindOnce(&ClusterManager::OnGetComparableProducts,
                     weak_ptr_factory_.GetWeakPtr(), entry_point_info,
                     std::move(callback)));
}

std::set<GURL> ClusterManager::FindSimilarCandidateProducts(
    const GURL& product_url) {
  std::set<GURL> similar_candidate_products;
  if (candidate_product_map_.find(product_url) ==
      candidate_product_map_.end()) {
    return similar_candidate_products;
  }

  if (IsCandidateProductInProductGroup(product_url, product_group_map_)) {
    return similar_candidate_products;
  }

  for (const auto& url :
       candidate_product_map_[product_url]->similar_candidate_products_urls) {
    // Remove products that are already in a group.
    if (IsCandidateProductInProductGroup(url, product_group_map_)) {
      continue;
    }
    similar_candidate_products.insert(url);
  }
  return similar_candidate_products;
}

void ClusterManager::GetEntryPointInfoForNavigation(
    const GURL& url,
    GetEntryPointInfoCallback callback) {
  // Don't trigger proactive entry points when the navigation url can be added
  // to an existing cluster.
  if (GetProductGroupForCandidateProduct(url).has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::set<GURL> similar_urls = FindSimilarCandidateProducts(url);
  if (similar_urls.size() == 0) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  similar_urls.insert(url);

  auto barrier_callback =
      base::BarrierCallback<std::pair<GURL, const ProductInfo>>(
          similar_urls.size(),
          base::BindOnce(&ClusterManager::OnProductInfoFetchedForSimilarUrls,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  for (const auto& similar_url : similar_urls) {
    GetProductInfo(similar_url, get_product_info_cb_, barrier_callback);
  }
}

void ClusterManager::GetEntryPointInfoForSelection(
    const GURL& old_url,
    const GURL& new_url,
    GetEntryPointInfoCallback callback) {
  // Don't trigger proactive entry points when the selection urls can be added
  // to an existing cluster.
  if (GetProductGroupForCandidateProduct(new_url).has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::set<GURL> similar_urls = FindSimilarCandidateProducts(old_url);
  if (similar_urls.find(new_url) == similar_urls.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::set<GURL> similar_urls_new = FindSimilarCandidateProducts(new_url);
  if (similar_urls_new.find(old_url) == similar_urls_new.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  similar_urls.merge(similar_urls_new);

  auto barrier_callback =
      base::BarrierCallback<std::pair<GURL, const ProductInfo>>(
          similar_urls.size(),
          base::BindOnce(&ClusterManager::OnProductInfoFetchedForSimilarUrls,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  for (const auto& similar_url : similar_urls) {
    GetProductInfo(similar_url, get_product_info_cb_, barrier_callback);
  }
}

void ClusterManager::OnProductInfoFetchedForSimilarUrls(
    GetEntryPointInfoCallback callback,
    const std::vector<std::pair<GURL, const ProductInfo>>& product_infos) {
  std::map<GURL, uint64_t> map;
  for (auto pair : product_infos) {
    if (pair.second.product_cluster_id.has_value()) {
      map[pair.first] = pair.second.product_cluster_id.value();
    }
  }
  std::move(callback).Run(std::make_optional<EntryPointInfo>(
      FindTitleForSimilarProducts(product_infos), std::move(map)));
}

void ClusterManager::OnGetComparableProducts(
    const EntryPointInfo& entry_point_info,
    GetEntryPointInfoCallback callback,
    const std::vector<uint64_t>& cluster_product_ids) {
  std::map<GURL, uint64_t> product_cluster_id_map;
  const std::vector<UrlInfo> url_infos = get_open_url_infos_cb_.Run();
  for (const auto& kv : entry_point_info.similar_candidate_products) {
    // If the product Id cannot be clustered, skip it.
    if (!base::Contains(cluster_product_ids, kv.second)) {
      continue;
    }

    // Only add URLs that are still open.
    for (const auto& url_info : url_infos) {
      if (kv.first == url_info.url) {
        product_cluster_id_map.emplace(kv.first, kv.second);
      }
    }
  }

  if (product_cluster_id_map.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(EntryPointInfo(entry_point_info.title,
                                         std::move(product_cluster_id_map)));
}

bool ClusterManager::IsSetEligibleForClustering(const base::Uuid& uuid,
                                                const base::Time& update_time) {
  // TODO(b/335724950): A set could become eligible again if it's opened,
  // despite that it's last update time has passed the limit. We need to handle
  // this case by checking if the product specifications page for a set is
  // opened.
  if (IsUrlOpen(GetProductSpecsTabUrlForID(uuid), get_open_url_infos_cb_)) {
    return true;
  }
  return base::Time::Now() - update_time <
         kProductSpecificationsSetValidForClusteringTime.Get();
}

void ClusterManager::RemoveIneligibleGroupsForClustering() {
  for (auto it = product_group_map_.begin(); it != product_group_map_.end();) {
    const auto& product_group = it->second;
    if (!IsSetEligibleForClustering(product_group->uuid,
                                    product_group->update_time)) {
      // If the product URLs in the removing set are open, add them back to
      // candidate products.
      for (const GURL& product_url : product_group->member_products) {
        if (IsUrlOpen(product_url, get_open_url_infos_cb_) &&
            candidate_product_map_.find(product_url) ==
                candidate_product_map_.end()) {
          get_product_info_cb_.Run(
              product_url,
              base::BindOnce(&ClusterManager::OnProductInfoRetrieved,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      }
      it = product_group_map_.erase(it);
    } else {
      it++;
    }
  }

  // Re-schedule another remove in one day.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClusterManager::RemoveIneligibleGroupsForClustering,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Days(1));
}

}  // namespace commerce
