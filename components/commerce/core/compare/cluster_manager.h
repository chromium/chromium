// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/commerce/core/commerce_types.h"
#include "url/gurl.h"

namespace commerce {

// Class for clustering product information.
class ClusterManager {
 public:
  using GetProductInfoCallback =
      base::RepeatingCallback<void(const GURL&, ProductInfoCallback)>;
  using GetOpenUrlInfosCallback =
      base::RepeatingCallback<const std::vector<UrlInfo>()>;

  ClusterManager(const GetProductInfoCallback& get_product_info_cb,
                 const GetOpenUrlInfosCallback& get_open_url_infos_cb);
  ~ClusterManager();
  ClusterManager(const ClusterManager&) = delete;
  ClusterManager& operator=(const ClusterManager&) = delete;

  // A notification that a WebWrapper with `url` has been destroyed. This
  // signals that the web page backing the provided WebWrapper is about to be
  // destroyed. Typically corresponds to a user closing a tab.
  void WebWrapperDestroyed(const GURL& url);
  // A notification that a web wrapper with `url` finished a navigation in the
  // primary main frame.
  void DidNavigatePrimaryMainFrame(const GURL& url);
  // A notification that the user navigated away from `from_url`.
  void DidNavigateAway(const GURL& from_url);

 private:
  friend class ClusterManagerTest;

  // Adds a candidate product to cluster.
  void AddCandidateProduct(
      const GURL& url,
      const std::optional<const ProductInfo>& product_info);

  // Removes a candidate product URL if it is not open in any tabs.
  void RemoveCandidateProductURLIfNotOpen(const GURL& url);

  // Callback to get product info.
  GetProductInfoCallback get_product_info_cb_;

  // Callback to get currently opened urls.
  GetOpenUrlInfosCallback get_open_url_infos_cb_;

  // A map storing info of candidate products, keyed by product page URL.
  // TODO(qinmin): change ProductInfo to another structure for finding
  // related products.
  std::map<GURL, ProductInfo> candidate_product_map_;

  base::WeakPtrFactory<ClusterManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
