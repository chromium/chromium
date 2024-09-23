// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/compare/candidate_product.h"
#include "components/commerce/core/compare/cluster_server_proxy.h"
#include "components/commerce/core/compare/product_group.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace commerce {
namespace {
static const uint64_t kProductID1 = 1;
static const uint64_t kProductID2 = 2;
static const uint64_t kProductID3 = 3;
static const uint64_t kProductID4 = 4;
static const uint64_t kProductID5 = 5;

const std::string kTestUrl1 = "http://www.foo1.com";
const std::string kTestUrl2 = "http://www.foo2.com";
const std::string kTestUrl3 = "http://www.foo3.com";
const std::string kProduct1Url = "http://www.chair.com";
const std::string kProduct2Url = "http://www.lamp.com";
const std::string kCategoryLamp = "Lamp";
const std::string kCategoryChair = "Chair";
const std::string kCategoryGamingChair = "GamingChair";
const std::string kProductGroupName = "Furniture";
const std::string kClusterTitle = "ClusteredProduct";

class MockObserver : public ClusterManager::Observer {
 public:
  MOCK_METHOD(void,
              OnClusterFinishedForNavigation,
              (const GURL& url),
              (override));
};

class MockClusterServerProxy : public ClusterServerProxy {
 public:
  MockClusterServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : ClusterServerProxy(identity_manager, url_loader_factory) {}
  MockClusterServerProxy(const MockClusterServerProxy&) = delete;
  MockClusterServerProxy operator=(const MockClusterServerProxy&) = delete;
  ~MockClusterServerProxy() override = default;

  MOCK_METHOD(void,
              GetComparableProducts,
              (const std::vector<uint64_t>&,
               ClusterServerProxy::GetComparableProductsCallback),
              (override));
};
}  // namespace

class ClusterManagerTest : public testing::Test {
 public:
  ClusterManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ClusterManagerTest() override = default;

  void SetUp() override {
    product_specification_service_ =
        std::make_unique<MockProductSpecificationsService>();
    EXPECT_CALL(
        *product_specification_service_,
        GetAllProductSpecifications(
            testing::An<ProductSpecificationsService::GetAllCallback>()))
        .Times(1);
    auto proxy = std::make_unique<MockClusterServerProxy>(nullptr, nullptr);
    server_proxy_ = proxy.get();
    cluster_manager_ = std::make_unique<ClusterManager>(
        product_specification_service_.get(), std::move(proxy),
        base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                            base::Unretained(this)),
        base::BindRepeating(&ClusterManagerTest::url_infos,
                            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
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
      std::vector<GURL> url_group,
      int64_t update_time_usec_since_epoch) {
    base::Uuid uuid = base::Uuid::GenerateRandomV4();
    return ProductSpecificationsSet(uuid.AsLowercaseString(), 0,
                                    update_time_usec_since_epoch, url_group,
                                    kProductGroupName);
  }

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
    return CreateProductSpecificationsSet(
        url, base::Time::Now().InMillisecondsSinceUnixEpoch());
  }

  void AddCategoryLabel(ProductCategory* product_category,
                        const std::string& default_label,
                        const std::string short_label = "",
                        bool should_trigger_cluster = true) {
    auto* category_label = product_category->add_category_labels();
    category_label->set_category_default_label(default_label);
    category_label->set_category_short_label(short_label);
    category_label->set_should_trigger_clustering(should_trigger_cluster);
  }

  ProductInfo CreateProductInfo(const std::string& label, int64_t product_id) {
    ProductInfo product_info = ProductInfo();
    product_info.product_cluster_id = product_id;
    auto* product_category =
        product_info.category_data.add_product_categories();
    AddCategoryLabel(product_category, label);
    return product_info;
  }

  ProductInfo CreateProductInfoWithShortLabel(const std::string& default_label,
                                              const std::string& short_label,
                                              int64_t product_id) {
    ProductInfo product_info = ProductInfo();
    product_info.product_cluster_id = product_id;
    auto* product_category =
        product_info.category_data.add_product_categories();
    AddCategoryLabel(product_category, default_label, short_label, true);
    return product_info;
  }

