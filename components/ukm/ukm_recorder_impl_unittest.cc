// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/ukm/scheme_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_recorder_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "url/gurl.h"

namespace ukm {
namespace {

using TestEvent1 = builders::PageLoad;

const uint64_t kTestEntryHash = 1234;
const uint64_t kTestMetricsHash = 12345;
const char kTestEntryName[] = "TestEntry";
const char kTestMetrics[] = "TestMetrics";
const int32_t kWebDXFeature1 = 1;
const int32_t kWebDXFeature2 = 2;
const size_t kWebDXFeatureNumberOfFeaturesForTesting = 5;

// Builds a blank UkmEntry with given SourceId.
mojom::UkmEntryPtr BlankUkmEntry(SourceId source_id) {
  return mojom::UkmEntry::New(source_id, 0ull,
                              base::flat_map<uint64_t, int64_t>());
}

std::map<uint64_t, builders::EntryDecoder> CreateTestingDecodeMap() {
  return {
      {kTestEntryHash,
       {kTestEntryName,
        {
            {kTestMetricsHash, kTestMetrics},
        }}},
  };
}

// Helper class for testing UkmRecorderImpl observers.
class TestUkmObserver : public UkmRecorderObserver {
 public:
  explicit TestUkmObserver(UkmRecorderImpl* ukm_recorder_impl) {
    base::flat_set<uint64_t> event_hashes = {kTestEntryHash};
    ukm_recorder_impl->AddUkmRecorderObserver(event_hashes, this);
  }

  ~TestUkmObserver() override = default;

  // UkmRecorderImpl::UkmRecorderObserver override.
  void OnEntryAdded(mojom::UkmEntryPtr entry) override {
    if (stop_waiting_)
      std::move(stop_waiting_).Run();
    ASSERT_EQ(entry->event_hash, ukm_entry_->event_hash);
    ASSERT_EQ(entry->source_id, ukm_entry_->source_id);
    ASSERT_EQ(entry->metrics[kTestMetricsHash],
              ukm_entry_->metrics[kTestMetricsHash]);
  }

  void OnUpdateSourceURL(SourceId source_id,
                         const std::vector<GURL>& urls) override {
    if (stop_waiting_)
      std::move(stop_waiting_).Run();
    ASSERT_EQ(source_id_, source_id);
    ASSERT_EQ(urls_, urls);
  }

  void OnPurgeRecordingsWithUrlScheme(const std::string& url_scheme) override {
    if (stop_waiting_)
      std::move(stop_waiting_).Run();
  }

  void OnPurge() override {
    if (stop_waiting_)
      std::move(stop_waiting_).Run();
  }

  void OnUkmAllowedStateChanged(ukm::UkmConsentState consent_state) override {
    if (stop_waiting_)
      std::move(stop_waiting_).Run();

    EXPECT_EQ(expected_state_, consent_state);
  }

  void WaitOnUkmAllowedStateChanged(ukm::UkmConsentState expected_state) {
    expected_state_ = expected_state;
    WaitCallback();
  }

  void WaitAddEntryCallback(uint64_t event_hash, mojom::UkmEntryPtr ukm_entry) {
    ukm_entry_ = std::move(ukm_entry);
    WaitCallback();
  }

  void WaitUpdateSourceURLCallback(SourceId source_id,
                                   const std::vector<GURL>& urls) {
    source_id_ = source_id;
    urls_ = urls;
    WaitCallback();
  }

