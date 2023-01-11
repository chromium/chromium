// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/ukm/scheme_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_recorder_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
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
      base::BindRepeating([](base::StringPiece) { return true; }));

  // Record some sources and events.
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

  // All sources and events have been recorded.
  EXPECT_TRUE(recorder.recording_enabled(EXTENSIONS));
  EXPECT_TRUE(recorder.recording_is_continuous_);
  EXPECT_EQ(4U, recorder.sources().size());
  EXPECT_EQ(2U, recorder.entries().size());

  recorder.PurgeRecordingsWithUrlScheme(kExtensionScheme);

  // Recorded sources of extension scheme and related events have been cleared.
  EXPECT_EQ(2U, recorder.sources().size());
  EXPECT_EQ(1U, recorder.sources().count(id1));
  EXPECT_EQ(0U, recorder.sources().count(id2));
  EXPECT_EQ(1U, recorder.sources().count(id3));
  EXPECT_EQ(0U, recorder.sources().count(id4));

  EXPECT_FALSE(recorder.recording_is_continuous_);
  EXPECT_EQ(1U, recorder.entries().size());
  EXPECT_EQ(id1, recorder.entries()[0]->source_id);

  // Recording is disabled for extensions, thus new extension URL will not be
  // recorded.
  recorder.UpdateRecording(UkmConsentState(UkmConsentType::MSBB));
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
  impl.UpdateRecording(UkmConsentState(MSBB));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Update service with App-sync consent as well.
  impl.UpdateRecording(UkmConsentState(MSBB, APPS));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Update service with only App-sync consent.
  // Only applicable to ASH builds but will not affect the test.
  impl.UpdateRecording(UkmConsentState(APPS));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_FALSE(impl.ShouldDropEntryForTesting(app_entry.get()));

  // Disabling recording will supersede any consent state.
  impl.UpdateRecording(UkmConsentState(MSBB, APPS));
  impl.DisableRecording();
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(msbb_entry.get()));
  EXPECT_TRUE(impl.ShouldDropEntryForTesting(app_entry.get()));
}

}  // namespace ukm