  void InitializeProductInfos() {
    product_infos_[GURL(kTestUrl1)] =
        CreateProductInfo(kCategoryLamp, kProductID1);
    product_infos_[GURL(kTestUrl2)] =
        CreateProductInfo(kCategoryChair, kProductID2);
    product_infos_[GURL(kTestUrl3)] =
        CreateProductInfo(kCategoryLamp, kProductID3);
    product_infos_[GURL(kProduct1Url)] =
        CreateProductInfo(kCategoryLamp, kProductID4);
    product_infos_[GURL(kProduct2Url)] =
        CreateProductInfo(kCategoryChair, kProductID5);
  }

  void GetEntryPointInfoForNavigation(const GURL& url,
                                      std::optional<EntryPointInfo>* result) {
    base::RunLoop run_loop;
    cluster_manager_->GetEntryPointInfoForNavigation(
        url,
        base::BindOnce(
            [](std::optional<EntryPointInfo>* ret,
               std::optional<EntryPointInfo> info) { *ret = std::move(info); },
            result)
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void GetEntryPointInfoForSelection(const GURL& old_url,
                                     const GURL& new_url,
                                     std::optional<EntryPointInfo>* result) {
    base::RunLoop run_loop;
    cluster_manager_->GetEntryPointInfoForSelection(
        old_url, new_url,
        base::BindOnce(
            [](std::optional<EntryPointInfo>* ret,
               std::optional<EntryPointInfo> info) { *ret = std::move(info); },
            result)
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void GetComparableProducts(const EntryPointInfo& entry_point_info,
                             std::optional<EntryPointInfo>* result) {
    base::RunLoop run_loop;
    cluster_manager_->GetComparableProducts(
        entry_point_info,
        base::BindOnce(
            [](std::optional<EntryPointInfo>* ret,
               std::optional<EntryPointInfo> info) { *ret = std::move(info); },
            result)
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockProductSpecificationsService>
      product_specification_service_;
  std::unique_ptr<ClusterManager> cluster_manager_;
  raw_ptr<MockClusterServerProxy> server_proxy_;
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
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kProduct1Url);
  ProductSpecificationsSet set2 = CreateProductSpecificationsSet(kProduct2Url);
  std::vector<ProductSpecificationsSet> sets({set1, set2});
  ON_CALL(*product_specification_service_,
          GetAllProductSpecifications(
              testing::An<ProductSpecificationsService::GetAllCallback>()))
      .WillByDefault(
          [sets](ProductSpecificationsService::GetAllCallback callback) {
            std::move(callback).Run(sets);
          });
  EXPECT_CALL(*product_specification_service_,
              GetAllProductSpecifications(
                  testing::An<ProductSpecificationsService::GetAllCallback>()))
      .Times(1);
  server_proxy_ = nullptr;
  cluster_manager_ = std::make_unique<ClusterManager>(
      product_specification_service_.get(),
      std::make_unique<ClusterServerProxy>(nullptr, nullptr),
      base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                          base::Unretained(this)),
      base::BindRepeating(&ClusterManagerTest::url_infos,
                          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(GetProductGroupMap()->size(), 2u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 1u);

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

TEST_F(ClusterManagerTest, ClusterManagerInitialization_SkipInvalidSet) {
  // Mock that set1 is no longer valid for clustering.
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(
      kProduct1Url, (base::Time::Now() -
                     kProductSpecificationsSetValidForClusteringTime.Get())
                        .InMillisecondsSinceUnixEpoch());
  // Mock that set2 is no longer valid for clustering but there is a product
  // specifications page open for this set.
  ProductSpecificationsSet set2 = CreateProductSpecificationsSet(
      kProduct2Url, (base::Time::Now() -
                     kProductSpecificationsSetValidForClusteringTime.Get())
                        .InMillisecondsSinceUnixEpoch());
  UpdateUrlInfos(
      std::vector<GURL>{GURL(GetProductSpecsTabUrlForID(set2.uuid()))});

  ProductSpecificationsSet set3 = CreateProductSpecificationsSet(kTestUrl3);
  std::vector<ProductSpecificationsSet> sets({set1, set2, set3});
  ON_CALL(*product_specification_service_,
          GetAllProductSpecifications(
              testing::An<ProductSpecificationsService::GetAllCallback>()))
      .WillByDefault(
          [sets](ProductSpecificationsService::GetAllCallback callback) {
            std::move(callback).Run(sets);
          });
  EXPECT_CALL(*product_specification_service_,
              GetAllProductSpecifications(
                  testing::An<ProductSpecificationsService::GetAllCallback>()))
      .Times(1);
  server_proxy_ = nullptr;
  cluster_manager_ = std::make_unique<ClusterManager>(
      product_specification_service_.get(),
      std::make_unique<ClusterServerProxy>(nullptr, nullptr),
      base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                          base::Unretained(this)),
      base::BindRepeating(&ClusterManagerTest::url_infos,
                          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(GetProductGroupMap()->size(), 2u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 0u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set3.uuid()), 1u);
}

TEST_F(ClusterManagerTest, ClusterManagerInitialization_KickOffRemoving) {
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kProduct1Url);
  std::vector<ProductSpecificationsSet> sets({set1});
  ON_CALL(*product_specification_service_,
          GetAllProductSpecifications(
              testing::An<ProductSpecificationsService::GetAllCallback>()))
      .WillByDefault(
          [sets](ProductSpecificationsService::GetAllCallback callback) {
            std::move(callback).Run(sets);
          });
  EXPECT_CALL(*product_specification_service_,
              GetAllProductSpecifications(
                  testing::An<ProductSpecificationsService::GetAllCallback>()))
      .Times(1);
  server_proxy_ = nullptr;
  cluster_manager_ = std::make_unique<ClusterManager>(
      product_specification_service_.get(),
      std::make_unique<ClusterServerProxy>(nullptr, nullptr),
      base::BindRepeating(&ClusterManagerTest::GetProductInfo,
                          base::Unretained(this)),
      base::BindRepeating(&ClusterManagerTest::url_infos,
                          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(GetProductGroupMap()->size(), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);

  // Add one set that will become invalid in one day and another set that will
  // become invalid in two days.
  std::vector<GURL> url_group1({GURL(kProduct2Url), GURL(kTestUrl1)});
  ProductSpecificationsSet set2 = CreateProductSpecificationsSet(
      url_group1,
      (base::Time::Now() -
       kProductSpecificationsSetValidForClusteringTime.Get() + base::Days(1))
          .InMillisecondsSinceUnixEpoch());
  cluster_manager_->OnProductSpecificationsSetAdded(set2);
  std::vector<GURL> url_group2({GURL(kTestUrl2)});
  ProductSpecificationsSet set3 = CreateProductSpecificationsSet(
      url_group2,
      (base::Time::Now() -
       kProductSpecificationsSetValidForClusteringTime.Get() + base::Days(2))
          .InMillisecondsSinceUnixEpoch());
  cluster_manager_->OnProductSpecificationsSetAdded(set3);

  ASSERT_EQ(GetProductGroupMap()->size(), 3u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set3.uuid()), 1u);

  // Mock that one URL in the soon-to-be invalid set is open.
  UpdateUrlInfos(std::vector<GURL>{GURL(kProduct2Url)});

  // Fast forward one day. The first set would become invalid and the open URLs
  // in the set will be added back to candidate products.
  ASSERT_EQ(0u, GetCandidateProductMap()->size());
  task_environment_.FastForwardBy(base::Days(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(GetProductGroupMap()->size(), 2u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 0u);
  ASSERT_EQ(GetProductGroupMap()->count(set3.uuid()), 1u);
  ASSERT_EQ(1u, GetCandidateProductMap()->size());

  // Fast forward one more day, the second set would become invalid and be
  // removed.
  task_environment_.FastForwardBy(base::Days(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(GetProductGroupMap()->size(), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set1.uuid()), 1u);
  ASSERT_EQ(GetProductGroupMap()->count(set2.uuid()), 0u);
  ASSERT_EQ(GetProductGroupMap()->count(set3.uuid()), 0u);
  ASSERT_EQ(1u, GetCandidateProductMap()->size());
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

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_EQ(info->similar_candidate_products.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo1], kProductID1);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo3], kProductID3);
  ASSERT_EQ(info->title, "Lamp");

  GetEntryPointInfoForNavigation(foo2, &info);
  ASSERT_FALSE(info);

  GetEntryPointInfoForNavigation(foo3, &info);
  ASSERT_EQ(info->similar_candidate_products.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo1], kProductID1);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo3], kProductID3);
  ASSERT_EQ(info->title, "Lamp");
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForNavigation_CanAddToGroup) {
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

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_TRUE(info.has_value());

  // Add a product group that foo1 and foo3 can be added to.
  base::Uuid uuid = AddProductSpecificationSet().uuid();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  base::RunLoop().RunUntilIdle();
  auto possible_product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo3);
  ASSERT_TRUE(possible_product_group);
  ASSERT_EQ(possible_product_group->uuid, product_group->uuid);

  // Since foo1 and foo3 can be added to an existing product group, they'll have
  // empty entry point info.
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info.has_value());
  GetEntryPointInfoForNavigation(foo3, &info);
  ASSERT_FALSE(info.has_value());
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForNavigationWithInvalidUrl) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(GURL(kTestUrl3), &info);
  ASSERT_FALSE(info);
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
  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_EQ(info->similar_candidate_products.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);
  ASSERT_EQ(info->title, "Lamp");
  GetEntryPointInfoForNavigation(foo2, &info);
  ASSERT_FALSE(info);

  // Remove product 3.
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info);
  GetEntryPointInfoForNavigation(foo2, &info);
  ASSERT_FALSE(info);
}

