// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/non_recording_site_data_cache.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

class NonRecordingSiteDataCacheTest : public testing::Test {
 public:
  NonRecordingSiteDataCacheTest()
      : use_in_memory_db_for_testing_(
            LevelDBSiteDataStore::UseInMemoryDBForTesting()),
        factory_(std::make_unique<SiteDataCacheFactory>()) {
    off_the_record_browser_context_.set_is_off_the_record(true);
  }

  ~NonRecordingSiteDataCacheTest() override = default;

  void SetUp() override {
    recording_data_cache_ = std::make_unique<SiteDataCacheImpl>(
        parent_browser_context_.UniqueId(), parent_browser_context_.GetPath());

    // Wait for the database to be initialized.
    base::RunLoop run_loop;
    recording_data_cache_->SetInitializationCallbackForTesting(
        run_loop.QuitClosure());
    run_loop.Run();

    non_recording_data_cache_ = std::make_unique<NonRecordingSiteDataCache>(
        off_the_record_browser_context_.UniqueId(), recording_data_cache_.get(),
        recording_data_cache_.get());
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("http://www.foo.com"));

  content::BrowserTaskEnvironment task_environment_;

  // Ensure that the database used by the data store owned by
  // |recording_data_cache_| gets created in memory.
  base::ScopedClosureRunner use_in_memory_db_for_testing_;

  // The data cache factory that will be used by the caches tested here.
  std::unique_ptr<SiteDataCacheFactory> factory_;

  // The on the record browser context.
  content::TestBrowserContext parent_browser_context_;
  // An off the record context.
  content::TestBrowserContext off_the_record_browser_context_;

  std::unique_ptr<SiteDataCacheImpl> recording_data_cache_;
  std::unique_ptr<NonRecordingSiteDataCache> non_recording_data_cache_;
};

}  // namespace

TEST_F(NonRecordingSiteDataCacheTest, EndToEnd) {
  // Ensures that the observation made via a writer created by the non
  // recording data cache aren't recorded.
  auto reader = non_recording_data_cache_->GetReaderForOrigin(kTestOrigin);
  EXPECT_TRUE(reader);
  auto fake_writer = non_recording_data_cache_->GetWriterForOrigin(kTestOrigin);
  EXPECT_TRUE(fake_writer);
  auto real_writer = recording_data_cache_->GetWriterForOrigin(kTestOrigin);
  EXPECT_TRUE(real_writer);

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  fake_writer->NotifySiteLoaded(TabVisibility::kBackground);
  fake_writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());

  real_writer->NotifySiteLoaded(TabVisibility::kBackground);
  real_writer->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  fake_writer->NotifySiteUnloaded(TabVisibility::kBackground);
  real_writer->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(NonRecordingSiteDataCacheTest, InspectorWorks) {
  // Make sure the inspector interface was registered at construction.
  SiteDataCacheInspector* inspector = factory_->GetInspectorForBrowserContext(
      off_the_record_browser_context_.UniqueId());
  EXPECT_NE(nullptr, inspector);
  EXPECT_EQ(non_recording_data_cache_.get(), inspector);

  EXPECT_STREQ("NonRecordingSiteDataCache", inspector->GetDataCacheName());

  // We expect an empty data cache at the outset.
  EXPECT_EQ(0U, inspector->GetAllInMemoryOrigins().size());
  std::unique_ptr<SiteDataProto> data;
  bool is_dirty = false;
  EXPECT_FALSE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
  EXPECT_FALSE(is_dirty);
  EXPECT_EQ(nullptr, data.get());

  {
    // Add an entry through the writing data cache, see that it's reflected in
    // the inspector interface.
    auto writer = recording_data_cache_->GetWriterForOrigin(kTestOrigin);

    EXPECT_EQ(1U, inspector->GetAllInMemoryOrigins().size());
    EXPECT_TRUE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
    EXPECT_FALSE(is_dirty);
    ASSERT_NE(nullptr, data.get());

    // Touch the underlying data, see that the dirty bit updates.
    writer->NotifySiteLoaded(TabVisibility::kBackground);
    EXPECT_TRUE(inspector->GetDataForOrigin(kTestOrigin, &is_dirty, &data));
    writer->NotifySiteUnloaded(TabVisibility::kBackground);
  }

  // Make sure the interface is unregistered from the browser context on
  // destruction.
  non_recording_data_cache_.reset();
  EXPECT_EQ(nullptr, factory_->GetInspectorForBrowserContext(
                         off_the_record_browser_context_.UniqueId()));
}

}  // namespace performance_manager
