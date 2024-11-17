// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_impl.h"

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "components/performance_manager/test_support/persistence/unittest_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {
namespace internal {

namespace {

class TestSiteDataImpl : public SiteDataImpl {
 public:
  using SiteDataImpl::FeatureObservationDuration;
  using SiteDataImpl::OnDestroyDelegate;
  using SiteDataImpl::site_characteristics_for_testing;
  using SiteDataImpl::TimeDeltaToInternalRepresentation;

  explicit TestSiteDataImpl(
      const url::Origin& origin,
      base::WeakPtr<SiteDataImpl::OnDestroyDelegate> delegate,
      SiteDataStore* data_store)
      : SiteDataImpl(origin, delegate, data_store) {}

  base::TimeDelta FeatureObservationTimestamp(
      const SiteDataFeatureProto& feature_proto) {
    return InternalRepresentationToTimeDelta(feature_proto.use_timestamp());
  }

 protected:
  ~TestSiteDataImpl() override = default;
};

class MockDataStore : public testing::NoopSiteDataStore {
 public:
  MockDataStore() = default;

  MockDataStore(const MockDataStore&) = delete;
  MockDataStore& operator=(const MockDataStore&) = delete;

  ~MockDataStore() override = default;

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

// Returns a SiteDataFeatureProto that indicates that a feature hasn't been
// used.
SiteDataFeatureProto GetUnusedFeatureProto() {
  SiteDataFeatureProto unused_feature_proto;
  unused_feature_proto.set_observation_duration(1U);
  unused_feature_proto.set_use_timestamp(0U);
  return unused_feature_proto;
}

// Returns a SiteDataFeatureProto that indicates that a feature has been used.
SiteDataFeatureProto GetUsedFeatureProto() {
  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_observation_duration(0U);
  used_feature_proto.set_use_timestamp(1U);
  return used_feature_proto;
}

}  // namespace

class SiteDataImplTest : public ::testing::Test {
 protected:
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  scoped_refptr<TestSiteDataImpl> GetDataImpl(
      const url::Origin& origin,
      base::WeakPtr<SiteDataImpl::OnDestroyDelegate> destroy_delegate,
      SiteDataStore* data_store) {
    return base::MakeRefCounted<TestSiteDataImpl>(origin, destroy_delegate,
                                                  data_store);
  }

  // Use a mock data store to intercept the initialization callback and save it
  // locally so it can be run later.
  scoped_refptr<TestSiteDataImpl> GetDataImplAndInterceptReadCallback(
      const url::Origin& origin,
      base::WeakPtr<SiteDataImpl::OnDestroyDelegate> destroy_delegate,
      MockDataStore* mock_data_store,
      SiteDataStore::ReadSiteDataFromStoreCallback* read_cb) {
    auto read_from_store_mock_impl =
        [&](const url::Origin& origin,
            SiteDataStore::ReadSiteDataFromStoreCallback callback) {
          *read_cb = std::move(callback);
        };

    EXPECT_CALL(*mock_data_store,
                ReadSiteDataFromStore(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(read_from_store_mock_impl));
    auto local_site_data =
        GetDataImpl(origin, destroy_delegate_.GetWeakPtr(), mock_data_store);
    ::testing::Mock::VerifyAndClear(mock_data_store);
    return local_site_data;
  }

  const url::Origin kDummyOrigin = url::Origin::Create(GURL("foo.com"));
  const url::Origin kDummyOrigin2 = url::Origin::Create(GURL("bar.com"));

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Use a NiceMock as there's no need to add expectations in these tests,
  // there's a dedicated test that ensure that the delegate works as expected.
  ::testing::NiceMock<testing::MockSiteDataImplOnDestroyDelegate>
      destroy_delegate_;

  testing::NoopSiteDataStore data_store_;
};

TEST_F(SiteDataImplTest, BasicTestEndToEnd) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();

