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
const std::string kProduct1Url = "http://www.chair.com";
const std::string kProduct2Url = "http://www.lamp.com";
const std::string kCategoryLamp = "Lamp";
const std::string kCategoryChair = "Chair";
const std::string kCategoryGamingChair = "GamingChair";
const std::string kProductGroupName = "Furniture";
}  // namespace

class MockProductSpecificationsService : public ProductSpecificationsService {
 public:
  explicit MockProductSpecificationsService(
      std::unique_ptr<ProductSpecificationsSyncBridge> bridge)
      : ProductSpecificationsService(std::move(bridge)) {}
  ~MockProductSpecificationsService() override = default;

  MOCK_METHOD(const std::vector<ProductSpecificationsSet>,
              GetAllProductSpecifications,
              (),
              (override));
};

class MockObserver : public ClusterManager::Observer {
 public:
  MOCK_METHOD(void,
              OnClusterFinishedForNavigation,
              (const GURL& url),
              (override));
};

class ClusterManagerTest : public testing::Test {
 public:
  ClusterManagerTest() = default;
  ~ClusterManagerTest() override = default;

  void SetUp() override {
    store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
    product_specification_service_ =
        std::make_unique<MockProductSpecificationsService>(
            std::make_unique<ProductSpecificationsSyncBridge>(
                syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                    store_.get()),
                processor_.CreateForwardingProcessor()));
    EXPECT_CALL(*product_specification_service_, GetAllProductSpecifications())
        .Times(1);
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

  ProductSpecificationsSet AddProductSpecificationSet() {
    ProductSpecificationsSet product_specifications_set =
        CreateProductSpecificationsSet(kProduct1Url);
    cluster_manager_->OnProductSpecificationsSetAdded(
        product_specifications_set);
    return product_specifications_set;
  }

  void RemoveProductSpecificationSet(const ProductSpecificationsSet& set) {
    cluster_manager_->OnProductSpecificationsSetRemoved(set);
  }

  std::vector<GURL> FindSimilarCandidateProductsForProductGroup(
      const base::Uuid& uuid) {
    return cluster_manager_->FindSimilarCandidateProductsForProductGroup(uuid);
  }

 protected:
  ProductSpecificationsSet CreateProductSpecificationsSet(
      const std::string& url,
      int64_t update_time_usec_since_epoch) {
    base::Uuid uuid = base::Uuid::GenerateRandomV4();
    std::vector<GURL> url_group = {GURL(url)};
    return ProductSpecificationsSet(uuid.AsLowercaseString(), 0,
                                    update_time_usec_since_epoch, url_group,
                                    kProductGroupName);
  }

  ProductSpecificationsSet CreateProductSpecificationsSet(
      const std::string& url) {
    return CreateProductSpecificationsSet(url, 0);
  }

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
    product_infos_[GURL(kProduct1Url)] = CreateProductInfo(kCategoryLamp);
    product_infos_[GURL(kProduct2Url)] = CreateProductInfo(kCategoryChair);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<MockProductSpecificationsService>
      product_specification_service_;
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

TEST_F(ClusterManagerTest,
       ClusterManagerInitializationWithExistingProductSpecificationsSets) {
  ProductSpecificationsSet set1 =
      CreateProductSpecificationsSet(kProduct1Url, 0);
  ProductSpecificationsSet set2 =
      CreateProductSpecificationsSet(kProduct2Url, 0);
  ON_CALL(*product_specification_service_, GetAllProductSpecifications())
      .WillByDefault(
          testing::Return(std::vector<ProductSpecificationsSet>{set1, set2}));
  EXPECT_CALL(*product_specification_service_, GetAllProductSpecifications())
      .Times(1);
  cluster_manager_ = std::make_unique<ClusterManager>(
      product_specification_service_.get(),
      base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                          base::Unretained(this)),
      base::BindRepeating(&ClusterManagerTest::url_infos,
                          base::Unretained(this)));
  ASSERT_EQ(GetProductGroupMap()->size(), 2u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 1u);
  base::RunLoop().RunUntilIdle();

