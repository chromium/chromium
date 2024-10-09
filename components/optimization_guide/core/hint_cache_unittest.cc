// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hint_cache.h"

#include <optional>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace {

std::string GetHostDomainOrg(int index) {
  return "host.domain" + base::NumberToString(index) + ".org";
}

class HintCacheTest : public ProtoDatabaseProviderTestBase,
                      public testing::WithParamInterface<bool> {
 public:
  HintCacheTest() : loaded_hint_(nullptr) {}

  HintCacheTest(const HintCacheTest&) = delete;
  HintCacheTest& operator=(const HintCacheTest&) = delete;

  ~HintCacheTest() override = default;

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
    optimization_guide_store_ =
        IsBackedByPersistentStore()
            ? std::make_unique<OptimizationGuideStore>(
                  db_provider_.get(), database_path, database_task_runner,
                  /*pref_service_=*/nullptr)
            : nullptr;
    hint_cache_ = std::make_unique<HintCache>(
        optimization_guide_store_ ? optimization_guide_store_->AsWeakPtr()
                                  : nullptr,
        memory_cache_size);
    is_store_initialized_ = false;
    hint_cache_->Initialize(purge_existing_data,
                            base::BindOnce(&HintCacheTest::OnStoreInitialized,
                                           base::Unretained(this)));
    while (!is_store_initialized_) {
      RunUntilIdle();
    }
    hint_cache_->SetClockForTesting(task_environment_.GetMockClock());
  }

  void DestroyHintCache() {
    loaded_hint_ = nullptr;
    hint_cache_.reset();
    optimization_guide_store_.reset();
    is_store_initialized_ = false;
    are_component_hints_updated_ = false;
    on_load_hint_callback_called_ = false;
    are_fetched_hints_updated_ = false;

    RunUntilIdle();
  }

  void ResetLoadedHint() { loaded_hint_ = nullptr; }

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
      base::Time stored_time,
      const base::flat_set<std::string>& hosts_fetched,
      const base::flat_set<GURL>& urls_fetched) {
    are_fetched_hints_updated_ = false;
    hint_cache_->UpdateFetchedHints(
        std::move(get_hints_response), stored_time, hosts_fetched, urls_fetched,
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

  proto::Hint CreateHintForURL(
      const GURL& url,
      std::optional<int> cache_duration_in_secs = std::nullopt) {
    proto::Hint hint;
    hint.set_key(url.spec());
    hint.set_key_representation(proto::FULL_URL);
    if (cache_duration_in_secs)
      hint.mutable_max_cache_duration()->set_seconds(*cache_duration_in_secs);
    proto::PageHint* page_hint = hint.add_page_hints();
    page_hint->add_allowlisted_optimizations()->set_optimization_type(
        optimization_guide::proto::PERFORMANCE_HINTS);
    page_hint->set_page_pattern("whatever/*");

    return hint;
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  bool IsBackedByPersistentStore() const { return GetParam(); }

 private:
  void OnStoreInitialized() { is_store_initialized_ = true; }
  void OnUpdateComponentHints() { are_component_hints_updated_ = true; }
  void OnLoadHint(const proto::Hint* hint) {
    on_load_hint_callback_called_ = true;
    loaded_hint_ = hint;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<OptimizationGuideStore> optimization_guide_store_;
  std::unique_ptr<HintCache> hint_cache_;
  raw_ptr<const proto::Hint> loaded_hint_;

  bool is_store_initialized_;
  bool are_component_hints_updated_;
  bool on_load_hint_callback_called_;
  bool are_fetched_hints_updated_;
};

INSTANTIATE_TEST_SUITE_P(WithPersistentStore,
                         HintCacheTest,
                         testing::Values(true, false));

TEST_P(HintCacheTest, ComponentUpdate) {
  if (!IsBackedByPersistentStore())
    return;

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(proto::HOST);
  proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(proto::HOST);
  proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(proto::HOST);

  update_data->MoveHintIntoUpdateData(std::move(hint1));
  update_data->MoveHintIntoUpdateData(std::move(hint2));
  update_data->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentHints(std::move(update_data));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subdomain.domain.org"));
}

TEST_P(HintCacheTest, ComponentUpdateWithSameVersionIgnored) {
  if (!IsBackedByPersistentStore())
    return;

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->MaybeCreateUpdateDataForComponentHints(version));
}

TEST_P(HintCacheTest, ComponentUpdateWithEarlierVersionIgnored) {
  if (!IsBackedByPersistentStore())
    return;

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

TEST_P(HintCacheTest, ComponentUpdateWithLaterVersionProcessed) {
  if (!IsBackedByPersistentStore())
    return;

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version_1("1.0.0");
  base::Version version_2("2.0.0");

  std::unique_ptr<StoreUpdateData> update_data_1 =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version_1);
  ASSERT_TRUE(update_data_1);

  proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(proto::HOST);
  proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(proto::HOST);
  proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(proto::HOST);

  update_data_1->MoveHintIntoUpdateData(std::move(hint1));
  update_data_1->MoveHintIntoUpdateData(std::move(hint2));
  update_data_1->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentHints(std::move(update_data_1));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subdomain.domain.org"));

  std::unique_ptr<StoreUpdateData> update_data_2 =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version_2);
  ASSERT_TRUE(update_data_2);

  proto::Hint hint4;
  hint4.set_key("subdomain.domain2.org");
  hint4.set_key_representation(proto::HOST);
  proto::Hint hint5;
  hint5.set_key("host.domain2.org");
  hint5.set_key_representation(proto::HOST);
  proto::Hint hint6;
  hint6.set_key("otherhost.subdomain.domain2.org");
  hint6.set_key_representation(proto::HOST);

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
  EXPECT_TRUE(hint_cache()->HasHint("subdomain.domain2.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.domain2.org"));
}

TEST_P(HintCacheTest, ComponentHintsAvailableAfterRestart) {
  if (!IsBackedByPersistentStore())
    return;

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
      hint1.set_key_representation(proto::HOST);
      proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(proto::HOST);
      proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(proto::HOST);

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
    EXPECT_TRUE(hint_cache()->HasHint("host.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_P(HintCacheTest, ComponentHintsUpdatableAfterRestartWithPurge) {
  if (!IsBackedByPersistentStore())
    return;

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
    hint1.set_key_representation(proto::HOST);
    proto::Hint hint2;
    hint2.set_key("host.domain.org");
    hint2.set_key_representation(proto::HOST);
    proto::Hint hint3;
    hint3.set_key("otherhost.subdomain.domain.org");
    hint3.set_key_representation(proto::HOST);

    update_data->MoveHintIntoUpdateData(std::move(hint1));
    update_data->MoveHintIntoUpdateData(std::move(hint2));
    update_data->MoveHintIntoUpdateData(std::move(hint3));

    UpdateComponentHints(std::move(update_data));

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Matched
    EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("host.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_P(HintCacheTest, ComponentHintsNotRetainedAfterRestartWithPurge) {
  if (!IsBackedByPersistentStore())
    return;

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
      hint1.set_key_representation(proto::HOST);
      proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(proto::HOST);
      proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(proto::HOST);

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
    EXPECT_EQ(hint_cache()->HasHint("subdomain.domain.org"), should_match);
    EXPECT_EQ(hint_cache()->HasHint("host.domain.org"), should_match);

    DestroyHintCache();
  }
}

TEST_P(HintCacheTest, TestMemoryCacheLeastRecentlyUsedPurge) {
  if (!IsBackedByPersistentStore())
    return;

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
    hint.set_key_representation(proto::HOST);
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
      ASSERT_TRUE(hint_cache()->GetHostKeyedHintIfLoaded(host));
      EXPECT_EQ(GetHostDomainOrg(i),
                hint_cache()->GetHostKeyedHintIfLoaded(host)->key());
    } else {
      EXPECT_FALSE(hint_cache()->GetHostKeyedHintIfLoaded(host));
    }
    EXPECT_TRUE(hint_cache()->HasHint(host));
  }
}

TEST_P(HintCacheTest, TestHostNotInCache) {
  if (!IsBackedByPersistentStore())
    return;
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
    hint.set_key_representation(proto::HOST);
    update_data->MoveHintIntoUpdateData(std::move(hint));
  }

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->HasHint(GetHostDomainOrg(kTestHintCount)));
}