  // Initially the feature usage should be reported as unknown.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesAudioInBackground());

  // Advance the clock by a time lower than the minimum observation time for
  // the audio feature.
  AdvanceClock(SiteDataImpl::GetFeatureObservationWindowLengthForTesting() -
               base::Seconds(1));

  // The audio feature usage is still unknown as the observation window hasn't
  // expired.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UsesAudioInBackground());

  // Report that the audio feature has been used.
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());

  // When a feature is in use it's expected that its recorded observation
  // timestamp is equal to the time delta since Unix Epoch when the observation
  // has been made.
  EXPECT_EQ(
      local_site_data
          ->FeatureObservationTimestamp(
              local_site_data->site_characteristics_for_testing()
                  .uses_audio_in_background())
          .InSeconds(),
      (base::TimeTicks::Now() - base::TimeTicks::UnixEpoch()).InSeconds());
  EXPECT_EQ(local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            base::TimeDelta());

  // Advance the clock and make sure that title change feature gets reported as
  // unused.
  AdvanceClock(SiteDataImpl::GetFeatureObservationWindowLengthForTesting());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureNotInUse,
            local_site_data->UpdatesTitleInBackground());

  // Observating that a feature has been used after its observation window has
  // expired should still be recorded, the feature should then be reported as
  // used.
  local_site_data->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UpdatesTitleInBackground());

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataImplTest, LastLoadedTime) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);

  // Create a second instance of this object, simulates having several tab
  // owning it.
  auto local_site_data2(local_site_data);

  local_site_data->NotifySiteLoaded();
  base::TimeDelta last_loaded_time =
      local_site_data->last_loaded_time_for_testing();

  AdvanceClock(base::Seconds(1));

  // Loading the site a second time shouldn't change the last loaded time.
  local_site_data2->NotifySiteLoaded();
  EXPECT_EQ(last_loaded_time, local_site_data2->last_loaded_time_for_testing());

  AdvanceClock(base::Seconds(1));

  // Unloading the site shouldn't update the last loaded time as there's still
  // a loaded instance.
  local_site_data2->NotifySiteUnloaded(TabVisibility::kForeground);
  EXPECT_EQ(last_loaded_time, local_site_data->last_loaded_time_for_testing());

  AdvanceClock(base::Seconds(1));

  local_site_data->NotifySiteUnloaded(TabVisibility::kForeground);
  EXPECT_NE(last_loaded_time, local_site_data->last_loaded_time_for_testing());
}

