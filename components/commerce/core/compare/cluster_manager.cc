// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <optional>
#include <set>
#include <string>

#include "components/commerce/core/compare/candidate_product.h"
#include "components/commerce/core/compare/product_group.h"

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

// Gets the bottom label from a product category.
std::optional<CategoryLabel> GetBottomLabel(const ProductCategory& category) {
  int label_size = category.category_labels_size();
  if (label_size == 0) {
    return std::nullopt;
  }
  return category.category_labels(label_size - 1);
}

// Determines if two CategoryData are similar. Currently this method only checks
// whether the bottom category matches.
// TODO(qinmin): adding more logics here for complicated cases.
bool AreCategoriesSimilar(const CategoryData& first,
                          const CategoryData& second) {
  std::set<std::string> bottom_labels;
  for (const auto& first_category : first.product_categories()) {
    std::optional<CategoryLabel> label = GetBottomLabel(first_category);
    if (label) {
      bottom_labels.emplace(label->category_default_label());
    }
  }
  for (const auto& second_category : second.product_categories()) {
    std::optional<CategoryLabel> label = GetBottomLabel(second_category);
    if (label && bottom_labels.find(label->category_default_label()) !=
                     bottom_labels.end()) {
      return true;
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
}  // namespace

ClusterManager::ClusterManager(
    const GetProductInfoCallback& get_product_info_cb,
    const GetOpenUrlInfosCallback& get_open_url_infos_cb)
    : get_product_info_cb_(get_product_info_cb),
      get_open_url_infos_cb_(get_open_url_infos_cb) {}

ClusterManager::~ClusterManager() = default;

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

  // TODO(qinmin): If a product is added to a product group, determine
  // whether it should be removed from `candidate_product_map_`.
  AddCandidateProduct(url, product_info);
  AddProductToProductGroupssIfNecessary(url, product_info);
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

void ClusterManager::AddProductToProductGroupssIfNecessary(
    const GURL& url,
    const std::optional<const ProductInfo>& product_info) {
  for (const auto& group : product_group_map_) {
    if (group.second->member_products.find(url) !=
        group.second->member_products.end()) {
      continue;
    }
    if (group.second->candidate_products.find(url) !=
        group.second->candidate_products.end()) {
      DCHECK(false);
      continue;
    }
    if (IsProductSimilarToGroup(product_info->category_data,
                                group.second->categories)) {
      group.second->candidate_products.insert(url);
    }
  }
}

void ClusterManager::RemoveCandidateProductURLIfNotOpen(const GURL& url) {
  if (candidate_product_map_.find(url) != candidate_product_map_.end() &&
      !IsUrlOpen(url, get_open_url_infos_cb_)) {
    candidate_product_map_.erase(url);
    for (const auto& product : candidate_product_map_) {
      product.second->similar_candidate_products_urls.erase(url);
    }
    RemoveProductFromProductGroupsIfNecessary(url);
  }
}

void ClusterManager::RemoveProductFromProductGroupsIfNecessary(
    const GURL& url) {
  for (const auto& group : product_group_map_) {
    if (group.second->candidate_products.find(url) !=
        group.second->candidate_products.end()) {
      group.second->candidate_products.erase(url);
    }
  }
}

void ClusterManager::AddProductGroup(
    std::unique_ptr<ProductGroup> product_group) {
  ProductGroup* group = product_group.get();
  product_group_map_[product_group->group_id] = std::move(product_group);
  for (const auto& candidate_product : candidate_product_map_) {
    if (IsProductSimilarToGroup(candidate_product.second->category_data,
                                group->categories)) {
      group->candidate_products.insert(candidate_product.first);
    }
  }
}

void ClusterManager::RemoveProductGroup(const std::string& group_id) {
  product_group_map_.erase(group_id);
}

}  // namespace commerce
