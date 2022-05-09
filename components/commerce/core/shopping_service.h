// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"

class GURL;

class PrefRegistrySimple;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace optimization_guide {
class NewOptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace commerce {

class ShoppingBookmarkModelObserver;

// Information returned by the product info APIs.
struct ProductInfo {
  ProductInfo();
  ProductInfo(const ProductInfo&) = delete;
  ProductInfo& operator=(const ProductInfo&) = delete;
  ~ProductInfo();

  std::string title;
  GURL image_url;
  uint64_t product_cluster_id;
  uint64_t offer_id;
  std::string currency_code;
  long amount_micros;
  std::string country_code;
};

// Callbacks for querying a single URL or observing information from all
// navigated urls.
using ProductInfoCallback =
    base::OnceCallback<void(const GURL&, const absl::optional<ProductInfo>&)>;

class ShoppingService : public KeyedService, public base::SupportsUserData {
 public:
  ShoppingService(bookmarks::BookmarkModel* bookmark_model,
                  optimization_guide::NewOptimizationGuideDecider* opt_guide);
  ~ShoppingService() override;

  ShoppingService(const ShoppingService&) = delete;
  ShoppingService& operator=(const ShoppingService&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void GetProductInfoForUrl(const GURL& url, ProductInfoCallback callback);

  void Shutdown() override;

 private:
  // Whether APIs like |GetProductInfoForURL| are enabled and allowed to be
  // used.
  bool IsProductInfoApiEnabled();

  void HandleOptGuideProductInfoResponse(
      const GURL& url,
      ProductInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> opt_guide_;

  // The service's means of observing the bookmark model which is automatically
  // removed from the model when destroyed. This will be null if no
  // BookmarkModel is provided to the service.
  std::unique_ptr<ShoppingBookmarkModelObserver> shopping_bookmark_observer_;

  base::WeakPtrFactory<ShoppingService> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