TEST_F(SiteDataImplTest, GetFeatureUsageForUnloadedSite) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();

  AdvanceClock(SiteDataImpl::GetFeatureObservationWindowLengthForTesting() -
               base::Seconds(1));
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesTitleInBackground());

  const base::TimeDelta observation_duration_before_unload =
      local_site_data->FeatureObservationDuration(
          local_site_data->site_characteristics_for_testing()
              .updates_title_in_background());

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);

  // Once unloaded the feature observations should still be accessible.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesTitleInBackground());

  // Advancing the clock shouldn't affect the observation duration for this
  // feature.
  AdvanceClock(base::Seconds(1));
  EXPECT_EQ(observation_duration_before_unload,
            local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .updates_title_in_background()));
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesTitleInBackground());

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();

  AdvanceClock(base::Seconds(1));

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureNotInUse,
            local_site_data->UpdatesTitleInBackground());

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataImplTest, AllDurationGetSavedOnUnload) {
  // This test helps making sure that the observation/timestamp fields get saved
  // for all the features being tracked.
  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);

  const base::TimeDelta kInterval = base::Seconds(1);
  const auto kIntervalInternalRepresentation =
      TestSiteDataImpl::TimeDeltaToInternalRepresentation(kInterval);
  const auto kZeroIntervalInternalRepresentation =
      TestSiteDataImpl::TimeDeltaToInternalRepresentation(base::TimeDelta());

  // The internal representation of a zero interval is expected to be equal to
  // zero as the protobuf use variable size integers and so storing zero values
  // is really efficient (uses only one bit).
  EXPECT_EQ(0U, kZeroIntervalInternalRepresentation);

  const base::TimeDelta kInitialTimeSinceEpoch =
      base::TimeTicks::Now() - base::TimeTicks::UnixEpoch();

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  AdvanceClock(kInterval);
  // Makes use of a feature to make sure that the observation timestamps get
  // saved.
  local_site_data->NotifyUsesAudioInBackground();
  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);

  SiteDataProto expected_proto;

  auto expected_last_loaded_time =
      TestSiteDataImpl::TimeDeltaToInternalRepresentation(
          kInterval + kInitialTimeSinceEpoch);

  expected_proto.set_last_loaded(expected_last_loaded_time);

  // Features that haven't been used should have an observation duration of
  // |kIntervalInternalRepresentation| and an observation timestamp equal to
  // zero.
  SiteDataFeatureProto unused_feature_proto;
  unused_feature_proto.set_observation_duration(
      kIntervalInternalRepresentation);

  expected_proto.mutable_updates_favicon_in_background()->CopyFrom(
      unused_feature_proto);
  expected_proto.mutable_updates_title_in_background()->CopyFrom(
      unused_feature_proto);

  // The audio feature has been used, so its observation duration value should
  // be equal to zero, and its observation timestamp should be equal to the last
  // loaded time in this case (as this feature has been used right before
  // unloading).
  SiteDataFeatureProto used_feature_proto;
  used_feature_proto.set_use_timestamp(expected_last_loaded_time);
  expected_proto.mutable_uses_audio_in_background()->CopyFrom(
      used_feature_proto);

  EXPECT_EQ(
      expected_proto.SerializeAsString(),
      local_site_data->site_characteristics_for_testing().SerializeAsString());
}

// Verify that the OnDestroyDelegate gets notified when a
// SiteDataImpl object gets destroyed.
TEST_F(SiteDataImplTest, DestroyNotifiesDelegate) {
  ::testing::StrictMock<testing::MockSiteDataImplOnDestroyDelegate>
      strict_delegate;
  {
    auto local_site_data =
        GetDataImpl(kDummyOrigin, strict_delegate.GetWeakPtr(), &data_store_);
    EXPECT_CALL(strict_delegate,
                OnSiteDataImplDestroyed(local_site_data.get()));
  }
  ::testing::Mock::VerifyAndClear(&strict_delegate);
}