TEST_P(HintCacheTest, TestMemoryCacheLoadCallback) {
  if (!IsBackedByPersistentStore())
    return;

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->MaybeCreateUpdateDataForComponentHints(version);
  ASSERT_TRUE(update_data);

  std::string hint_key = "subdomain.domain.org";
  proto::Hint hint;
  hint.set_key(hint_key);
  hint.set_key_representation(proto::HOST);
  update_data->MoveHintIntoUpdateData(std::move(hint));

  UpdateComponentHints(std::move(update_data));

  EXPECT_FALSE(hint_cache()->GetHostKeyedHintIfLoaded("subdomain.domain.org"));
  LoadHint("subdomain.domain.org");
  EXPECT_TRUE(hint_cache()->GetHostKeyedHintIfLoaded("subdomain.domain.org"));

  EXPECT_TRUE(GetLoadedHint());
  EXPECT_EQ(hint_key, GetLoadedHint()->key());
}

TEST_P(HintCacheTest, StoreValidFetchedHints) {
  if (!IsBackedByPersistentStore()) {
    // Checking the fetched hints update time is not relevant when we don't have
    // a backing store.
    return;
  }

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key("host.domain.org");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.domain.org"}, {});
  EXPECT_TRUE(are_fetched_hints_updated());

  // Next update time for hints should be updated.
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
}

