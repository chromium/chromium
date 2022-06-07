// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"

class GURL;
class PrefService;

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
class WebWrapper;

// Information returned by the product info APIs.
struct ProductInfo {
  ProductInfo();
  ProductInfo(const ProductInfo&);
  ProductInfo& operator=(const ProductInfo&);
  ~ProductInfo();

  std::string title;
  GURL image_url;
  uint64_t product_cluster_id;
  uint64_t offer_id;
  std::string currency_code;
  long amount_micros;
  std::string country_code;
};

// Information returned by the merchant info APIs.
struct MerchantInfo {
  MerchantInfo();
  MerchantInfo(const MerchantInfo&) = delete;
  MerchantInfo& operator=(const MerchantInfo&) = delete;
  MerchantInfo(MerchantInfo&&);
  MerchantInfo& operator=(MerchantInfo&&) = default;
  ~MerchantInfo();

  float star_rating;
  uint32_t count_rating;
  GURL details_page_url;
  bool has_return_policy;
  float non_personalized_familiarity_score;
  bool contains_sensitive_content;
  bool proactive_message_disabled;
};

// Callbacks for querying a single URL or observing information from all
// navigated urls.
using ProductInfoCallback =
    base::OnceCallback<void(const GURL&, const absl::optional<ProductInfo>&)>;
using MerchantInfoCallback =
    base::OnceCallback<void(const GURL&, absl::optional<MerchantInfo>)>;

class ShoppingService : public KeyedService, public base::SupportsUserData {
 public:
  ShoppingService(bookmarks::BookmarkModel* bookmark_model,
                  optimization_guide::NewOptimizationGuideDecider* opt_guide,
                  PrefService* pref_service);
  ~ShoppingService() override;

  ShoppingService(const ShoppingService&) = delete;
  ShoppingService& operator=(const ShoppingService&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void GetProductInfoForUrl(const GURL& url, ProductInfoCallback callback);

  void GetMerchantInfoForUrl(const GURL& url, MerchantInfoCallback callback);

  void Shutdown() override;

 private:
  // "CommerceTabHelper" encompases both the content/ and ios/ versions.
  friend class CommerceTabHelper;
  // Test classes are also friends.
  friend class ShoppingServiceTestBase;

  // A notification that a WebWrapper has been created. This typically
  // corresponds to a user creating a tab.
  void WebWrapperCreated(WebWrapper* web);

  // A notification that a WebWrapper has been destroyed. This signals that the
  // web page backing the provided WebWrapper is about to be destroyed.
  // Typically corresponds to a user closing a tab.
  void WebWrapperDestroyed(WebWrapper* web);

 private:
  // Allow tests to access private methods.
  friend class ShoppingServiceTestBase;

  // A notification that a web wrapper finished a navigation in the primary
  // main frame.
  void DidNavigatePrimaryMainFrame(WebWrapper* web);

  // A notification that the user navigated away from the |from_url|.
  void DidNavigateAway(WebWrapper* web, const GURL& from_url);

  // A notification that the provided web wrapper has finished loading its main
  // frame.
  void DidFinishLoad(WebWrapper* web);

  // Whether APIs like |GetProductInfoForURL| are enabled and allowed to be
  // used.
  bool IsProductInfoApiEnabled();

  // Whether the PDP (product details page) state of a page is allowed to be
  // recorded.
  bool IsPDPMetricsRecordingEnabled();

  // A callback for recording metrics after page navigation and having
  // determined the page is shopping related.
  void PDPMetricsCallback(
      bool is_off_the_record,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Whether APIs like |GetMerchantInfoForURL| are enabled and allowed to be
  // used.
  bool IsMerchantInfoApiEnabled();

  void HandleOptGuideProductInfoResponse(
      const GURL& url,
      ProductInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void HandleOptGuideMerchantInfoResponse(
      const GURL& url,
      MerchantInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Update the cache notifying that a tab is on the specified URL.
  void UpdateProductInfoCacheForInsertion(const GURL& url);

  // Update the data stored in the cache.
  void UpdateProductInfoCache(const GURL& url,
                              bool needs_js,
                              std::unique_ptr<ProductInfo> info);

  // Get the data stored in the cache or nullptr if none exists.
  const ProductInfo* GetFromProductInfoCache(const GURL& url);

  // Update the cache storing product info for a navigation away from the
  // provided URL or closing of a tab.
  void UpdateProductInfoCacheForRemoval(const GURL& url);

  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> opt_guide_;

  raw_ptr<PrefService> pref_service_;

  // The service's means of observing the bookmark model which is automatically
  // removed from the model when destroyed. This will be null if no
  // BookmarkModel is provided to the service.
  std::unique_ptr<ShoppingBookmarkModelObserver> shopping_bookmark_observer_;

  // This is a cache that maps URL to a tuple of number of web wrappers the URL
  // is open in, whether the javascript fallback needs to run, and the product
  // info associated with the URL, so: <count, run_js, info>.
  std::unordered_map<std::string,
                     std::tuple<uint32_t, bool, std::unique_ptr<ProductInfo>>>
      product_info_cache_;

  base::WeakPtrFactory<ShoppingService> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