TEST_F(SiteDataImplTest, OnInitCallbackMergePreviousObservations) {
  // Use a mock data store to intercept the initialization callback and save it
  // locally so it can be run later. This simulates an asynchronous
  // initialization of this object and is used to test that the observations
  // made between the time this object has been created and the callback is
  // called get properly merged.
  ::testing::StrictMock<MockDataStore> mock_data_store;
  SiteDataStore::ReadSiteDataFromStoreCallback read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, destroy_delegate_.GetWeakPtr(), &mock_data_store, &read_cb);

  // Simulates audio in background usage before the callback gets called.
  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesFaviconInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesTitleInBackground());

  // Unload the site and save the last loaded time to make sure the
  // initialization doesn't overwrite it.
  AdvanceClock(base::Seconds(1));
  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
  AdvanceClock(base::Seconds(1));
  auto last_loaded = local_site_data->last_loaded_time_for_testing();

  // Add a couple of performance samples.
  local_site_data->NotifyLoadTimePerformanceMeasurement(
      base::Microseconds(100), base::Microseconds(1000), 2000u);
  local_site_data->NotifyLoadTimePerformanceMeasurement(
      base::Microseconds(200), base::Microseconds(500), 1000u);

  // Make sure the local performance samples are averaged as expected.
  EXPECT_EQ(2U, local_site_data->load_duration().num_datums());
  EXPECT_EQ(150, local_site_data->load_duration().value());

  EXPECT_EQ(2U, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(750.0, local_site_data->cpu_usage_estimate().value());

  EXPECT_EQ(2U, local_site_data->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1500.0, local_site_data->private_footprint_kb_estimate().value());

  // This protobuf should have a valid |last_loaded| field and valid observation
  // durations for each features, but the |use_timestamp| field shouldn't have
  // been initialized for the features that haven't been used.
  EXPECT_TRUE(
      local_site_data->site_characteristics_for_testing().has_last_loaded());
  EXPECT_TRUE(local_site_data->site_characteristics_for_testing()
                  .uses_audio_in_background()
                  .has_use_timestamp());
  EXPECT_FALSE(local_site_data->site_characteristics_for_testing()
                   .updates_title_in_background()
                   .has_use_timestamp());
  EXPECT_TRUE(local_site_data->site_characteristics_for_testing()
                  .updates_title_in_background()
                  .has_observation_duration());
  EXPECT_FALSE(local_site_data->site_characteristics_for_testing()
                   .has_load_time_estimates());

  // Initialize a fake protobuf that indicates that this site updates its title
  // while in background and set a fake last loaded time (this should be
  // overridden once the callback runs).
  std::optional<SiteDataProto> test_proto = SiteDataProto();
  SiteDataFeatureProto unused_feature_proto = GetUnusedFeatureProto();
  test_proto->mutable_updates_title_in_background()->CopyFrom(
      GetUsedFeatureProto());
  test_proto->mutable_updates_favicon_in_background()->CopyFrom(
      unused_feature_proto);
  test_proto->mutable_uses_audio_in_background()->CopyFrom(
      unused_feature_proto);
  test_proto->set_last_loaded(42);

  // Set the previously saved performance averages.
  auto* estimates = test_proto->mutable_load_time_estimates();
  estimates->set_avg_load_duration_us(50);
  estimates->set_avg_cpu_usage_us(250);
  estimates->set_avg_footprint_kb(500);

  // Run the callback to indicate that the initialization has completed.
  std::move(read_cb).Run(test_proto);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UpdatesTitleInBackground());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            local_site_data->UpdatesFaviconInBackground());
  EXPECT_EQ(last_loaded, local_site_data->last_loaded_time_for_testing());

  // Make sure the local performance samples have been updated with the previous
  // averages.
  EXPECT_EQ(3U, local_site_data->load_duration().num_datums());
  EXPECT_EQ(137.5, local_site_data->load_duration().value());

  EXPECT_EQ(3U, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(562.5, local_site_data->cpu_usage_estimate().value());

  EXPECT_EQ(3U, local_site_data->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1125, local_site_data->private_footprint_kb_estimate().value());

  // Verify that the in-memory data is flushed to the protobuffer on write.
  EXPECT_CALL(mock_data_store,
              WriteSiteDataIntoStore(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const url::Origin& origin, const SiteDataProto& proto) {
            ASSERT_TRUE(proto.has_load_time_estimates());
            const auto& estimates = proto.load_time_estimates();
            ASSERT_TRUE(estimates.has_avg_load_duration_us());
            EXPECT_EQ(137.5, estimates.avg_load_duration_us());
            ASSERT_TRUE(estimates.has_avg_cpu_usage_us());
            EXPECT_EQ(562.5, estimates.avg_cpu_usage_us());
            ASSERT_TRUE(estimates.has_avg_footprint_kb());
            EXPECT_EQ(1125, estimates.avg_footprint_kb());
          }));

  local_site_data = nullptr;
  ::testing::Mock::VerifyAndClear(&mock_data_store);
}