TEST_P(HintCacheTest, ParseEmptyFetchedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Time stored_time = base::Time().Now() + base::Days(1);
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.domain.org"}, {});
  // Empty Fetched Hints causes the metadata entry to be updated if store is
  // available.
  EXPECT_TRUE(are_fetched_hints_updated());

  if (IsBackedByPersistentStore()) {
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
  } else {
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());
    // Fetched hosts should still have an entry despite not getting a hint back
    // for it.
    EXPECT_TRUE(hint_cache()->HasHint("host.domain.org"));
  }
}

TEST_P(HintCacheTest, StoreValidFetchedHintsWithServerProvidedExpiryTime) {
  const int kMemoryCacheSize = 5;
  const int kFetchedHintExpirationSecs = 60;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  // Set server-provided expiration time.
  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key("host.domain.org");
  hint->mutable_max_cache_duration()->set_seconds(kFetchedHintExpirationSecs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  GURL navigation_url("https://foo.com");
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.domain.org"}, {navigation_url});
  EXPECT_TRUE(are_fetched_hints_updated());

  if (IsBackedByPersistentStore()) {
    // Next update time for hints should be updated.
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
  } else {
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());
  }

  // Should be loaded right when response is received.
  EXPECT_TRUE(hint_cache()->GetHostKeyedHintIfLoaded("host.domain.org"));

  // Set time so hint should be expired.
  MoveClockForwardBy(base::Seconds(kFetchedHintExpirationSecs + 1));
  EXPECT_FALSE(hint_cache()->GetHostKeyedHintIfLoaded("host.domain.org"));
}