TEST_F(ClusterManagerTest, PrioritizeShortCategoryLabels) {
  const std::string kCategoryShortLabel = "S";
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  // Add 2 products with the same category and both have short category labels.
  product_infos_[GURL(kTestUrl1)] = CreateProductInfoWithShortLabel(
      kCategoryLamp, kCategoryShortLabel, kProductID1);
  product_infos_[GURL(kTestUrl2)] = CreateProductInfoWithShortLabel(
      kCategoryLamp, kCategoryShortLabel, kProductID2);

  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_EQ(info->similar_candidate_products.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo1], kProductID1);
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo2], kProductID2);
  ASSERT_EQ(info->title, kCategoryShortLabel);
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
  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  cluster_manager_->DidNavigateAway(foo3);
  ASSERT_EQ(2u, GetCandidateProductMap()->size());
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info);

  // Let GetProductInfo() for the 3rd product to complete.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info);
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

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(foo2, &info);
  ASSERT_EQ(3u, info->similar_candidate_products.size());
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo4), 1u);
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_EQ(3u, info->similar_candidate_products.size());
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo4), 1u);

  // Similar candidates will not include `foo1` if it is added to a product
  // group.
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kProduct1Url);
  cluster_manager_->OnProductSpecificationsSetAdded(set1);

  GetEntryPointInfoForNavigation(foo2, &info);
  ASSERT_EQ(2u, info->similar_candidate_products.size());
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo4), 1u);
  GetEntryPointInfoForNavigation(foo1, &info);
  ASSERT_FALSE(info);
}

