// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/test_support/persistence/unittest_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

class MockSiteDataStore : public testing::NoopSiteDataStore {
 public:
  MockSiteDataStore() = default;

  MockSiteDataStore(const MockSiteDataStore&) = delete;
  MockSiteDataStore& operator=(const MockSiteDataStore&) = delete;

  ~MockSiteDataStore() override = default;

  MOCK_METHOD(void,
              ReadSiteDataFromStore,
              (const url::Origin&,
               SiteDataStore::ReadSiteDataFromStoreCallback),
              (override));

  MOCK_METHOD(void,
              WriteSiteDataIntoStore,
              (const url::Origin&, const SiteDataProto&),
              (override));
};

void InitializeSiteDataProto(SiteDataProto* site_data) {
  DCHECK(site_data);
  site_data->set_last_loaded(42);

  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_observation_duration(0U);
  used_feature_proto.set_use_timestamp(1U);

  site_data->mutable_updates_favicon_in_background()->CopyFrom(
      used_feature_proto);
  site_data->mutable_updates_title_in_background()->CopyFrom(
      used_feature_proto);
  site_data->mutable_uses_audio_in_background()->CopyFrom(used_feature_proto);

  DCHECK(site_data->IsInitialized());
}

}  // namespace

class SiteDataReaderTest : public ::testing::Test {
 public:
  SiteDataReaderTest(const SiteDataReaderTest&) = delete;
  SiteDataReaderTest& operator=(const SiteDataReaderTest&) = delete;

 protected:
  // The constructors needs to call 'new' directly rather than using the
  // base::MakeRefCounted helper function because the constructor of
  // SiteDataImpl is protected and not visible to
  // base::MakeRefCounted.
  SiteDataReaderTest() {
    test_impl_ = base::WrapRefCounted(
        new internal::SiteDataImpl(url::Origin::Create(GURL("foo.com")),
                                   delegate_.GetWeakPtr(), &data_store_));
    test_impl_->NotifySiteLoaded();
    test_impl_->NotifyLoadedSiteBackgrounded();
    SiteDataReader* reader = new SiteDataReaderImpl(test_impl_.get());
    reader_ = base::WrapUnique(reader);
  }

  ~SiteDataReaderTest() override {
    test_impl_->NotifySiteUnloaded(
        performance_manager::TabVisibility::kBackground);
  }
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // created by this class, NiceMock is used to avoid having to set expectations
  // in test cases that don't care about this.
  ::testing::NiceMock<testing::MockSiteDataImplOnDestroyDelegate> delegate_;

  testing::NoopSiteDataStore data_store_;

  // The SiteDataImpl object used in these tests.
  scoped_refptr<internal::SiteDataImpl> test_impl_;

  // A SiteDataReader object associated with the origin used
  // to create this object.
  std::unique_ptr<SiteDataReader> reader_;
};

TEST_F(SiteDataReaderTest, TestAccessors) {
  // Initially we have no information about any of the features.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader_->UsesAudioInBackground());

  // Simulates a title update event, make sure it gets reported directly.
  test_impl_->NotifyUpdatesTitleInBackground();

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader_->UpdatesTitleInBackground());

  // Advance the clock by a large amount of time, enough for the unused features
  // observation windows to expire.
  AdvanceClock(base::Days(31));

  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            reader_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureNotInUse,
            reader_->UsesAudioInBackground());
}

TEST_F(SiteDataReaderTest, FreeingReaderDoesntCauseWriteOperation) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockSiteDataStore> data_store;

  // Override the read callback to simulate a successful read from the
  // data store.
  SiteDataProto proto = {};
  InitializeSiteDataProto(&proto);
  auto read_from_store_mock_impl =
      [&](const url::Origin& origin,
          SiteDataStore::ReadSiteDataFromStoreCallback callback) {
        std::move(callback).Run(std::optional<SiteDataProto>(proto));
      };

  EXPECT_CALL(data_store,
              ReadSiteDataFromStore(::testing::Property(&url::Origin::Serialize,
                                                        kOrigin.Serialize()),
                                    ::testing::_))
      .WillOnce(::testing::Invoke(read_from_store_mock_impl));

  scoped_refptr<internal::SiteDataImpl> impl(
      base::WrapRefCounted(new internal::SiteDataImpl(
          kOrigin, delegate_.GetWeakPtr(), &data_store)));
  std::unique_ptr<SiteDataReader> reader =
      base::WrapUnique(new SiteDataReaderImpl(impl));
  ::testing::Mock::VerifyAndClear(&data_store);

  EXPECT_TRUE(impl->fully_initialized_for_testing());

  // Resetting the reader shouldn't cause any write operation to the data store.
  EXPECT_CALL(data_store, WriteSiteDataIntoStore(::testing::_, ::testing::_))
      .Times(0);
  reader.reset();
  ::testing::Mock::VerifyAndClear(&data_store);
}

TEST_F(SiteDataReaderTest, OnDataLoadedCallbackInvoked) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockSiteDataStore> data_store;

  // Create the impl.
  EXPECT_CALL(data_store,
              ReadSiteDataFromStore(::testing::Property(&url::Origin::Serialize,
                                                        kOrigin.Serialize()),
                                    ::testing::_));
  scoped_refptr<internal::SiteDataImpl> impl = base::WrapRefCounted(
      new internal::SiteDataImpl(kOrigin, delegate_.GetWeakPtr(), &data_store));

  // Create the reader.
  std::unique_ptr<SiteDataReader> reader =
      base::WrapUnique(new SiteDataReaderImpl(impl));
  EXPECT_FALSE(reader->DataLoaded());

  // Register a data ready closure.
  bool on_data_loaded = false;
  reader->RegisterDataLoadedCallback(base::BindLambdaForTesting(
      [&on_data_loaded]() { on_data_loaded = true; }));

  // Transition the impl to fully initialized, which should cause the callbacks
  // to fire.
  EXPECT_FALSE(impl->DataLoaded());
  EXPECT_FALSE(on_data_loaded);
  impl->TransitionToFullyInitialized();
  EXPECT_TRUE(impl->DataLoaded());
  EXPECT_TRUE(on_data_loaded);
}

TEST_F(SiteDataReaderTest, DestroyingReaderCancelsPendingCallbacks) {
  const url::Origin kOrigin = url::Origin::Create(GURL("foo.com"));
  ::testing::StrictMock<MockSiteDataStore> data_store;

  // Create the impl.
  EXPECT_CALL(data_store,
              ReadSiteDataFromStore(::testing::Property(&url::Origin::Serialize,
                                                        kOrigin.Serialize()),
                                    ::testing::_));
  scoped_refptr<internal::SiteDataImpl> impl = base::WrapRefCounted(
      new internal::SiteDataImpl(kOrigin, delegate_.GetWeakPtr(), &data_store));

  // Create the reader.
  std::unique_ptr<SiteDataReader> reader =
      base::WrapUnique(new SiteDataReaderImpl(impl));
  EXPECT_FALSE(reader->DataLoaded());

  // Register a data ready closure.
  reader->RegisterDataLoadedCallback(
      base::MakeExpectedNotRunClosure(FROM_HERE));

  // Reset the reader.
  reader.reset();

  // Transition the impl to fully initialized, which should cause the callbacks
  // to fire. The reader's callback should *not* be invoked.
  EXPECT_FALSE(impl->DataLoaded());
  impl->TransitionToFullyInitialized();
  EXPECT_TRUE(impl->DataLoaded());
}

}  // namespace performance_manager
