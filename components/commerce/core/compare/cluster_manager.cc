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

}  // namespace

ClusterManager::ClusterManager(
    ProductSpecificationsService* product_specification_service,
    const GetProductInfoCallback& get_product_info_cb,
    const GetOpenUrlInfosCallback& get_open_url_infos_cb)
    : get_product_info_cb_(get_product_info_cb),
      get_open_url_infos_cb_(get_open_url_infos_cb) {
  obs_.Observe(product_specification_service);
}

ClusterManager::~ClusterManager() = default;

void ClusterManager::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& product_specifications_set) {
  base::Uuid uuid = product_specifications_set.uuid();
  product_group_map_[uuid] =
      std::make_unique<ProductGroup>(uuid, product_specifications_set.urls());
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
    const ProductSpecificationsSet& product_specifications_set) {
  OnProductSpecificationsSetAdded(product_specifications_set);
}

void ClusterManager::OnProductSpecificationsSetRemoved(const base::Uuid& uuid) {
  product_group_map_.erase(uuid);
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
  // TODO(qinmin): check if there are corner cases with existing product groups.
  if (candidate_product_map_.find(url) != candidate_product_map_.end()) {
    return;
  }

  AddCandidateProduct(url, product_info);
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
  ProductGroup* group = product_group_map_[uuid].get();
  for (const auto& candidate_product : candidate_product_map_) {
    if (group->member_products.find(candidate_product.first) !=
        group->member_products.end()) {
      continue;
    }
    if (IsProductSimilarToGroup(candidate_product.second->category_data,
                                group->categories)) {
      candidate_products.emplace_back(candidate_product.first);
    }
  }
  return candidate_products;
}

}  // namespace commerce