TEST_F(ClusterManagerTest,
       FindSimilarCandidateProductForMultiLabelProductGroup) {
  ProductInfo product_info = ProductInfo();
  auto* product_categories_1 =
      product_info.category_data.add_product_categories();
  AddCategoryLabel(product_categories_1, kCategoryLamp);
  auto* product_categories_2 =
      product_info.category_data.add_product_categories();
  AddCategoryLabel(product_categories_2, kCategoryChair);
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
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kProduct1Url);
  ProductSpecificationsSet set2 = CreateProductSpecificationsSet(kProduct2Url);
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
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(kProduct1Url);
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
  ProductSpecificationsSet set1 = CreateProductSpecificationsSet(
      kTestUrl1,
      (base::Time::Now() - base::Days(1)).InMillisecondsSinceUnixEpoch());
  cluster_manager_->OnProductSpecificationsSetAdded(set1);
  GURL foo(kProduct1Url);
  UpdateUrlInfos(std::vector<GURL>{foo});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo);
  base::RunLoop().RunUntilIdle();

  auto product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo);
  ASSERT_EQ(product_group->uuid, set1.uuid());

  ProductSpecificationsSet set2 = CreateProductSpecificationsSet(
      kTestUrl3, base::Time::Now().InMillisecondsSinceUnixEpoch());
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

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForSelection(foo1, foo2, &info);
  ASSERT_FALSE(info);
  GetEntryPointInfoForSelection(foo1, foo3, &info);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products.size(), 2u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo1], kProductID1);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);
  ASSERT_EQ(info->similar_candidate_products[foo3], kProductID3);
}