  void WaitCallback() {
    base::RunLoop run_loop;
    stop_waiting_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::OnceClosure stop_waiting_;
  mojom::UkmEntryPtr ukm_entry_;
  SourceId source_id_;
  std::vector<GURL> urls_;
  ukm::UkmConsentState expected_state_;
};

}  // namespace

TEST(UkmRecorderImplTest, IsSampledIn) {
  UkmRecorderImpl impl;

  for (int i = 0; i < 100; ++i) {
    // These are constant regardless of the seed, source, and event.
    EXPECT_FALSE(impl.IsSampledIn(-i, i, 0));
    EXPECT_TRUE(impl.IsSampledIn(-i, i, 1));
  }

  // These depend on the source, event, and initial seed. There's no real
  // predictability here but should see roughly 50% true and 50% false with
  // no obvious correlation and the same for every run of the test.
  impl.SetSamplingSeedForTesting(123);
  EXPECT_FALSE(impl.IsSampledIn(1, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(1, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(456);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(789);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));

  // Load a configuration for more detailed testing.
  std::map<std::string, std::string> params = {
      {"y.a", "3"},
      {"y.b", "y.a"},
      {"y.c", "y.a"},
  };
  impl.LoadExperimentSamplingParams(params);
  EXPECT_LT(impl.default_sampling_rate_, 0);

  // Functions under test take hashes instead of strings.
  uint64_t hash_ya = base::HashMetricName("y.a");
  uint64_t hash_yb = base::HashMetricName("y.b");
  uint64_t hash_yc = base::HashMetricName("y.c");

  // Check that the parameters are active.
  EXPECT_TRUE(impl.IsSampledIn(11, hash_ya));
  EXPECT_TRUE(impl.IsSampledIn(22, hash_ya));
  EXPECT_FALSE(impl.IsSampledIn(33, hash_ya));
  EXPECT_FALSE(impl.IsSampledIn(44, hash_ya));
  EXPECT_FALSE(impl.IsSampledIn(55, hash_ya));

  // Check that sampled in/out is the same for all three.
  for (int source = 0; source < 100; ++source) {
    bool sampled_in = impl.IsSampledIn(source, hash_ya);
    EXPECT_EQ(sampled_in, impl.IsSampledIn(source, hash_yb));
    EXPECT_EQ(sampled_in, impl.IsSampledIn(source, hash_yc));
  }
}

TEST(UkmRecorderImplTest, PurgeExtensionRecordings) {
  TestUkmRecorder recorder;
  // Enable extension sync.
  recorder.SetIsWebstoreExtensionCallback(
      base::BindRepeating([](std::string_view) { return true; }));

  // Record some sources, events, and web features.
  SourceId id1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id1, GURL("https://www.google.ca"));
  SourceId id2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id2, GURL("chrome-extension://abc/manifest.json"));
  SourceId id3 = ConvertToSourceId(3, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id3, GURL("http://www.wikipedia.org"));
  SourceId id4 = ConvertToSourceId(4, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id4, GURL("chrome-extension://abc/index.html"));

  TestEvent1(id1).Record(&recorder);
  TestEvent1(id2).Record(&recorder);

  recorder.RecordWebDXFeatures(id3, {kWebDXFeature1},
                               kWebDXFeatureNumberOfFeaturesForTesting);
  recorder.RecordWebDXFeatures(id4, {kWebDXFeature2},
                               kWebDXFeatureNumberOfFeaturesForTesting);

  // All sources, events, and web features have been recorded.
  EXPECT_TRUE(recorder.recording_enabled(EXTENSIONS));
  EXPECT_TRUE(recorder.recording_is_continuous_);
  EXPECT_EQ(4U, recorder.sources().size());
  EXPECT_EQ(2U, recorder.entries().size());
  EXPECT_EQ(2U, recorder.webdx_features().size());

  recorder.PurgeRecordingsWithUrlScheme(kExtensionScheme);

  // Recorded sources of extension scheme and related events/web features have
  // been cleared.
  EXPECT_EQ(2U, recorder.sources().size());
  EXPECT_EQ(1U, recorder.sources().count(id1));
  EXPECT_EQ(0U, recorder.sources().count(id2));
  EXPECT_EQ(1U, recorder.sources().count(id3));
  EXPECT_EQ(0U, recorder.sources().count(id4));

  EXPECT_FALSE(recorder.recording_is_continuous_);
  EXPECT_EQ(1U, recorder.entries().size());
  EXPECT_EQ(id1, recorder.entries()[0]->source_id);

  EXPECT_EQ(1U, recorder.webdx_features().size());
  EXPECT_TRUE(base::Contains(recorder.webdx_features(), id3));

  // Recording is disabled for extensions, thus new extension URL will not be
  // recorded.
  recorder.UpdateRecording({UkmConsentType::MSBB});
  recorder.UpdateSourceURL(id4, GURL("chrome-extension://abc/index.html"));
  EXPECT_FALSE(recorder.recording_state_.Has(UkmConsentType::EXTENSIONS));
  EXPECT_EQ(2U, recorder.sources().size());
}

TEST(UkmRecorderImplTest, WebApkSourceUrl) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url("https://example_url.com/manifest.json");
  SourceId id =
      UkmRecorderImpl::GetSourceIdFromScopeImpl(url, SourceIdType::WEBAPK_ID);

