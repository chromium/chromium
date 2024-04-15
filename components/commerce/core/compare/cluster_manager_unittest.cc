// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/candidate_product.h"
#include "components/commerce/core/compare/product_group.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {
const std::string kTestUrl1 = "http://www.foo1.com";
const std::string kTestUrl2 = "http://www.foo2.com";
const std::string kTestUrl3 = "http://www.foo3.com";
const std::string kCategoryLamp = "Lamp";
const std::string kCategoryChair = "Chair";
const std::string kProductGroupId = "FurnitureGroup";
const std::string kGroupTile = "Furniture";
}  // namespace

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

  std::map<GURL, std::unique_ptr<CandidateProduct>>* GetCandidateProductMap() {
    return &cluster_manager_->candidate_product_map_;
  }

  std::map<std::string, std::unique_ptr<ProductGroup>>* GetProductGroupMap() {
    return &cluster_manager_->product_group_map_;
  }

  void UpdateUrlInfos(std::vector<GURL> urls) {
    url_infos_.clear();
    for (const auto& url : urls) {
      UrlInfo info;
      info.url = url;
      url_infos_.emplace_back(info);
    }
  }

  void UpdateProductInfo(const std::string& label) {
    product_info_ = ProductInfo();
    product_info_.category_data.add_product_categories()
        ->add_category_labels()
        ->set_category_default_label(label);
  }

  void AddProductGroup(const std::string& label) {
    std::unique_ptr<ProductGroup> group =
        std::make_unique<ProductGroup>(kProductGroupId, kGroupTile);
    CategoryData data;
    data.add_product_categories()
        ->add_category_labels()
        ->set_category_default_label(label);
    group->categories.emplace_back(data);
    cluster_manager_->AddProductGroup(std::move(group));
  }

  void RemoveProductGroup(const std::string& group_id) {
    cluster_manager_->RemoveProductGroup(group_id);
  }

 protected:
  std::unique_ptr<ClusterManager> cluster_manager_;
  ProductInfo product_info_;
  std::vector<UrlInfo> url_infos_;
};

TEST_F(ClusterManagerTest, AddAndRemoveCandidateProduct) {
  GURL url(kTestUrl1);
  UrlInfo info;
  info.url = url;
  url_infos_.emplace_back(info);
  cluster_manager_->DidNavigatePrimaryMainFrame(url);
  ASSERT_EQ(1u, GetCandidateProductMap()->size());

  url_infos_.clear();
  cluster_manager_->DidNavigateAway(url);
  ASSERT_EQ(0u, GetCandidateProductMap()->size());
}

TEST_F(ClusterManagerTest, NewCandidateProductClustered) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add 3 products, product 1 and 3 has the same category.
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  CandidateProduct* product1 = (*GetCandidateProductMap())[foo1].get();
  ASSERT_EQ(product1->similar_candidate_products_urls.size(), 1u);
  ASSERT_EQ(product1->similar_candidate_products_urls.count(foo3), 1u);

  CandidateProduct* product2 = (*GetCandidateProductMap())[foo2].get();
  ASSERT_EQ(product2->similar_candidate_products_urls.size(), 0u);

  CandidateProduct* product3 = (*GetCandidateProductMap())[foo3].get();
  ASSERT_EQ(product3->similar_candidate_products_urls.size(), 1u);
  ASSERT_EQ(product3->similar_candidate_products_urls.count(foo1), 1u);
}

TEST_F(ClusterManagerTest, RemoveClusteredCandidateProduct) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add 3 products, product 1 and 3 has the same category.
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  // Remove product 3.
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  CandidateProduct* product1 = (*GetCandidateProductMap())[foo1].get();
  ASSERT_EQ(product1->similar_candidate_products_urls.size(), 0u);

  CandidateProduct* product2 = (*GetCandidateProductMap())[foo2].get();
  ASSERT_EQ(product2->similar_candidate_products_urls.size(), 0u);
}

TEST_F(ClusterManagerTest, AddCandidateProductToExistingProductGroup) {
  AddProductGroup(kCategoryLamp);
  ProductGroup* product_group = (*GetProductGroupMap())[kProductGroupId].get();

  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add the first product.
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  ASSERT_EQ(0u, product_group->member_products.size());
  ASSERT_EQ(1u, product_group->candidate_products.size());
  ASSERT_EQ(product_group->candidate_products.count(foo1), 1u);

  // Add the second product.
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  ASSERT_EQ(0u, product_group->member_products.size());
  ASSERT_EQ(1u, product_group->candidate_products.size());
  ASSERT_EQ(product_group->candidate_products.count(foo1), 1u);

  // Add the third product.
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(0u, product_group->member_products.size());
  ASSERT_EQ(2u, product_group->candidate_products.size());
  ASSERT_EQ(product_group->candidate_products.count(foo1), 1u);
  ASSERT_EQ(product_group->candidate_products.count(foo3), 1u);
}

TEST_F(ClusterManagerTest, AddProductGroupAfterAddingCandidateProduct) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);

  AddProductGroup(kCategoryLamp);
  ProductGroup* product_group = (*GetProductGroupMap())[kProductGroupId].get();
  ASSERT_EQ(0u, product_group->member_products.size());
  ASSERT_EQ(2u, product_group->candidate_products.size());
  ASSERT_EQ(product_group->candidate_products.count(foo1), 1u);
  ASSERT_EQ(product_group->candidate_products.count(foo3), 1u);
}

TEST_F(ClusterManagerTest, RemoveProductGroup) {
  AddProductGroup(kCategoryLamp);
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  RemoveProductGroup(kProductGroupId);
  ASSERT_FALSE((*GetProductGroupMap())[kProductGroupId]);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());
}

TEST_F(ClusterManagerTest, RemoveCandidateProductFromProductGroup) {
  AddProductGroup(kCategoryLamp);
  ProductGroup* product_group = (*GetProductGroupMap())[kProductGroupId].get();
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add 3 products.
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  UpdateProductInfo(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  UpdateProductInfo(kCategoryLamp);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());
  ASSERT_EQ(2u, product_group->candidate_products.size());

  // Remove product 3.
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  ASSERT_EQ(0u, product_group->member_products.size());
  ASSERT_EQ(1u, product_group->candidate_products.size());
  ASSERT_EQ(product_group->candidate_products.count(foo1), 1u);
}

}  // namespace commerce