  ProductGroup* product1 = (*GetProductGroupMap())[set1.uuid()].get();
  ASSERT_EQ(product1->categories.size(), 1u);
  ASSERT_EQ(product1->categories[0].product_categories_size(), 1);
  auto category = product1->categories[0].product_categories(0);
  ASSERT_EQ(category.category_labels_size(), 1);
  ASSERT_EQ(category.category_labels(0).category_default_label(),
            kCategoryLamp);
  ProductGroup* product2 = (*GetProductGroupMap())[set2.uuid()].get();
  ASSERT_EQ(product2->categories.size(), 1u);
  ASSERT_EQ(product2->categories[0].product_categories_size(), 1);
  category = product2->categories[0].product_categories(0);
  ASSERT_EQ(category.category_labels_size(), 1);
  ASSERT_EQ(category.category_labels(0).category_default_label(),
            kCategoryChair);
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForNavigation) {
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

  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForNavigation(foo1);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");

  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo2));

  info = cluster_manager_->GetEntryPointInfoForNavigation(foo3);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForNavigationWithInvalidUrl) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  ASSERT_FALSE(
      cluster_manager_->GetEntryPointInfoForNavigation(GURL(kTestUrl3)));
}

TEST_F(ClusterManagerTest,
       GetEntryPointInfoForNavigationWithMultiLabelProduct) {
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

  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForNavigation(foo1);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 3u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");

  info = cluster_manager_->GetEntryPointInfoForNavigation(foo2);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->title, "GamingChair");

  info = cluster_manager_->GetEntryPointInfoForNavigation(foo3);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");
}

TEST_F(ClusterManagerTest,
       GetEntryPointInfoForNavigationAfterRemoveingCandidateProduct) {
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
  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForNavigation(foo1);
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo2));

  // Remove product 3.
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo1));
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo2));
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
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo1));
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo1));

  // Let GetProductInfo() for the 3rd product to complete.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo1));
}

TEST_F(ClusterManagerTest, FindSimilarCandidateProductsForProductGroup) {
  base::Uuid uuid = AddProductSpecificationSet().uuid();
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

  base::Uuid uuid = AddProductSpecificationSet().uuid();
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
       FindSimilarCandidateProductWithProductsAlreadyInGroup) {
  GURL foo1(kProduct1Url);
  GURL foo2(kTestUrl1);
  GURL foo3(kTestUrl2);
  GURL foo4(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3, foo4});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo4);
  base::RunLoop().RunUntilIdle();

  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForNavigation(foo2);
  ASSERT_EQ(3u, info->similar_candidate_products_urls.size());
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo4), 1u);
  info = cluster_manager_->GetEntryPointInfoForNavigation(foo1);
  ASSERT_EQ(3u, info->similar_candidate_products_urls.size());
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo4), 1u);

  // Similar candidates will not include `foo1` if it is added to a product
  // group.
  ProductSpecificationsSet set1 =
      CreateProductSpecificationsSet(kProduct1Url, 0);
  cluster_manager_->OnProductSpecificationsSetAdded(set1);

  info = cluster_manager_->GetEntryPointInfoForNavigation(foo2);
  ASSERT_EQ(2u, info->similar_candidate_products_urls.size());
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo4), 1u);
  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForNavigation(foo1));
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
  product_infos_[GURL(kProduct1Url)] = product_info;

  base::Uuid uuid = AddProductSpecificationSet().uuid();
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

TEST_F(ClusterManagerTest,
       FindSimilarCandidateProductsForProductGroupWithProductsAlreadyInGroup) {
  ProductSpecificationsSet set1 =
      CreateProductSpecificationsSet(kProduct1Url, 0);
  ProductSpecificationsSet set2 =
      CreateProductSpecificationsSet(kProduct2Url, 0);
  cluster_manager_->OnProductSpecificationsSetAdded(set1);
  cluster_manager_->OnProductSpecificationsSetAdded(set2);
  GURL foo1(kProduct1Url);
  GURL foo2(kProduct2Url);
  GURL foo3(kTestUrl1);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();

  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(set1.uuid());
  ASSERT_EQ(1u, candidates.size());
  ASSERT_TRUE(std::find(candidates.begin(), candidates.end(), foo3) !=
              candidates.end());
  candidates = FindSimilarCandidateProductsForProductGroup(set2.uuid());
  ASSERT_EQ(0u, candidates.size());
}

