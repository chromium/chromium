// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "components/commerce/core/compare/candidate_product.h"
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
  return category.category_labels(label_size - 1 - level_from_bottom)
      .category_default_label();
}

std::optional<std::string> GetShortestLabelAtBottom(
    const CategoryData& category_data) {
  std::optional<std::string> title;
  for (const auto& product_category : category_data.product_categories()) {
    std::optional<std::string> bottom_label =
        GetLabelFromBottom(product_category, 0);
    if (!bottom_label) {
      continue;
    }
    if (!title || title->length() > bottom_label->length()) {
      title = std::move(bottom_label);
    }
  }
  return title;
}

// Determines if two CategoryData are similar. If the bottom label from one of
// the categories in the first CategoryData matches either the bottom or the
// second to bottom label from one of the categories in the second CategoryData,
// they are considered similar.
bool AreCategoriesSimilar(const CategoryData& first,
                          const CategoryData& second) {
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
    const GetProductInfoCallback& get_product_info_cb,
    const GetOpenUrlInfosCallback& get_open_url_infos_cb)
    : get_product_info_cb_(get_product_info_cb),
      get_open_url_infos_cb_(get_open_url_infos_cb) {
  const std::vector<ProductSpecificationsSet> product_specifications_sets =
      product_specification_service->GetAllProductSpecifications();
  for (const auto& product : product_specifications_sets) {
    OnProductSpecificationsSetAdded(product);
  }

  obs_.Observe(product_specification_service);
}

ClusterManager::~ClusterManager() = default;

void ClusterManager::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& product_specifications_set) {
  base::Uuid uuid = product_specifications_set.uuid();
  product_group_map_[uuid] =
      std::make_unique<ProductGroup>(uuid, product_specifications_set.urls(),
                                     product_specifications_set.update_time());
  const std::set<GURL>& urls = product_group_map_[uuid]->member_products;
  if (urls.size() == 0) {
    CHECK(false) << "Production specification set shouldn't be empty.";
    return;
  }

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

std::optional<EntryPointInfo> ClusterManager::GetEntryPointInfoForNavigation(
    GURL url) {
  std::set<GURL> similar_urls = FindSimilarCandidateProducts(url);
  if (similar_urls.size() == 0) {
    return std::nullopt;
  }

  similar_urls.insert(url);
  std::optional<std::string> title =
      GetShortestLabelAtBottom(candidate_product_map_[url]->category_data);
  return std::make_optional<EntryPointInfo>(title ? title.value() : "",
                                            std::move(similar_urls));
}

std::optional<EntryPointInfo> ClusterManager::GetEntryPointInfoForSelection(
    GURL old_url,
    GURL new_url) {
  std::set<GURL> similar_urls = FindSimilarCandidateProducts(old_url);
  if (similar_urls.find(new_url) == similar_urls.end()) {
    return std::nullopt;
  }
  std::set<GURL> similar_urls_new = FindSimilarCandidateProducts(new_url);
  if (similar_urls_new.find(old_url) == similar_urls_new.end()) {
    return std::nullopt;
  }
  similar_urls.merge(similar_urls_new);
  std::optional<std::string> title_old =
      GetShortestLabelAtBottom(candidate_product_map_[old_url]->category_data);
  std::optional<std::string> title_new =
      GetShortestLabelAtBottom(candidate_product_map_[new_url]->category_data);
  std::optional<std::string> title;
  if (!title_old) {
    title = std::move(title_new);
  } else if (title_new) {
    title = title_old->size() < title_new->size() ? std::move(title_old)
                                                  : std::move(title_new);
  } else {
    title = std::move(title_old);
  }
  return std::make_optional<EntryPointInfo>(title ? title.value() : "",
                                            std::move(similar_urls));
}

}  // namespace commerce
