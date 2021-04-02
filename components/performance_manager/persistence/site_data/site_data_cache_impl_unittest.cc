// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"

#include <set>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/unittest_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

constexpr base::TimeDelta kDelay = base::TimeDelta::FromMinutes(1);

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
url::Origin TestOrigin1() {
  return url::Origin::Create(GURL("http://www.foo.com"));
}
url::Origin TestOrigin2() {
  return url::Origin::Create(GURL("http://www.bar.com"));
}

class MockSiteCache : public testing::NoopSiteDataStore {
 public:
  MockSiteCache() = default;
  ~MockSiteCache() = default;

  MOCK_METHOD1(RemoveSiteDataFromStore, void(const std::vector<url::Origin>&));
  MOCK_METHOD0(ClearStore, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSiteCache);
};

}  // namespace

class SiteDataCacheImplTest : public ::testing::Test {
 protected:
  SiteDataCacheImplTest()
      : data_cache_factory_(std::make_unique<SiteDataCacheFactory>()) {
    data_cache_ = std::make_unique<SiteDataCacheImpl>(
        browser_context_.UniqueId(), browser_context_.GetPath());
    mock_db_ = new ::testing::StrictMock<MockSiteCache>();
    data_cache_->SetDataStoreForTesting(base::WrapUnique(mock_db_));
    WaitForAsyncOperationsToComplete();
  }

  void TearDown() override { WaitForAsyncOperationsToComplete(); }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void WaitForAsyncOperationsToComplete() { task_environment_.RunUntilIdle(); }

  // Populates |writer_|, |reader_| and |data_| to refer to a tab navigated to
  // |TestOrigin1()| that updated its title in background. Populates |writer2_|,
  // |reader2_| and |data2_| to refer to a tab navigated to |TestOrigin2()| that
  // updates its favicon in background.
  void SetupTwoSitesUsingFeaturesInBackground() {
    // Load a first origin, and then make use of a feature on it.
    ASSERT_FALSE(reader_);
    reader_ = data_cache_->GetReaderForOrigin(TestOrigin1());
    EXPECT_TRUE(reader_);

    ASSERT_FALSE(writer_);
    writer_ = data_cache_->GetWriterForOrigin(TestOrigin1());
    EXPECT_TRUE(writer_);

    ASSERT_FALSE(data_);
    data_ =
        data_cache_->origin_data_map_for_testing().find(TestOrigin1())->second;
    EXPECT_TRUE(data_);

    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
              reader_->UpdatesTitleInBackground());
    writer_->NotifySiteLoaded(TabVisibility::kBackground);
    writer_->NotifyUpdatesTitleInBackground();
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
              reader_->UpdatesTitleInBackground());
    AdvanceClock(kDelay);

    // Load a second origin, make use of a feature on it too.
    ASSERT_FALSE(reader2_);
    reader2_ = data_cache_->GetReaderForOrigin(TestOrigin2());
    EXPECT_TRUE(reader2_);

    ASSERT_FALSE(writer2_);
    writer2_ = data_cache_->GetWriterForOrigin(TestOrigin2());
    EXPECT_TRUE(writer2_);

    ASSERT_FALSE(data2_);
    data2_ =
        data_cache_->origin_data_map_for_testing().find(TestOrigin2())->second;
    EXPECT_TRUE(data2_);

    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
              reader2_->UpdatesFaviconInBackground());
    writer2_->NotifySiteLoaded(TabVisibility::kBackground);
    writer2_->NotifyUpdatesFaviconInBackground();
    EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
              reader2_->UpdatesFaviconInBackground());
    AdvanceClock(kDelay);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserContext browser_context_;

  // Owned by |data_cache_|.
  ::testing::StrictMock<MockSiteCache>* mock_db_ = nullptr;
  std::unique_ptr<SiteDataCacheFactory> data_cache_factory_;
  std::unique_ptr<SiteDataCacheImpl> data_cache_;

  std::unique_ptr<SiteDataReader> reader_;
  std::unique_ptr<SiteDataWriter> writer_;
  internal::SiteDataImpl* data_ = nullptr;

  std::unique_ptr<SiteDataReader> reader2_;
  std::unique_ptr<SiteDataWriter> writer2_;
  internal::SiteDataImpl* data2_ = nullptr;
};

TEST_F(SiteDataCacheImplTest, EndToEnd) {
  auto reader = data_cache_->GetReaderForOrigin(TestOrigin1());
  EXPECT_TRUE(reader);
  auto writer = data_cache_->GetWriterForOrigin(TestOrigin1());
  EXPECT_TRUE(writer);

  EXPECT_EQ(1U, data_cache_->origin_data_map_for_testing().size());

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteLoaded(TabVisibility::kBackground);
  writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  writer->NotifySiteUnloaded(TabVisibility::kBackground);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  auto reader_copy = data_cache_->GetReaderForOrigin(TestOrigin1());
  EXPECT_EQ(1U, data_cache_->origin_data_map_for_testing().size());
  auto reader2 = data_cache_->GetReaderForOrigin(TestOrigin2());
  EXPECT_EQ(2U, data_cache_->origin_data_map_for_testing().size());
  reader2.reset();

  WaitForAsyncOperationsToComplete();
  EXPECT_EQ(1U, data_cache_->origin_data_map_for_testing().size());
  reader_copy.reset();

  reader.reset();
  writer.reset();
  EXPECT_TRUE(data_cache_->origin_data_map_for_testing().empty());

  EXPECT_CALL(*mock_db_, ClearStore());
  data_cache_->ClearAllSiteData();
}