TEST_F(ClusterManagerTest,
       GetEntryPointInfoForSelectionWithMultiLabelProducts) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});

  // Product 1 belongs to 2 categories.
  auto* product_category_1 =
      product_infos_[foo1].category_data.add_product_categories();
  AddCategoryLabel(product_category_1, kCategoryChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  // Product 2's category label has 2 levels, Chair -> GamingChair.
  auto* product_category_2 =
      product_infos_[foo2].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_category_2, kCategoryGamingChair);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForSelection(foo1, foo2, &info);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products.size(), 3u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);

  GetEntryPointInfoForSelection(foo1, foo3, &info);
  ASSERT_EQ(info->title, "Lamp");
  ASSERT_EQ(info->similar_candidate_products.size(), 3u);
  ASSERT_EQ(info->similar_candidate_products.count(foo1), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo2), 1u);
  ASSERT_EQ(info->similar_candidate_products.count(foo3), 1u);

  GetEntryPointInfoForSelection(foo2, foo3, &info);
  ASSERT_FALSE(info);
}

TEST_F(ClusterManagerTest, GetEntryPointInfoForSelection_CanAddToGroup) {
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  cluster_manager_->DidNavigatePrimaryMainFrame(foo1);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo2);
  cluster_manager_->DidNavigatePrimaryMainFrame(foo3);
  base::RunLoop().RunUntilIdle();

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForSelection(foo1, foo3, &info);
  ASSERT_TRUE(info.has_value());

  // Add a product group that foo1 and foo3 can be added to.
  base::Uuid uuid = AddProductSpecificationSet().uuid();
  ProductGroup* product_group = (*GetProductGroupMap())[uuid].get();
  ASSERT_EQ(1u, product_group->member_products.size());
  base::RunLoop().RunUntilIdle();
  auto possible_product_group =
      cluster_manager_->GetProductGroupForCandidateProduct(foo3);
  ASSERT_TRUE(possible_product_group);
  ASSERT_EQ(possible_product_group->uuid, product_group->uuid);

  // Since foo1 and foo3 can be added to an existing product group, they'll have
  // empty entry point info.
  GetEntryPointInfoForSelection(foo1, foo3, &info);
  ASSERT_FALSE(info.has_value());
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

