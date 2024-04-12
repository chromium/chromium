// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include "base/functional/callback.h"
#include "components/commerce/core/commerce_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {

class ClusterManagerTest : public testing::Test {
 public:
  ClusterManagerTest() = default;
  ~ClusterManagerTest() override = default;

  void SetUp() override {
    cluster_manager_ = std::make_unique<ClusterManager>(
        base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                            base::Unretained(this)),
        base::BindRepeating(&ClusterManagerTest::url_infos,
                            base::Unretained(this)));
  }

  void GetProductInfo(const GURL& url, ProductInfoCallback product_info_cb) {
    std::move(product_info_cb).Run(url, product_info_);
  }

  const std::vector<UrlInfo> url_infos() { return url_infos_; }

  std::map<GURL, ProductInfo> GetCandidateProductMap() {
    return cluster_manager_->candidate_product_map_;
  }

 protected:
  std::unique_ptr<ClusterManager> cluster_manager_;
  ProductInfo product_info_;
  std::vector<UrlInfo> url_infos_;
};

TEST_F(ClusterManagerTest, AddAndRemoveCandidateProduct) {
  GURL url("www.foo.com");
  UrlInfo info;
  info.url = url;
  url_infos_.emplace_back(info);
  cluster_manager_->DidNavigatePrimaryMainFrame(url);
  ASSERT_EQ(1u, GetCandidateProductMap().size());

  url_infos_.clear();
  cluster_manager_->DidNavigateAway(url);
  ASSERT_EQ(0u, GetCandidateProductMap().size());
}

}  // namespace commerce