TEST_F(SiteDataCacheImplTest, ClearSiteDataForOrigins) {
  SetupTwoSitesUsingFeaturesInBackground();

  const base::TimeDelta last_loaded_time2_before_urls_deleted =
      data2_->last_loaded_time_for_testing();

  // Make sure that all data passed to |ClearSiteDataForOrigins| get passed to
  // the database, even if they're not in the internal map used by the data
  // cache.
  const url::Origin kOriginNotInMap =
      url::Origin::Create(GURL("http://www.url-not-in-map.com"));
  std::vector<url::Origin> origins_to_remove = {TestOrigin1(), kOriginNotInMap};
  EXPECT_CALL(*mock_db_,
              RemoveSiteDataFromStore(::testing::WhenSorted(
                  ::testing::ElementsAre(TestOrigin1(), kOriginNotInMap))));
  data_cache_->ClearSiteDataForOrigins(origins_to_remove);
  ::testing::Mock::VerifyAndClear(mock_db_);

  // The information for the first site should have been cleared.
  EXPECT_GE((base::TimeTicks::Now() - base::TimeTicks::UnixEpoch()).InSeconds(),
            data_->last_loaded_time_for_testing().InSeconds());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  // The second site shouldn't have been cleared.
  EXPECT_EQ(data2_->last_loaded_time_for_testing(),
            last_loaded_time2_before_urls_deleted);
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader2_->UpdatesFaviconInBackground());

  writer_->NotifySiteUnloaded(TabVisibility::kBackground);
  writer2_->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataCacheImplTest, ClearAllSiteData) {
  SetupTwoSitesUsingFeaturesInBackground();

  // Delete all the information stored in the data store.
  EXPECT_CALL(*mock_db_, ClearStore());
  data_cache_->ClearAllSiteData();
  ::testing::Mock::VerifyAndClear(mock_db_);

  // The information for both sites should have been cleared.
  EXPECT_GE((base::TimeTicks::Now() - base::TimeTicks::UnixEpoch()).InSeconds(),
            data_->last_loaded_time_for_testing().InSeconds());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  EXPECT_GE((base::TimeTicks::Now() - base::TimeTicks::UnixEpoch()).InSeconds(),
            data2_->last_loaded_time_for_testing().InSeconds());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader2_->UpdatesFaviconInBackground());

  writer_->NotifySiteUnloaded(TabVisibility::kBackground);
  writer2_->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataCacheImplTest, InspectorWorks) {
  // Make sure the inspector interface was registered at construction.
  SiteDataCacheInspector* inspector =
      SiteDataCacheFactory::GetInstance()->GetInspectorForBrowserContext(
          browser_context_.UniqueId());
  EXPECT_NE(nullptr, inspector);
  EXPECT_EQ(data_cache_.get(), inspector);

  EXPECT_STREQ("SiteDataCache", inspector->GetDataCacheName());

  // We expect an empty data store at the outset.
  EXPECT_EQ(0U, inspector->GetAllInMemoryOrigins().size());
  std::unique_ptr<SiteDataProto> data;
  bool is_dirty = false;
  EXPECT_FALSE(inspector->GetDataForOrigin(TestOrigin1(), &is_dirty, &data));
  EXPECT_FALSE(is_dirty);
  EXPECT_EQ(nullptr, data.get());

  {
    // Add an entry, see that it's reflected in the inspector interface.
    auto writer = data_cache_->GetWriterForOrigin(TestOrigin1());

    EXPECT_EQ(1U, inspector->GetAllInMemoryOrigins().size());
    EXPECT_TRUE(inspector->GetDataForOrigin(TestOrigin1(), &is_dirty, &data));
    EXPECT_FALSE(is_dirty);
    ASSERT_NE(nullptr, data.get());

    // Touch the underlying data, see that the dirty bit updates.
    writer->NotifySiteLoaded(TabVisibility::kBackground);
    EXPECT_TRUE(inspector->GetDataForOrigin(TestOrigin1(), &is_dirty, &data));
    EXPECT_TRUE(is_dirty);
    writer->NotifySiteUnloaded(TabVisibility::kBackground);
  }

  // Make sure the interface is unregistered from the browser context on
  // destruction.
  data_cache_.reset();
  EXPECT_EQ(nullptr,
            SiteDataCacheFactory::GetInstance()->GetInspectorForBrowserContext(
                browser_context_.UniqueId()));
}

}  // namespace performance_manager