TEST_F(ClusterManagerTest, TabClosedWhenGetComparableProducts) {
  std::vector<uint64_t> product_ids{kProductID1, kProductID2, kProductID3};
  EXPECT_CALL(*server_proxy_, GetComparableProducts(product_ids, _))
      .WillRepeatedly(
          [](std::vector<uint64_t> ids,
             ClusterServerProxy::GetComparableProductsCallback callback) {
            std::move(callback).Run(ids);
          });
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  std::map<GURL, uint64_t> comparable_products;
  comparable_products.emplace(foo1, kProductID1);
  comparable_products.emplace(foo2, kProductID2);
  comparable_products.emplace(foo3, kProductID3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  EntryPointInfo info(kClusterTitle, comparable_products);
  std::optional<EntryPointInfo> result_info;
  GetComparableProducts(info, &result_info);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_info->similar_candidate_products, comparable_products);
  ASSERT_EQ(result_info->title, kClusterTitle);

  UpdateUrlInfos(std::vector<GURL>{foo1, foo2});
  GetComparableProducts(info, &result_info);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(result_info->similar_candidate_products.size(), 2u);
  ASSERT_TRUE(base::Contains(result_info->similar_candidate_products, foo1));
  ASSERT_TRUE(base::Contains(result_info->similar_candidate_products, foo2));
  ASSERT_EQ(result_info->title, kClusterTitle);
}

TEST_F(ClusterManagerTest, GetComparableProductsWithPartialProductsComparable) {
  std::vector<uint64_t> product_ids{kProductID1, kProductID2, kProductID3};
  EXPECT_CALL(*server_proxy_, GetComparableProducts(product_ids, _))
      .WillRepeatedly(
          [](std::vector<uint64_t> ids,
             ClusterServerProxy::GetComparableProductsCallback callback) {
            std::move(callback).Run(
                std::vector<uint64_t>{kProductID1, kProductID2});
          });
  GURL foo1(kTestUrl1);
  GURL foo2(kTestUrl2);
  GURL foo3(kTestUrl3);
  std::map<GURL, uint64_t> comparable_products;
  comparable_products.emplace(foo1, kProductID1);
  comparable_products.emplace(foo2, kProductID2);
  comparable_products.emplace(foo3, kProductID3);
  UpdateUrlInfos(std::vector<GURL>{foo1, foo2, foo3});
  EntryPointInfo info(kClusterTitle, comparable_products);
  std::optional<EntryPointInfo> result_info;
  GetComparableProducts(info, &result_info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(result_info->similar_candidate_products.size(), 2u);
  ASSERT_TRUE(base::Contains(result_info->similar_candidate_products, foo1));
  ASSERT_TRUE(base::Contains(result_info->similar_candidate_products, foo2));
  ASSERT_EQ(result_info->title, kClusterTitle);
}

TEST_F(ClusterManagerTest, GetEntryPointInfoTitle_MostCommonBottomLabel) {
  GURL urlA = GURL("https://example.com/xbox");
  GURL urlB = GURL("https://example.com/ps5");
  UpdateUrlInfos(std::vector<GURL>{urlA, urlB});

  // Product A has three categories:
  // Gaming > Gaming Console
  // Gaming > XBox Console
  // Gaming > Game Console
  product_infos_[urlA] = CreateProductInfo("Gaming", kProductID1);
  auto* product_categories_A_1 =
      product_infos_[urlA].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_A_1, "Gaming Console");
  auto* product_categories_A_2 =
      product_infos_[urlA].category_data.add_product_categories();
  AddCategoryLabel(product_categories_A_2, "Gaming");
  AddCategoryLabel(product_categories_A_2, "XBox Console");
  auto* product_categories_A_3 =
      product_infos_[urlA].category_data.add_product_categories();
  AddCategoryLabel(product_categories_A_3, "Gaming");
  AddCategoryLabel(product_categories_A_3, "Game Console");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlA);

  // Product B has three categories:
  // Gaming > Gaming Console
  // Gaming > PS5 Console
  // Gaming > Game Console
  product_infos_[urlB] = CreateProductInfo("Gaming", kProductID2);
  auto* product_categories_B_1 =
      product_infos_[urlB].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_B_1, "Gaming Console");
  auto* product_categories_B_2 =
      product_infos_[urlB].category_data.add_product_categories();
  AddCategoryLabel(product_categories_B_2, "Gaming");
  AddCategoryLabel(product_categories_B_2, "PS5 Console");
  auto* product_categories_B_3 =
      product_infos_[urlB].category_data.add_product_categories();
  AddCategoryLabel(product_categories_B_3, "Gaming");
  AddCategoryLabel(product_categories_B_3, "Game Console");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlB);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  // Bottom label shared by all products will be picked. In a tie, pick the
  // shorter one.
  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForSelection(urlA, urlB, &info);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Game Console");
}

