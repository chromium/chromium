// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/candidate_product.h"
#include "components/commerce/core/compare/product_group.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {
const std::string kTestUrl1 = "http://www.foo1.com";
const std::string kTestUrl2 = "http://www.foo2.com";
const std::string kTestUrl3 = "http://www.foo3.com";
const std::string kProductUrl = "http://www.chair.com";
const std::string kCategoryLamp = "Lamp";
const std::string kCategoryChair = "Chair";
const std::string kCategoryGamingChair = "GamingChair";
const std::string kProductGroupName = "Furniture";
}  // namespace

class ClusterManagerTest : public testing::Test {
 public:
  ClusterManagerTest() = default;
  ~ClusterManagerTest() override = default;

  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    product_specification_service_ =
        std::make_unique<ProductSpecificationsService>(
            std::make_unique<ProductSpecificationsSyncBridge>(
                syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                    store_.get()),
                processor_.CreateForwardingProcessor()));
    cluster_manager_ = std::make_unique<ClusterManager>(
        product_specification_service_.get(),
        base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                            base::Unretained(this)),
        base::BindRepeating(&ClusterManagerTest::url_infos,
                            base::Unretained(this)));
    InitializeProductInfos();
  }

  void GetProductInfo(const GURL& url, ProductInfoCallback product_info_cb) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(product_info_cb), url, product_infos_[url]));
  }

  const std::vector<UrlInfo> url_infos() { return url_infos_; }

  std::map<GURL, std::unique_ptr<CandidateProduct>>* GetCandidateProductMap() {
    return &cluster_manager_->candidate_product_map_;
  }

  std::map<base::Uuid, std::unique_ptr<ProductGroup>>* GetProductGroupMap() {
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

  base::Uuid AddProductSpecificationSet() {
    base::Uuid uuid = base::Uuid::GenerateRandomV4();
    std::vector<GURL> url_group = {GURL(kProductUrl)};
    cluster_manager_->OnProductSpecificationsSetAdded(ProductSpecificationsSet(
        uuid.AsLowercaseString(), 0, 0, url_group, kProductGroupName));
    return uuid;
  }

  void RemoveProductSpecificationSet(const base::Uuid& uuid) {
    cluster_manager_->OnProductSpecificationsSetRemoved(uuid);
  }

  std::vector<GURL> FindSimilarCandidateProductsForProductGroup(
      const base::Uuid& uuid) {
    return cluster_manager_->FindSimilarCandidateProductsForProductGroup(uuid);
  }

 protected:
  ProductInfo CreateProductInfo(const std::string& label) {
    ProductInfo product_info = ProductInfo();
    product_info.category_data.add_product_categories()
        ->add_category_labels()
        ->set_category_default_label(label);
    return product_info;
  }

  void InitializeProductInfos() {
    product_infos_[GURL(kTestUrl1)] = CreateProductInfo(kCategoryLamp);
    product_infos_[GURL(kTestUrl2)] = CreateProductInfo(kCategoryChair);
    product_infos_[GURL(kTestUrl3)] = CreateProductInfo(kCategoryLamp);
    product_infos_[GURL(kProductUrl)] = CreateProductInfo(kCategoryLamp);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ProductSpecificationsService> product_specification_service_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  std::unique_ptr<ClusterManager> cluster_manager_;
  std::map<GURL, ProductInfo> product_infos_;
  std::vector<UrlInfo> url_infos_;
};

TEST_F(ClusterManagerTest, AddAndRemoveCandidateProduct) {
  GURL url(kTestUrl1);
  UrlInfo info;
  info.url = url;
  url_infos_.emplace_back(info);
  cluster_manager_->DidNavigatePrimaryMainFrame(url);
  base::RunLoop().RunUntilIdle();
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
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  std::set<GURL> similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo1);
  ASSERT_EQ(similar_candidate_products.size(), 1u);
  ASSERT_EQ(similar_candidate_products.count(foo3), 1u);

  similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo2);
  ASSERT_EQ(similar_candidate_products.size(), 0u);

  similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo3);
  ASSERT_EQ(similar_candidate_products.size(), 1u);
  ASSERT_EQ(similar_candidate_products.count(foo1), 1u);
}

TEST_F(ClusterManagerTest, CandidateProductWithMultipleLabelsClustered) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Product 1 belongs to 2 categories.
  product_infos_[foo1].category_data.add_product_categories()
        ->add_category_labels()
        ->set_category_default_label(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  // Product 2's category label has 2 levels, Chair -> GamingChair.
  product_infos_[foo2].category_data.mutable_product_categories(0)
        ->add_category_labels()
        ->set_category_default_label(kCategoryGamingChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  std::set<GURL> similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo1);
  ASSERT_EQ(similar_candidate_products.size(), 2u);
  ASSERT_EQ(similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(similar_candidate_products.count(foo3), 1u);

  similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo2);
  ASSERT_EQ(similar_candidate_products.size(), 1u);
  ASSERT_EQ(similar_candidate_products.count(foo1), 1u);

  similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo3);
  ASSERT_EQ(similar_candidate_products.size(), 1u);
  ASSERT_EQ(similar_candidate_products.count(foo1), 1u);
}