TEST_P(HintCacheTest, StoreValidFetchedHintsWithDefaultExpiryTime) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  // Default update time for empty optimization guide store is base::Time().
  EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key("host.domain.org");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.domain.org"}, {});
  EXPECT_TRUE(are_fetched_hints_updated());

  if (IsBackedByPersistentStore()) {
    // Next update time for hints should be updated.
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), stored_time);
  } else {
    EXPECT_EQ(hint_cache()->GetFetchedHintsUpdateTime(), base::Time());
  }

  // Should be loaded right when response is received.
  EXPECT_TRUE(hint_cache()->GetHostKeyedHintIfLoaded("host.domain.org"));

  // Set time so hint should be expired.
  MoveClockForwardBy(
      optimization_guide::features::StoredFetchedHintsFreshnessDuration() +
      base::Seconds(1));
  EXPECT_FALSE(hint_cache()->GetHostKeyedHintIfLoaded("host.domain.org"));
}

TEST_P(HintCacheTest, CacheValidURLKeyedHint) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL url("https://whatever.com/r/werd");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(url);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  EXPECT_TRUE(hint_cache()->GetURLKeyedHint(url));
}

TEST_P(HintCacheTest, URLKeyedHintExpired) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL url("https://whatever.com/r/werd");
  int cache_duration_in_secs = 60;

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(url, cache_duration_in_secs);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  EXPECT_TRUE(hint_cache()->GetURLKeyedHint(url));

  MoveClockForwardBy(base::Seconds(cache_duration_in_secs + 1));
  EXPECT_FALSE(hint_cache()->GetURLKeyedHint(url));
}

TEST_P(HintCacheTest, PurgeExpiredFetchedHints) {
  if (!IsBackedByPersistentStore()) {
    // Purging expired fetched hints is only really relevant for when we have
    // a backing store.
    return;
  }

  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_TRUE(update_data);

  int cache_duration_in_secs = 60;

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  std::string host = "shouldpurge.com";
  proto::Hint* hint1 = get_hints_response->add_hints();
  hint1->set_key_representation(proto::HOST);
  hint1->set_key(host);
  hint1->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("page pattern");
  std::string host2 = "notpurged.com";
  proto::Hint* hint2 = get_hints_response->add_hints();
  hint2->set_key_representation(proto::HOST);
  hint2->set_key(host2);
  hint2->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs * 2);
  proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"shouldpurge.com", "notpurged.com"}, {});
  EXPECT_TRUE(are_fetched_hints_updated());
  EXPECT_TRUE(hint_cache()->HasHint("shouldpurge.com"));
  EXPECT_TRUE(hint_cache()->HasHint("notpurged.com"));

  MoveClockForwardBy(base::Seconds(cache_duration_in_secs + 1));

  hint_cache()->PurgeExpiredFetchedHints();
  RunUntilIdle();

  EXPECT_FALSE(hint_cache()->HasHint("shouldpurge.com"));
  EXPECT_TRUE(hint_cache()->HasHint("notpurged.com"));
}

TEST_P(HintCacheTest, ClearFetchedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL url("https://whatever.com/r/werd");
  int cache_duration_in_secs = 60;

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(url, cache_duration_in_secs);

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  std::string host = "host.com";
  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(host);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.com"}, {});
  EXPECT_TRUE(are_fetched_hints_updated());
  LoadHint(host);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  EXPECT_TRUE(hint_cache()->GetURLKeyedHint(url));
  EXPECT_TRUE(hint_cache()->GetHostKeyedHintIfLoaded(host));

  ResetLoadedHint();
  hint_cache()->ClearFetchedHints();
  EXPECT_FALSE(hint_cache()->GetURLKeyedHint(url));
  EXPECT_FALSE(hint_cache()->GetHostKeyedHintIfLoaded(host));
}

