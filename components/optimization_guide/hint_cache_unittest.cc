// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_cache.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace {

std::string GetHostDomainOrg(int index) {
  return "host.domain" + base::NumberToString(index) + ".org";
}

class HintCacheTest : public ProtoDatabaseProviderTestBase {
 public:
  HintCacheTest() : loaded_hint_(nullptr) {}

  ~HintCacheTest() override {}

  void SetUp() override { ProtoDatabaseProviderTestBase::SetUp(); }

  void TearDown() override {
    ProtoDatabaseProviderTestBase::TearDown();
    DestroyHintCache();
  }

 protected:
  // Creates and initializes the hint cache and optimization guide store and
  // waits for the callback indicating that initialization is complete.
  void CreateAndInitializeHintCache(int memory_cache_size,
                                    bool purge_existing_data = false) {
    auto database_path = temp_dir_.GetPath();
    auto database_task_runner = task_environment_.GetMainThreadTaskRunner();
    hint_cache_ = std::make_unique<HintCache>(
        std::make_unique<OptimizationGuideStore>(
            db_provider_.get(), database_path, database_task_runner),
        memory_cache_size);
    is_store_initialized_ = false;
    hint_cache_->Initialize(purge_existing_data,
                            base::BindOnce(&HintCacheTest::OnStoreInitialized,
                                           base::Unretained(this)));
    while (!is_store_initialized_) {
      RunUntilIdle();
    }
  }

  void DestroyHintCache() {
    hint_cache_.reset();
    loaded_hint_ = nullptr;
    is_store_initialized_ = false;
    are_component_hints_updated_ = false;
    on_load_hint_callback_called_ = false;
    are_fetched_hints_updated_ = false;

    RunUntilIdle();
  }

  HintCache* hint_cache() { return hint_cache_.get(); }

  bool are_fetched_hints_updated() { return are_fetched_hints_updated_; }

  // Updates the cache with |component_data| and waits for callback indicating
  // that the update is complete.
  void UpdateComponentHints(std::unique_ptr<StoreUpdateData> component_data) {
    are_component_hints_updated_ = false;
    hint_cache_->UpdateComponentHints(
        std::move(component_data),
        base::BindOnce(&HintCacheTest::OnUpdateComponentHints,
                       base::Unretained(this)));
    while (!are_component_hints_updated_) {
      RunUntilIdle();
    }
  }

  void UpdateFetchedHintsAndWait(
      std::unique_ptr<proto::GetHintsResponse> get_hints_response,
      base::Time stored_time) {
    are_fetched_hints_updated_ = false;
    hint_cache_->UpdateFetchedHints(
        std::move(get_hints_response), stored_time,
        base::BindOnce(&HintCacheTest::OnHintsUpdated, base::Unretained(this)));

    while (!are_fetched_hints_updated_)
      RunUntilIdle();
  }

  void OnHintsUpdated() { are_fetched_hints_updated_ = true; }

  // Loads hint for the specified host from the cache and waits for callback
  // indicating that loading the hint is complete.
  void LoadHint(const std::string& host) {
    on_load_hint_callback_called_ = false;
    loaded_hint_ = nullptr;
    hint_cache_->LoadHint(host, base::BindOnce(&HintCacheTest::OnLoadHint,
                                               base::Unretained(this)));
    while (!on_load_hint_callback_called_) {
      RunUntilIdle();
    }
  }

  const proto::Hint* GetLoadedHint() const { return loaded_hint_; }

 private:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void OnStoreInitialized() { is_store_initialized_ = true; }
  void OnUpdateComponentHints() { are_component_hints_updated_ = true; }
  void OnLoadHint(const proto::Hint* hint) {
    on_load_hint_callback_called_ = true;
    loaded_hint_ = hint;
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<HintCache> hint_cache_;
  const proto::Hint* loaded_hint_;

  bool is_store_initialized_;
  bool are_component_hints_updated_;
  bool on_load_hint_callback_called_;
  bool are_fetched_hints_updated_;

  DISALLOW_COPY_AND_ASSIGN(HintCacheTest);
};

TEST_F(HintCacheTest, ComponentUpdate) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(proto::HOST_SUFFIX);

  update_data->MoveHintIntoUpdateData(std::move(hint1));
  update_data->MoveHintIntoUpdateData(std::move(hint2));
  update_data->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentHints(std::move(update_data));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));
}

TEST_F(HintCacheTest, ComponentUpdateWithSameVersionIgnored) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->MaybeCreateUpdateDataForComponentHints(version));
}