TEST_F(ClusterManagerTest, RemoveClusteredCandidateProduct) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add 3 products, product 1 and 3 has the same category.
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  // Remove product 3.
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  std::set<GURL> similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo1);
  ASSERT_EQ(similar_candidate_products.size(), 0u);

  similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo2);
  ASSERT_EQ(similar_candidate_products.size(), 0u);
}

TEST_F(ClusterManagerTest,
       CandidateProductRemovedBeforeGetProductInfoCompletes) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Add 2 products.
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  base::RunLoop().RunUntilIdle();

  // Add the 3rd product, and immediately removes it.
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  // Let GetProductInfo() for the 3rd product to complete.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  std::set<GURL> similar_candidate_products =
      cluster_manager_->FindSimilarCandidateProducts(foo1);
  ASSERT_EQ(similar_candidate_products.size(), 0u);
}

TEST_F(ClusterManagerTest, FindSimilarCandidateProductsForProductGroup) {
  base::Uuid uuid = AddProductSpecificationSet();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());

  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(uuid);

  ASSERT_EQ(1u, product_group->member_products.size());
  ASSERT_EQ(2u, candidates.size());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo1) !=
              candidates.end());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo3) !=
              candidates.end());
}

TEST_F(ClusterManagerTest,
       FindSimilarCandidateProductBeforeGetProductInfoCompletes) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();

  base::Uuid uuid = AddProductSpecificationSet();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());

  // Before GetProductInfo() completes,
  // FindSimilarCandidateProductsForProductGroup() should not find any matches.
  FindSimilarCandidateProductsForProductGroup(uuid);
  ASSERT_EQ(1u, product_group->member_products.size());

  base::RunLoop().RunUntilIdle();
  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(uuid);
  ASSERT_EQ(1u, product_group->member_products.size());
  ASSERT_EQ(2u, candidates.size());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo1) !=
              candidates.end());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo3) !=
              candidates.end());
}

TEST_F(ClusterManagerTest,
       FindSimilarCandidateProductForMultiLabelProductGroup) {
  ProductInfo product_info = ProductInfo();
  product_info.category_data.add_product_categories()
      ->add_category_labels()
      ->set_category_default_label(kCategoryLamp);
  product_info.category_data.add_product_categories()
      ->add_category_labels()
      ->set_category_default_label(kCategoryChair);
  product_infos_[GURL(kProductUrl)] = product_info;

  base::Uuid uuid = AddProductSpecificationSet();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(uuid);

  ASSERT_EQ(1u, product_group->member_products.size());
  ASSERT_EQ(3u, candidates.size());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo1) !=
              candidates.end());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo2) !=
              candidates.end());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo3) !=
              candidates.end());
}

TEST_F(ClusterManagerTest, RemoveProductGroup) {
  base::Uuid uuid = AddProductSpecificationSet();
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  RemoveProductSpecificationSet(uuid);
  ASSERT_FALSE((*GetProductGroupMap())[uuid]);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());
}

TEST_F(ClusterManagerTest, GetProductGroupForCandidateProduct) {
  base::Uuid uuid = AddProductSpecificationSet();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  base::RunLoop().RunUntilIdle();
  GURL foo1(kTestUrl1);
  ASSERT_FALSE(cluster_manager_->GetProductGroupForCandidateProduct(foo1));

  UpdateUrlInfos(std::vector<GURL>{foo1});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  base::RunLoop().RunUntilIdle();
  auto possible_product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo1);
  ASSERT_TRUE(possible_product_group);
  ASSERT_EQ(possible_product_group->uuid, product_group->uuid);

  UpdateUrlInfos(std::vector<GURL>());
  cluster_manager_->DidNavigateAway(foo1);
  ASSERT_FALSE(cluster_manager_->GetProductGroupForCandidateProduct(foo1));
}

TEST_F(ClusterManagerTest, AddCandidateProductAlreadyInProductGroups) {
  base::Uuid uuid = AddProductSpecificationSet();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  GURL foo1(kProductUrl);
  UpdateUrlInfos(std::vector<GURL>{foo1});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  base::RunLoop().RunUntilIdle();
  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(uuid);
  ASSERT_EQ(0u, candidates.size());

  ASSERT_FALSE(cluster_manager_->GetProductGroupForCandidateProduct(foo1));
  ASSERT_EQ(1u, product_group->member_products.size());
}

}  // namespace commerce