TEST_F(SiteDataImplTest, LateAsyncReadDoesntEraseData) {
  // Ensure that no historical data get lost if an asynchronous read from the
  // data store finishes after the last reference to a SiteDataImpl gets
  // destroyed.

  ::testing::StrictMock<MockDataStore> mock_data_store;
  SiteDataStore::ReadSiteDataFromStoreCallback read_cb;

  auto local_site_data_writer = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, destroy_delegate_.GetWeakPtr(), &mock_data_store, &read_cb);

  local_site_data_writer->NotifySiteLoaded();
  local_site_data_writer->NotifyLoadedSiteBackgrounded();
  local_site_data_writer->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data_writer->UsesAudioInBackground());

  local_site_data_writer->NotifySiteUnloaded(TabVisibility::kBackground);

  // Releasing |local_site_data_writer| should cause this object to get
  // destroyed but there shouldn't be any write operation as the read hasn't
  // completed.
  EXPECT_CALL(destroy_delegate_, OnSiteDataImplDestroyed(::testing::_));
  EXPECT_CALL(mock_data_store,
              WriteSiteDataIntoStore(::testing::_, ::testing::_))
      .Times(0);
  local_site_data_writer = nullptr;
  ::testing::Mock::VerifyAndClear(&destroy_delegate_);
  ::testing::Mock::VerifyAndClear(&mock_data_store);

  EXPECT_TRUE(read_cb.IsCancelled());
}

TEST_F(SiteDataImplTest, LateAsyncReadDoesntBypassClearEvent) {
  ::testing::NiceMock<MockDataStore> mock_data_store;
  SiteDataStore::ReadSiteDataFromStoreCallback read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, destroy_delegate_.GetWeakPtr(), &mock_data_store, &read_cb);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            local_site_data->UsesAudioInBackground());
  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);

  // TODO(sebmarchand): Test that data is cleared here.
  local_site_data->ClearObservationsAndInvalidateReadOperation();

  EXPECT_TRUE(read_cb.IsCancelled());
}

TEST_F(SiteDataImplTest, BackgroundedCountTests) {
  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);

  // By default the tabs are expected to be foregrounded.
  EXPECT_EQ(0U, local_site_data->loaded_tabs_in_background_count_for_testing());

  local_site_data->NotifySiteLoaded();
  AdvanceClock(base::Seconds(1));
  local_site_data->NotifyLoadedSiteBackgrounded();

  auto background_session_begin =
      local_site_data->background_session_begin_for_testing();
  EXPECT_EQ(base::TimeTicks::Now(), background_session_begin);

  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());

  AdvanceClock(base::Seconds(1));

  // Add a second instance of this object, this one pretending to be in
  // foreground.
  auto local_site_data_copy(local_site_data);
  local_site_data_copy->NotifySiteLoaded();
  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());

  EXPECT_EQ(background_session_begin,
            local_site_data->background_session_begin_for_testing());

  AdvanceClock(base::Seconds(1));

  local_site_data->NotifyLoadedSiteForegrounded();
  EXPECT_EQ(0U, local_site_data->loaded_tabs_in_background_count_for_testing());

  auto expected_observation_duration =
      base::TimeTicks::Now() - background_session_begin;

  auto observed_observation_duration =
      local_site_data->FeatureObservationDuration(
          local_site_data->site_characteristics_for_testing()
              .updates_title_in_background());

  EXPECT_EQ(expected_observation_duration, observed_observation_duration);

  AdvanceClock(base::Seconds(1));

  local_site_data->NotifyLoadedSiteBackgrounded();
  EXPECT_EQ(1U, local_site_data->loaded_tabs_in_background_count_for_testing());
  background_session_begin =
      local_site_data->background_session_begin_for_testing();
  EXPECT_EQ(base::TimeTicks::Now(), background_session_begin);

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
  local_site_data_copy->NotifySiteUnloaded(TabVisibility::kForeground);
}