TEST_F(ClusterManagerTest, RemoveProductGroup) {
  ProductSpecificationsSet product_specs = AddProductSpecificationSet();
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  RemoveProductSpecificationSet(product_specs);
  ASSERT_FALSE((*GetProductGroupMap())[product_specs.uuid()]);
  ASSERT_EQ(3u, GetCandidateProductMap()->size());
}

TEST_F(ClusterManagerTest, GetProductGroupForCandidateProduct) {
  base::Uuid uuid = AddProductSpecificationSet().uuid();
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

TEST_F(ClusterManagerTest, GetProductGroupForCandidateProductAlreadyInGroup) {
  ProductSpecificationsSet set1 =
      CreateProductSpecificationsSet(kProduct1Url, 0);
  cluster_manager_->OnProductSpecificationsSetAdded(set1);
  GURL foo1(kProduct1Url);
  GURL foo2(kTestUrl1);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  base::RunLoop().RunUntilIdle();

  auto product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo1);
  ASSERT_FALSE(product_group);
  product_group = cluster_manager_->GetProductGroupForCandidateProduct(foo2);
  ASSERT_TRUE(product_group);
  ASSERT_EQ(product_group->uuid, set1.uuid());
}

TEST_F(ClusterManagerTest, MultipleSimilarProductGroupForCandidateProduct) {
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kTestUrl1, 0);
  cluster_manager_->OnProductSpecificationsSetAdded(set1);
  GURL foo(kProduct1Url);
  UpdateUrlInfos(std::vector<GURL>{foo});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo);
  base::RunLoop().RunUntilIdle();

  auto product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo);
  ASSERT_EQ(product_group->uuid, set1.uuid());

  ProductSpecificationsSet set2 =
      CreateProductSpecificationsSet(kTestUrl3, 100);
  cluster_manager_->OnProductSpecificationsSetAdded(set2);
  base::RunLoop().RunUntilIdle();
  product_group = cluster_manager_->GetProductGroupForCandidateProduct(foo);
  ASSERT_EQ(product_group->uuid, set2.uuid());
}

TEST_F(ClusterManagerTest, AddCandidateProductAlreadyInProductGroups) {
  base::Uuid uuid = AddProductSpecificationSet().uuid();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  GURL foo1(kProduct1Url);
  UpdateUrlInfos(std::vector<GURL>{foo1});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  base::RunLoop().RunUntilIdle();
  std::vector<GURL> candidates =
      FindSimilarCandidateProductsForProductGroup(uuid);
  ASSERT_EQ(0u, candidates.size());

  ASSERT_FALSE(cluster_manager_->GetProductGroupForCandidateProduct(foo1));
  ASSERT_EQ(1u, product_group->member_products.size());
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForSelection) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForSelection(foo1, foo2));
  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForSelection(foo1, foo3);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);
}

TEST_F(ClusterManagerTest,
       GetEntryPointInfoForSelectionWithMultiLabelProducts) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Product 1 belongs to 2 categories.
  product_infos_[foo1]
      .category_data.add_product_categories()
      ->add_category_labels()
      ->set_category_default_label(kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  // Product 2's category label has 2 levels, Chair -> GamingChair.
  product_infos_[foo2]
      .category_data.mutable_product_categories(0)
      ->add_category_labels()
      ->set_category_default_label(kCategoryGamingChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  std::optional<EntryPointInfo> info =
      cluster_manager_->GetEntryPointInfoForSelection(foo1, foo2);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 3u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);

  info = cluster_manager_->GetEntryPointInfoForSelection(foo1, foo3);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products_urls.size(), 3u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products_urls.count(foo3), 1u);

  ASSERT_FALSE(cluster_manager_->GetEntryPointInfoForSelection(foo2, foo3));
}

TEST_F(ClusterManagerTest, ClusterManagerObserver) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  MockObserver observer;
  EXPECT_CALL(observer, OnClusterFinishedForNavigation(foo1)).Times(1);
  EXPECT_CALL(observer, OnClusterFinishedForNavigation(foo2)).Times(0);

  cluster_manager_->AddObserver(&observer);
  UpdateUrlInfos(std::vector<GURL>{foo1});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  base::RunLoop().RunUntilIdle();

  cluster_manager_->RemoveObserver(&observer);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  base::RunLoop().RunUntilIdle();
}
}  // namespace commerce