  ASSERT_NE(kInvalidSourceId, id);

  const auto& sources = test_ukm_recorder.GetSources();
  ASSERT_EQ(1ul, sources.size());
  auto it = sources.find(id);
  ASSERT_NE(sources.end(), it);
  EXPECT_EQ(url, it->second->url());
  EXPECT_EQ(1u, it->second->urls().size());
  EXPECT_EQ(SourceIdType::WEBAPK_ID, GetSourceIdType(id));
}

TEST(UkmRecorderImplTest, PaymentAppScopeUrl) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url("https://bobpay.com");
  SourceId id = UkmRecorderImpl::GetSourceIdFromScopeImpl(
      url, SourceIdType::PAYMENT_APP_ID);

  ASSERT_NE(kInvalidSourceId, id);

  const auto& sources = test_ukm_recorder.GetSources();
  ASSERT_EQ(1ul, sources.size());
  auto it = sources.find(id);
  ASSERT_NE(sources.end(), it);
  EXPECT_EQ(url, it->second->url());
  EXPECT_EQ(1u, it->second->urls().size());
  EXPECT_EQ(SourceIdType::PAYMENT_APP_ID, GetSourceIdType(id));
}

TEST(UkmRecorderImplTest, WebIdentityScopeUrl) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url("https://idp.com");
  SourceId id = UkmRecorderImpl::GetSourceIdFromScopeImpl(
      url, SourceIdType::WEB_IDENTITY_ID);

  ASSERT_NE(kInvalidSourceId, id);

  const auto& sources = test_ukm_recorder.GetSources();
  ASSERT_EQ(1ul, sources.size());
  auto it = sources.find(id);
  ASSERT_NE(sources.end(), it);
  EXPECT_EQ(url, it->second->url());
  EXPECT_EQ(1u, it->second->urls().size());
  EXPECT_EQ(SourceIdType::WEB_IDENTITY_ID, GetSourceIdType(id));
}

// Tests that UkmRecorderObserver is notified on a new UKM entry.
TEST(UkmRecorderImplTest, ObserverNotifiedOnNewEntry) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  TestUkmObserver test_observer(&test_ukm_recorder);

  test_ukm_recorder.decode_map_ = CreateTestingDecodeMap();
  auto entry = mojom::UkmEntry::New();
  entry->event_hash = kTestEntryHash;
  entry->source_id = 345;
  entry->metrics[kTestMetricsHash] = 10;
  test_ukm_recorder.AddEntry(entry->Clone());
  test_observer.WaitAddEntryCallback(kTestEntryHash, std::move(entry));
}

// Tests that UkmRecorderObserver is notified on source URL updates.
TEST(UkmRecorderImplTest, ObserverNotifiedOnSourceURLUpdate) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  TestUkmObserver test_observer(&test_ukm_recorder);
  uint64_t source_id = 345;

  GURL url("http://abc.com");
  std::vector<GURL> urls;
  urls.emplace_back(url);
  test_ukm_recorder.UpdateSourceURL(source_id, url);
  test_observer.WaitUpdateSourceURLCallback(source_id, urls);
}

// Tests that UkmRecorderObserver is notified on source URL updates.
TEST(UkmRecorderImplTest, ObserverNotifiedWhenNotRecording) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  TestUkmObserver test_observer(&test_ukm_recorder);
  test_ukm_recorder.DisableRecording();

  GURL url("http://abc.com");
  std::vector<GURL> urls;
  urls.emplace_back(url);

  // Updating source should notify observers when recording is disabled.
  uint64_t source_id1 = 345;
  test_ukm_recorder.UpdateSourceURL(source_id1, url);
  test_observer.WaitUpdateSourceURLCallback(source_id1, urls);

  // Updating app URLs should notify observers when recording is disabled.
  uint64_t source_id2 = 12;
  test_ukm_recorder.UpdateAppURL(source_id2, url, AppType::kPWA);
  test_observer.WaitUpdateSourceURLCallback(source_id2, urls);

  // Recording navigation data should notify observers when recording is
  // disabled.
  SourceId source_id3 = ConvertToSourceId(15, SourceIdType::NAVIGATION_ID);
  UkmSource::NavigationData data;
  data.urls.push_back(url);
  data.urls.emplace_back("https://bcd.com");
  test_ukm_recorder.RecordNavigation(source_id3, data);
  test_observer.WaitUpdateSourceURLCallback(source_id3, data.urls);
}