TEST_F(HintCacheTest, ComponentUpdateWithEarlierVersionIgnored) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version_1("1.0.0");
  base::Version version_2("2.0.0");

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version_2);
  ASSERT_TRUE(update_data);

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->MaybeCreateUpdateDataForComponentHints(version_1));
}

TEST_F(HintCacheTest, ComponentUpdateWithLaterVersionProcessed) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version_1("1.0.0");
  base::Version version_2("2.0.0");

  std::unique_ptr<StoreUpdateData> update_data_1 =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version_1);
  ASSERT_TRUE(update_data_1);

  proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(proto::HOST_SUFFIX);

  update_data_1->MoveHintIntoUpdateData(std::move(hint1));
  update_data_1->MoveHintIntoUpdateData(std::move(hint2));
  update_data_1->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentHints(std::move(update_data_1));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

  std::unique_ptr<StoreUpdateData> update_data_2 =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version_2);
  ASSERT_TRUE(update_data_2);

  proto::Hint hint4;
  hint4.set_key("subdomain.domain2.org");
  hint4.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint5;
  hint5.set_key("host.domain2.org");
  hint5.set_key_representation(proto::HOST_SUFFIX);
  proto::Hint hint6;
  hint6.set_key("otherhost.subdomain.domain2.org");
  hint6.set_key_representation(proto::HOST_SUFFIX);

  update_data_2->MoveHintIntoUpdateData(std::move(hint4));
  update_data_2->MoveHintIntoUpdateData(std::move(hint5));
  update_data_2->MoveHintIntoUpdateData(std::move(hint6));

  UpdateComponentHints(std::move(update_data_2));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("domain2.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain2.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain2.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain2.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain2.org"));
}

TEST_F(HintCacheTest, ComponentHintsAvailableAfterRestart) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 false /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<StoreUpdateData> update_data =
        hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
    if (i == 0) {
      ASSERT_TRUE(update_data);

      proto::Hint hint1;
      hint1.set_key("subdomain.domain.org");
      hint1.set_key_representation(proto::HOST_SUFFIX);
      proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(proto::HOST_SUFFIX);
      proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(proto::HOST_SUFFIX);

      update_data->MoveHintIntoUpdateData(std::move(hint1));
      update_data->MoveHintIntoUpdateData(std::move(hint2));
      update_data->MoveHintIntoUpdateData(std::move(hint3));

      UpdateComponentHints(std::move(update_data));
    } else {
      EXPECT_FALSE(update_data);
    }

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Matched
    EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, ComponentHintsUpdatableAfterRestartWithPurge) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 true /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<StoreUpdateData> update_data =
        hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
    ASSERT_TRUE(update_data);

    proto::Hint hint1;
    hint1.set_key("subdomain.domain.org");
    hint1.set_key_representation(proto::HOST_SUFFIX);
    proto::Hint hint2;
    hint2.set_key("host.domain.org");
    hint2.set_key_representation(proto::HOST_SUFFIX);
    proto::Hint hint3;
    hint3.set_key("otherhost.subdomain.domain.org");
    hint3.set_key_representation(proto::HOST_SUFFIX);

    update_data->MoveHintIntoUpdateData(std::move(hint1));
    update_data->MoveHintIntoUpdateData(std::move(hint2));
    update_data->MoveHintIntoUpdateData(std::move(hint3));

    UpdateComponentHints(std::move(update_data));

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Matched
    EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, ComponentHintsNotRetainedAfterRestartWithPurge) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 true /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<StoreUpdateData> update_data =
        hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
    if (i == 0) {
      ASSERT_TRUE(update_data);

      proto::Hint hint1;
      hint1.set_key("subdomain.domain.org");
      hint1.set_key_representation(proto::HOST_SUFFIX);
      proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(proto::HOST_SUFFIX);
      proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(proto::HOST_SUFFIX);

      update_data->MoveHintIntoUpdateData(std::move(hint1));
      update_data->MoveHintIntoUpdateData(std::move(hint2));
      update_data->MoveHintIntoUpdateData(std::move(hint3));

      UpdateComponentHints(std::move(update_data));
    } else {
      EXPECT_TRUE(update_data);
    }

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Maybe matched
    bool should_match = (i == 0);
    EXPECT_EQ(hint_cache()->HasHint("otherhost.subdomain.domain.org"),
              should_match);
    EXPECT_EQ(hint_cache()->HasHint("host.subdomain.domain.org"), should_match);
    EXPECT_EQ(hint_cache()->HasHint("subhost.host.subdomain.domain.org"),
              should_match);

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, TestMemoryCacheLeastRecentlyUsedPurge) {
  const int kTestHintCount = 10;
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  for (int i = 0; i < kTestHintCount; ++i) {
    proto::Hint hint;
    hint.set_key(GetHostDomainOrg(i));
    hint.set_key_representation(proto::HOST_SUFFIX);
    update_data->MoveHintIntoUpdateData(std::move(hint));
  }

  UpdateComponentHints(std::move(update_data));

  for (int i = kTestHintCount - 1; i >= 0; --i) {
    std::string host = GetHostDomainOrg(i);
    EXPECT_TRUE(hint_cache()->HasHint(host));
    LoadHint(host);
    ASSERT_TRUE(GetLoadedHint());
    EXPECT_EQ(GetLoadedHint()->key(), host);
  }

  for (int i = 0; i < kTestHintCount; ++i) {
    std::string host = GetHostDomainOrg(i);
    if (i < kMemoryCacheSize) {
      ASSERT_TRUE(hint_cache()->GetHintIfLoaded(host));
      EXPECT_EQ(GetHostDomainOrg(i),
                hint_cache()->GetHintIfLoaded(host)->key());
    } else {
      EXPECT_FALSE(hint_cache()->GetHintIfLoaded(host));
    }
    EXPECT_TRUE(hint_cache()->HasHint(host));
  }
}