TEST_P(HintCacheTest, UnsupportedURLsForURLKeyedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL https_url("https://whatever.com/r/werd");
  GURL http_url("http://werd.com/werd/");
  GURL file_url("file://dog.png");
  GURL chrome_url("chrome://dog.png");
  GURL auth_url("https://username:password@www.example.com/");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(https_url);
  *(hints.Add()) = CreateHintForURL(http_url);
  *(hints.Add()) = CreateHintForURL(file_url);
  *(hints.Add()) = CreateHintForURL(chrome_url);
  *(hints.Add()) = CreateHintForURL(auth_url);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  EXPECT_TRUE(hint_cache()->GetURLKeyedHint(https_url));
  EXPECT_TRUE(hint_cache()->GetURLKeyedHint(http_url));
  EXPECT_FALSE(hint_cache()->GetURLKeyedHint(file_url));
  EXPECT_FALSE(hint_cache()->GetURLKeyedHint(chrome_url));
  EXPECT_FALSE(hint_cache()->GetURLKeyedHint(auth_url));
}

TEST_P(HintCacheTest, URLsWithNoURLKeyedHints) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL https_url_without_hint("https://whatever.com/r/nohint");
  GURL https_url_without_hint_has_fragment("https://whatever.com/r/nohint#123");
  GURL https_url_with_hint("https://whatever.com/r/hint");
  GURL https_url_unseen("https://unseen.com/new");
  GURL file_url("file://dog.png");
  GURL chrome_url("chrome://dog.png");
  GURL auth_url("https://username:password@www.example.com/");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(https_url_with_hint);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  // Add the url without hint to the url-keyed cache via UpdateFetchedHints.
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  std::string host = "host.com";
  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(host);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.com"}, {https_url_without_hint});

  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(https_url_with_hint));
  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(
      https_url_without_hint_has_fragment));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(file_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(chrome_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(auth_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(https_url_unseen));
}

TEST_P(HintCacheTest, ProcessHintsNoUpdateData) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  proto::Hint hint;
  hint.set_key("whatever.com");
  hint.set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint.add_page_hints();
  page_hint->set_page_pattern("foo.org/*/one/");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = hint;

  EXPECT_EQ(hint_cache()->ProcessAndCacheHints(&hints, nullptr),
            !IsBackedByPersistentStore());
}

TEST_P(HintCacheTest,
       ProcessHintsWithNoPageHintsOrAllowlistedOptimizationsAndUpdateData) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  proto::Hint hint;
  hint.set_key("whatever.com");
  hint.set_key_representation(proto::HOST);

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = hint;

  std::unique_ptr<StoreUpdateData> update_data =
      StoreUpdateData::CreateComponentStoreUpdateData(base::Version("1.0.0"));
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));
  if (IsBackedByPersistentStore()) {
    // Verify there is 1 store entries: 1 for the metadata entry.
    EXPECT_EQ(1ul, update_data->TakeUpdateEntries()->size());
  }
}

TEST_P(HintCacheTest,
       ProcessHintsWithNoPageHintsButHasAllowlistedOptimizationsAndUpdateData) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  proto::Hint hint;
  hint.set_key("whatever.com");
  hint.set_key_representation(proto::HOST);
  hint.add_allowlisted_optimizations()->set_optimization_type(
      optimization_guide::proto::DEFER_ALL_SCRIPT);

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = hint;

  std::unique_ptr<StoreUpdateData> update_data =
      StoreUpdateData::CreateComponentStoreUpdateData(base::Version("1.0.0"));
  EXPECT_TRUE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));
  if (IsBackedByPersistentStore()) {
    // Verify there is 1 store entries: 1 for the metadata entry plus the 1
    // added hint entry.
    EXPECT_EQ(2ul, update_data->TakeUpdateEntries()->size());
  }
}