// Tests that UkmRecorderObserver is notified on purge.
TEST(UkmRecorderImplTest, ObserverNotifiedOnPurge) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  TestUkmObserver test_observer(&test_ukm_recorder);

  test_ukm_recorder.PurgeRecordingsWithUrlScheme(kExtensionScheme);
  test_observer.WaitCallback();

  test_ukm_recorder.Purge();
  test_observer.WaitCallback();
}

TEST(UkmRecorderImplTest, ObserverNotifiedOnUkmAllowedStateChanged) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  TestUkmObserver test_observer(&test_ukm_recorder);

  test_ukm_recorder.OnUkmAllowedStateChanged(ukm::UkmConsentState());
  test_observer.WaitOnUkmAllowedStateChanged(ukm::UkmConsentState());

  test_ukm_recorder.OnUkmAllowedStateChanged(ukm::UkmConsentState::All());
  test_observer.WaitOnUkmAllowedStateChanged(ukm::UkmConsentState::All());
}

// Tests that adding and removing observers work as expected.
TEST(UkmRecorderImplTest, AddRemoveObserver) {
  base::test::TaskEnvironment env;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Adding 3 observers, the first 2 oberserve the same event
  // while the last one observes a different event.
  UkmRecorderObserver obs1, obs2, obs3;
  base::flat_set<uint64_t> events1 = {123};
  test_ukm_recorder.AddUkmRecorderObserver(events1, &obs1);
  test_ukm_recorder.AddUkmRecorderObserver(events1, &obs2);
  base::flat_set<uint64_t> events2 = {345};
  test_ukm_recorder.AddUkmRecorderObserver(events2, &obs3);

  // Remove the first observer.
  test_ukm_recorder.RemoveUkmRecorderObserver(&obs1);
  {
    base::AutoLock auto_lock(test_ukm_recorder.lock_);
    ASSERT_FALSE(test_ukm_recorder.observers_.empty());
    // There are still 2 separate events being observed, each
    // has one observer now.
    ASSERT_NE(test_ukm_recorder.observers_.find(events1),
              test_ukm_recorder.observers_.end());
    ASSERT_NE(test_ukm_recorder.observers_.find(events2),
              test_ukm_recorder.observers_.end());
  }
  // Removing the 2nd observer.
  test_ukm_recorder.RemoveUkmRecorderObserver(&obs2);
  {
    base::AutoLock auto_lock(test_ukm_recorder.lock_);
    // Only the 2nd event is being observed now, the first
    // event should be removed from the observers map.
    ASSERT_EQ(test_ukm_recorder.observers_.find(events1),
              test_ukm_recorder.observers_.end());
    ASSERT_NE(test_ukm_recorder.observers_.find(events2),
              test_ukm_recorder.observers_.end());
  }
  // Removing the last observer should clear the observer map.
  test_ukm_recorder.RemoveUkmRecorderObserver(&obs3);
  {
    base::AutoLock auto_lock(test_ukm_recorder.lock_);
    ASSERT_TRUE(test_ukm_recorder.observers_.empty());
  }
}

TEST(UkmRecorderImplTest, VerifyShouldDropEntry) {
  UkmRecorderImpl impl;

  // Enable Recording, if recording was disabled everything
  // would be dropped.
  impl.EnableRecording();

  auto msbb_entry =
      BlankUkmEntry(ConvertToSourceId(1, SourceIdType::NAVIGATION_ID));
  auto app_entry = BlankUkmEntry(ConvertToSourceId(1, SourceIdType::APP_ID));

  // Neither MSBB nor App-Sync is consented too, both will be dropped.
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Update service with MSBB consent.
  impl.UpdateRecording({MSBB});
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Update service with App-sync consent as well.
  impl.UpdateRecording({MSBB, APPS});
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Update service with only App-sync consent.
  // Only applicable to ASH builds but will not affect the test.
  impl.UpdateRecording({APPS});
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Disabling recording will supersede any consent state.
  impl.UpdateRecording({MSBB, APPS});
  impl.DisableRecording();
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(app_entry.get()));
}