TEST_F(SiteDataImplTest, OptionalFieldsNotPopulatedWhenClean) {
  ::testing::StrictMock<MockDataStore> mock_data_store;
  SiteDataStore::ReadSiteDataFromStoreCallback read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, destroy_delegate_.GetWeakPtr(), &mock_data_store, &read_cb);

  EXPECT_EQ(0u, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(0u, local_site_data->private_footprint_kb_estimate().num_datums());

  std::optional<SiteDataProto> test_proto = SiteDataProto();

  // Run the callback to indicate that the initialization has completed.
  std::move(read_cb).Run(test_proto);

  // There still should be no perf data.
  EXPECT_EQ(0u, local_site_data->cpu_usage_estimate().num_datums());
  EXPECT_EQ(0u, local_site_data->private_footprint_kb_estimate().num_datums());

  // Dirty the record to force a write.
  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data->NotifyUsesAudioInBackground();

  // Verify that the saved protobuffer isn't populated with the perf fields.
  EXPECT_CALL(mock_data_store,
              WriteSiteDataIntoStore(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const url::Origin& origin, const SiteDataProto& proto) {
            ASSERT_FALSE(proto.has_load_time_estimates());
          }));

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
  local_site_data = nullptr;
  ::testing::Mock::VerifyAndClear(&mock_data_store);
}

TEST_F(SiteDataImplTest, FlushingStateToProtoDoesntAffectData) {
  // Create 2 DataImpl object and do the same operations on them, ensures that
  // calling FlushStateToProto doesn't affect the data that gets recorded.

  auto local_site_data =
      GetDataImpl(kDummyOrigin, destroy_delegate_.GetWeakPtr(), &data_store_);
  auto local_site_data_ref =
      GetDataImpl(kDummyOrigin2, destroy_delegate_.GetWeakPtr(), &data_store_);

  local_site_data->NotifySiteLoaded();
  local_site_data->NotifyLoadedSiteBackgrounded();
  local_site_data_ref->NotifySiteLoaded();
  local_site_data_ref->NotifyLoadedSiteBackgrounded();

  AdvanceClock(base::Seconds(15));
  local_site_data->FlushStateToProto();
  AdvanceClock(base::Seconds(15));

  local_site_data->NotifyUsesAudioInBackground();
  local_site_data_ref->NotifyUsesAudioInBackground();

  local_site_data->FlushStateToProto();

  EXPECT_EQ(local_site_data->FeatureObservationTimestamp(
                local_site_data->site_characteristics_for_testing()
                    .uses_audio_in_background()),
            local_site_data_ref->FeatureObservationTimestamp(
                local_site_data_ref->site_characteristics_for_testing()
                    .uses_audio_in_background()));

  EXPECT_EQ(local_site_data->FeatureObservationDuration(
                local_site_data->site_characteristics_for_testing()
                    .updates_title_in_background()),
            local_site_data_ref->FeatureObservationDuration(
                local_site_data_ref->site_characteristics_for_testing()
                    .updates_title_in_background()));

  local_site_data->NotifySiteUnloaded(TabVisibility::kBackground);
  local_site_data_ref->NotifySiteUnloaded(TabVisibility::kBackground);
}

TEST_F(SiteDataImplTest, DataLoadedCallbackInvoked) {
  ::testing::StrictMock<MockDataStore> mock_data_store;
  SiteDataStore::ReadSiteDataFromStoreCallback read_cb;

  auto local_site_data = GetDataImplAndInterceptReadCallback(
      kDummyOrigin, destroy_delegate_.GetWeakPtr(), &mock_data_store, &read_cb);

  EXPECT_FALSE(local_site_data->DataLoaded());

  bool callback_invoked = false;
  local_site_data->RegisterDataLoadedCallback(
      base::BindLambdaForTesting([&]() { callback_invoked = true; }));

  // Run the callback to indicate that the initialization has completed.
  std::optional<SiteDataProto> test_proto = SiteDataProto();
  std::move(read_cb).Run(test_proto);

  EXPECT_TRUE(callback_invoked);
  EXPECT_TRUE(local_site_data->DataLoaded());
}

}  // namespace internal
}  // namespace performance_manager