TEST_P(HintCacheTest, ProcessHintsWithPageHintsAndUpdateData) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  google::protobuf::RepeatedPtrField<proto::Hint> hints;

  proto::Hint hint;
  hint.set_key("foo.org");
  hint.set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint.add_page_hints();
  page_hint->set_page_pattern("foo.org/*/one/");
  *(hints.Add()) = hint;

  proto::Hint no_page_hints_hint;
  no_page_hints_hint.set_key("nopagehints.com");
  no_page_hints_hint.set_key_representation(proto::HOST);
  *(hints.Add()) = no_page_hints_hint;

  std::unique_ptr<StoreUpdateData> update_data =
      StoreUpdateData::CreateComponentStoreUpdateData(base::Version("1.0.0"));
  EXPECT_TRUE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));
  if (IsBackedByPersistentStore()) {
    // Verify there are 2 store entries: 1 for the metadata entry plus
    // the 1 added hint entry.
    EXPECT_EQ(2ul, update_data->TakeUpdateEntries()->size());
  }
}

TEST_P(HintCacheTest, RemoveHintsForURLs) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  int cache_duration_in_secs = 60;
  std::string host = "host.com";
  GURL url("https://bar.com/r/baz");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  *(get_hints_response->add_hints()) =
      CreateHintForURL(url, cache_duration_in_secs);

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(host);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  UpdateFetchedHintsAndWait(std::move(get_hints_response), base::Time().Now(),
                            {host}, {url});
  EXPECT_TRUE(are_fetched_hints_updated());
  EXPECT_TRUE(hint_cache()->HasHint(host));
  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(url));

  hint_cache()->RemoveHintsForURLs({url, GURL(host)});
  EXPECT_TRUE(hint_cache()->HasHint(host));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_P(HintCacheTest, RemoveHintsForHosts) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  int cache_duration_in_secs = 60;
  std::string host = "host.com";
  GURL url("https://bar.com/r/baz");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  *(get_hints_response->add_hints()) =
      CreateHintForURL(url, cache_duration_in_secs);

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(host);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  UpdateFetchedHintsAndWait(std::move(get_hints_response), base::Time().Now(),
                            {host}, {url});
  EXPECT_TRUE(are_fetched_hints_updated());
  EXPECT_TRUE(hint_cache()->HasHint(host));
  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(url));

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hint_cache()->RemoveHintsForHosts(run_loop->QuitClosure(),
                                    {url.spec(), host});
  run_loop->Run();

  EXPECT_FALSE(hint_cache()->HasHint(host));
  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_P(HintCacheTest, URLsWithNoURLKeyedHintsFetchedURLWasFragment) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  std::unique_ptr<StoreUpdateData> update_data =
      hint_cache()->CreateUpdateDataForFetchedHints(base::Time());
  ASSERT_EQ(update_data != nullptr, IsBackedByPersistentStore());

  GURL https_url_without_hint("https://whatever.com/r/nohint");
  GURL https_url_without_hint_has_fragment("https://whatever.com/r/nohint#123");
  GURL https_url_unseen("https://unseen.com/new");
  GURL file_url("file://dog.png");
  GURL chrome_url("chrome://dog.png");
  GURL auth_url("https://username:password@www.example.com/");

  google::protobuf::RepeatedPtrField<proto::Hint> hints;
  *(hints.Add()) = CreateHintForURL(https_url_without_hint_has_fragment);

  // Only URL-keyed hint included so there are no hints to store within the
  // update data.
  EXPECT_FALSE(hint_cache()->ProcessAndCacheHints(
      &hints, IsBackedByPersistentStore() ? update_data.get() : nullptr));

  // Add the url without hint to the url-keyed cache via UpdateFetchedHints.
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  std::string host = "host.com";
  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(host);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("page pattern");

  base::Time stored_time = base::Time().Now();
  UpdateFetchedHintsAndWait(std::move(get_hints_response), stored_time,
                            {"host.com"},
                            {https_url_without_hint_has_fragment});

  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(https_url_without_hint));
  EXPECT_TRUE(hint_cache()->HasURLKeyedEntryForURL(
      https_url_without_hint_has_fragment));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(file_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(chrome_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(auth_url));
  EXPECT_FALSE(hint_cache()->HasURLKeyedEntryForURL(https_url_unseen));
}

}  // namespace

}  // namespace optimization_guide