TEST(UkmRecorderImplTest, WebDXFeaturesConsent) {
  UkmRecorderImpl impl;

  // Enable recording and set no sampling (1-in-1).
  impl.EnableRecording();
  impl.SetWebDXFeaturesSamplingForTesting(/*rate=*/1);

  const SourceId kMsbbSourceId =
      ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  const SourceId kAppsSourceId = ConvertToSourceId(1, SourceIdType::APP_ID);

  // Although recording is enabled, neither MSBB nor app-sync are consented to,
  // so no web features should be recorded.
  impl.RecordWebDXFeatures(kMsbbSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kAppsSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 0u);

  // Consent to MSBB only. Only MSBB-related web features should be recorded.
  impl.UpdateRecording({MSBB});
  impl.RecordWebDXFeatures(kMsbbSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kAppsSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 1u);
  EXPECT_TRUE(base::Contains(impl.webdx_features(), kMsbbSourceId));
  impl.webdx_features().clear();

  // Consent to app-sync only. Only app-related related web features should be
  // recorded.
  impl.UpdateRecording({APPS});
  impl.RecordWebDXFeatures(kMsbbSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kAppsSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 1u);
  EXPECT_TRUE(base::Contains(impl.webdx_features(), kAppsSourceId));
  impl.webdx_features().clear();

  // Consent to both MSBB and app-sync. Both MSBB and app related web features
  // should be recorded.
  impl.UpdateRecording({MSBB, APPS});
  impl.RecordWebDXFeatures(kMsbbSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kAppsSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 2u);
  EXPECT_TRUE(base::Contains(impl.webdx_features(), kMsbbSourceId));
  EXPECT_TRUE(base::Contains(impl.webdx_features(), kAppsSourceId));
  impl.webdx_features().clear();

  // Disable recording altogether. No web features should be recorded.
  impl.DisableRecording();
  impl.RecordWebDXFeatures(kMsbbSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kAppsSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 0u);
}

TEST(UkmRecorderImplTest, WebDXFeaturesSampling) {
  UkmRecorderImpl impl;

  // Enable recording, consent to MSBB, and set 1-in-2 sampling.
  impl.EnableRecording();
  impl.UpdateRecording({MSBB});
  impl.SetWebDXFeaturesSamplingForTesting(/*rate=*/2);
  impl.SetSamplingSeedForTesting(0);

  // Create a sampled-in source and sampled-out source. Note that generally,
  // whether a source is sampled-in or sampled-out is "random". These are
  // handpicked source IDs that are known to be sampled-in/out in advance.
  const SourceId kSampledInSourceId =
      ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  const SourceId kSampledOutSourceId =
      ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);

  impl.RecordWebDXFeatures(kSampledInSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kSampledOutSourceId, {kWebDXFeature1},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 1u);
  EXPECT_TRUE(base::Contains(impl.webdx_features(), kSampledInSourceId));
  EXPECT_FALSE(base::Contains(impl.webdx_features(), kSampledOutSourceId));

  // Verify that being sampled-in or sampled-out is consistent across calls.
  // I.e., if a source is sampled-in, then all calls recording web features to
  // it will go through. Similarly, if a source is sampled-out, then all calls
  // recording web features to it will be no-ops. In other words, it's all or
  // nothing.
  impl.RecordWebDXFeatures(kSampledInSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  impl.RecordWebDXFeatures(kSampledOutSourceId, {kWebDXFeature2},
                           kWebDXFeatureNumberOfFeaturesForTesting);
  EXPECT_EQ(impl.webdx_features().size(), 1u);
  ASSERT_TRUE(base::Contains(impl.webdx_features(), kSampledInSourceId));
  EXPECT_TRUE(
      impl.webdx_features().at(kSampledInSourceId).Contains(kWebDXFeature1));
  EXPECT_TRUE(
      impl.webdx_features().at(kSampledInSourceId).Contains(kWebDXFeature2));
  EXPECT_FALSE(base::Contains(impl.webdx_features(), kSampledOutSourceId));
}

}  // namespace ukm