TEST_F(HintCacheTest, TestHostNotInCache) {
  const int kTestHintCount = 10;
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  for (int i = 0; i < kTestHintCount; ++i) {
    proto::Hint hint;
    hint.set_key(GetHostDomainOrg(i));
    hint.set_key_representation(proto::HOST_SUFFIX);
    update_data->MoveHintIntoUpdateData(std::move(hint));
  }

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->HasHint(GetHostDomainOrg(kTestHintCount)));
}

TEST_F(HintCacheTest, TestMemoryCacheLoadCallback) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  std::string hint_key = "subdomain.domain.org";
  proto::Hint hint;
  hint.set_key(hint_key);
  hint.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint));

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->GetHintIfLoaded("host.subdomain.domain.org"));
  LoadHint("host.subdomain.domain.org");
  EXPECT_TRUE(hint_cache()->GetHintIfLoaded("host.subdomain.domain.org"));

  EXPECT_TRUE(GetLoadedHint());
  EXPECT_EQ(hint_key, GetLoadedHint()->key());
}

TEST_F(HintCacheTest, StoreValidFetchedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST_SUFFIX);
  hint->set_key("host.domain.org");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time);
  EXPECT_TRUE(are_fetched_hints_updated());

  // Next update time for hints should be updated.
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
}

TEST_F(HintCacheTest, ParseEmptyFetchedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Time stored_time = base::Time().Now() + base::TimeDelta().FromDays(1);
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time);
  // Empty Fetched Hints causes the metadata entry to be updated.
  EXPECT_TRUE(are_fetched_hints_updated());
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
}

TEST_F(HintCacheTest, StoreValidFetchedHintsWithServerProvidedExpiryTime) {
  base::HistogramTester histogram_tester;
  const int kMemoryCacheSize = 5;
  const int kFetchedHintExpirationSecs = 60;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  // Set server-provided expiration time.
  get_hints_response->mutable_max_cache_duration()->set_seconds(
      kFetchedHintExpirationSecs);

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST_SUFFIX);
  hint->set_key("host.domain.org");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time);
  EXPECT_TRUE(are_fetched_hints_updated());

  // Next update time for hints should be updated.
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);

  LoadHint("host.domain.org");
  // HISTOGRAM TEST!
  histogram_tester.ExpectTimeBucketCount(
      "OptimizationGuide.HintCache.FetchedHint.TimeToExpiration",
      base::TimeDelta::FromSeconds(kFetchedHintExpirationSecs), 1);
}

TEST_F(HintCacheTest, StoreValidFetchedHintsWithDefaultExpiryTime) {
  base::HistogramTester histogram_tester;
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST_SUFFIX);
  hint->set_key("host.domain.org");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time);
  EXPECT_TRUE(are_fetched_hints_updated());

  // Next update time for hints should be updated.
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);

  LoadHint("host.domain.org");
  histogram_tester.ExpectTimeBucketCount(
      "OptimizationGuide.HintCache.FetchedHint.TimeToExpiration",
      optimization_guide::features::StoredFetchedHintsFreshnessDuration(), 1);
}

}  // namespace

}  // namespace optimization_guide