TEST_F(ClusterManagerTest, GetEntryPointInfoTitle_MostCommonLabel) {
  GURL urlA = GURL("https://example.com/chair1");
  GURL urlB = GURL("https://example.com/chair2");
  GURL urlC = GURL("https://example.com/chair3");
  UpdateUrlInfos(std::vector<GURL>{urlA, urlB, urlC});

  // Product A has one category:
  // Furniture > Chair
  product_infos_[urlA] = CreateProductInfo("Furniture", kProductID1);
  auto* product_categories_A =
      product_infos_[urlA].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_A, "Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlA);

  // Product B has one category:
  // Chair > Office Chair
  product_infos_[urlB] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_B =
      product_infos_[urlB].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_B, "Office Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlB);

  // Product C has one category:
  // Chair > Office Chair
  product_infos_[urlC] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_C =
      product_infos_[urlC].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_C, "Office Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlC);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  // Chair gets picked because it's considered a bottom label as long as it's a
  // bottom label in at least one product.
  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(urlA, &info);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Chair");
}

TEST_F(ClusterManagerTest, GetEntryPointInfoTitle_SecondToBottomLabel) {
  GURL urlA = GURL("https://example.com/chair1");
  GURL urlB = GURL("https://example.com/chair2");
  GURL urlC = GURL("https://example.com/chair3");
  UpdateUrlInfos(std::vector<GURL>{urlA, urlB, urlC});

  // Product A has two categories:
  // Chair > Ergonomic Chair
  // Chair > Office Chair
  product_infos_[urlA] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_A_1 =
      product_infos_[urlA].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_A_1, "Ergonomic Chair");
  auto* product_categories_A_2 =
      product_infos_[urlA].category_data.add_product_categories();
  AddCategoryLabel(product_categories_A_2, "Chair");
  AddCategoryLabel(product_categories_A_2, "Office Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlA);

  // Product B has one category:
  // Chair > Ergonomic Chair
  product_infos_[urlB] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_B =
      product_infos_[urlB].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_B, "Ergonomic Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlB);

  // Product C has one category:
  // Chair > Office Chair
  product_infos_[urlC] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_C =
      product_infos_[urlC].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_C, "Office Chair");
  cluster_manager_->DidNavigatePrimaryMainFrame(urlC);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, GetCandidateProductMap()->size());

  // There is no bottom label shared by all products, so the second-to-bottom
  // label that is shared by all products will be picked.
  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForNavigation(urlA, &info);
  ASSERT_TRUE(info);
  ASSERT_EQ(info->title, "Chair");
}

TEST_F(ClusterManagerTest, NoClusterForNonTriggerCategory) {
  GURL urlA = GURL("https://example.com/chair1");
  GURL urlB = GURL("https://example.com/chair2");
  UpdateUrlInfos(std::vector<GURL>{urlA, urlB});

  // Product A and B have the same category: Chair > Office Chair, and Office
  // Chair should not trigger clustering.
  product_infos_[urlA] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_A =
      product_infos_[urlA].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_A, "Office Chair", "", false);
  cluster_manager_->DidNavigatePrimaryMainFrame(urlA);

  product_infos_[urlB] = CreateProductInfo("Chair", kProductID1);
  auto* product_categories_B =
      product_infos_[urlB].category_data.mutable_product_categories(0);
  AddCategoryLabel(product_categories_B, "Office Chair", "", false);
  cluster_manager_->DidNavigatePrimaryMainFrame(urlB);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, GetCandidateProductMap()->size());

  std::optional<EntryPointInfo> info;
  GetEntryPointInfoForSelection(urlA, urlB, &info);
  ASSERT_FALSE(info);
}

}  // namespace commerce
