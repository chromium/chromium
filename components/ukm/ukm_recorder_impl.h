// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "components/ukm/bitset.h"
#include "components/ukm/ukm_consent_state.h"
#include "components/ukm/ukm_entry_filter.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "ukm_consent_state.h"

namespace metrics {
class UkmBrowserTestBase;
}

namespace ukm {
class Aggregate;
class Report;
class UkmRecorderImplTest;
class UkmRecorderObserver;
class UkmSource;
class UkmTestHelper;
class UkmUtilsForTest;

COMPONENT_EXPORT(UKM_RECORDER) BASE_DECLARE_FEATURE(kUkmSamplingRateFeature);

// Convention for console debugging messages.
// Example usage:
// $ ./out/Default/chrome --force-enable-metrics-reporting
// --metrics-upload-interval=60 \
// --vmodule=*components/ukm*=3
enum DebuggingLogLevel {
  // Infrequent actions such as changes to user consent, or actions that
  // typically occur once per reporting cycle, e.g. serialization of locally
  // recorded event data into one report and uploading the report to the UKM
  // server.
  Rare = 1,
  // Frequent and recurrent actions within each reporting period, such as an
  // event being recorded, or a new browser navigation has occurred.
  Medium = 2,
  // Very frequent and possibly spammy actions or checks, such as events being
  // dropped due to disabled recording.
  Frequent = 3,
};

namespace debug {
class UkmDebugDataExtractor;
}

class COMPONENT_EXPORT(UKM_RECORDER) UkmRecorderImpl : public UkmRecorder {
  using IsWebstoreExtensionCallback =
      base::RepeatingCallback<bool(std::string_view id)>;

 public:
  UkmRecorderImpl();
  ~UkmRecorderImpl() override;

  // Enables/disables recording control if data is allowed to be collected.
  // |state| defines what is allowed to be collected.
  // See components/ukm/ukm_consent_state.h for details.
  void UpdateRecording(const ukm::UkmConsentState state);
  // Enables recording if MSBB is consented.
  void EnableRecording();
  // Disables recording without updating the consent state.
  void DisableRecording();

  // Controls sampling for testing purposes. Sampling is 1-in-N (N==rate).
  void SetSamplingForTesting(int rate) override;

  // Controls web features sampling for testing purposes. Sampling is 1-in-N
  // (N==rate).
  void SetWebDXFeaturesSamplingForTesting(int rate);

  // True if sampling has been configured.
  bool IsSamplingConfigured() const;

  // Deletes all stored recordings.
  void Purge();

  // Deletes stored Sources containing URLs of the given scheme and events
  // attributed with these Sources.
  void PurgeRecordingsWithUrlScheme(const std::string& url_scheme);

  // Deletes stored Sources with the given Source id type and events
  // attributed with these Sources.
  void PurgeRecordingsWithSourceIdType(ukm::SourceIdType source_id_type);

  // Deletes stored Sources with any Source Id related to MSBB. This included
  // all SourceIds that are not of type APP_ID.
  void PurgeRecordingsWithMsbbSources();

  // Marks a source as no longer needed to be kept alive in memory. The source
  // with given id will be removed from in-memory recordings at the next
  // reporting cycle.
  void MarkSourceForDeletion(ukm::SourceId source_id) override;

  // Sets a callback for determining if an extension URL can be recorded.
  void SetIsWebstoreExtensionCallback(
      const IsWebstoreExtensionCallback& callback);

  // Sets the UkmEntryFilter that will be applied to all subsequent entries
  // reported via AddEntry(). Does not apply the filter to any entries that are
  // already recorded.
  //
  // Currently only accommodates one entry filter.
  void SetEntryFilter(std::unique_ptr<UkmEntryFilter> entry_filter);

  // Register an observer to be notified when a new UKM entry that comes with
  // one of the |event_hashes| is added. This method can be called on any
  // thread.
  void AddUkmRecorderObserver(const base::flat_set<uint64_t>& event_hashes,
                              UkmRecorderObserver* observer);

  // Clears the given |observer| from |observers_|. This method can be called
  // on any thread. If an observer is registered for multiple event sets, it
  // will be removed from all the sets. If an event set no longer has any
  // observers as a result of this call, it will be removed from |observers_|
  // map.
  void RemoveUkmRecorderObserver(UkmRecorderObserver* observer);

  // Called when UKM consent state changed.
  void OnUkmAllowedStateChanged(UkmConsentState state);

  // Sets the sampling seed for testing purposes.
  void SetSamplingSeedForTesting(uint32_t seed) {
    // Normally the seed is set during object construction and remains
    // constant in order to provide consistent results when doing an "is
    // sampled in" query for a given source and event. A "const cast" is
    // necessary to override that.
    *const_cast<uint32_t*>(&sampling_seed_) = seed;
  }

  bool recording_enabled() const { return recording_enabled_; }

  bool recording_enabled(ukm::UkmConsentType type) const {
    return recording_state_.Has(type);
  }

  bool ShouldDropEntryForTesting(mojom::UkmEntry* entry);

 protected:
  // Calculates sampled in/out for a specific source/event based on internal
  // configuration. This function is guaranteed to always return the same
  // result over the life of this object for the same config & input parameters.
  bool IsSampledIn(int64_t source_id, uint64_t event_id);

  // Like above but uses a passed |sampling_rate| instead of internal config.
  bool IsSampledIn(int64_t source_id, uint64_t event_id, int sampling_rate);

  void InitDecodeMap();

  // Writes recordings into a report proto, and clears recordings.
  void StoreRecordingsInReport(Report* report);

  // Prunes data after storing records in the report. Returns the time elapsed
  // in seconds from the moment the newest truncated source was created to the
  // moment it was discarded from memory, if pruning happened  due to number
  // of sources exceeding the max threshold.
  int PruneData(std::set<SourceId>& source_ids_seen);

  // Deletes Sources, Events and Web Features with these source_ids.
  void PurgeDataBySourceIds(const std::unordered_set<SourceId>& source_ids);

  const std::map<SourceId, std::unique_ptr<UkmSource>>& sources() const {
    return recordings_.sources;
  }

  const std::vector<mojom::UkmEntryPtr>& entries() const {
    return recordings_.entries;
  }

  std::map<SourceId, BitSet>& webdx_features() {
    return recordings_.webdx_features;
  }

  const std::map<SourceId, BitSet>& webdx_features() const {
    return recordings_.webdx_features;
  }

  // Keep only newest |max_kept_sources| sources when the number of sources
  // in recordings_ exceeds this threshold. We only consider the set of ids
  // contained in |pruning_set|. Returns the age of newest truncated
  // source in seconds.
  int PruneOldSources(size_t max_kept_sources,
                      const std::set<SourceId>& pruning_set);

  // UkmRecorder:
  void AddEntry(mojom::UkmEntryPtr entry) override;
  void RecordWebDXFeatures(SourceId source_id,
                           const std::set<int32_t>& features,
                           const size_t max_feature_value) override;
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id,
                    const GURL& url,
                    const AppType app_type) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  using UkmRecorder::RecordOtherURL;

  // Get the UkmConsentType associated for a given SourceIdType.
  static UkmConsentType GetConsentType(SourceIdType type);

 protected:
  // Get the set of hashes of event types that are observed by any of the
  // |observers_|. These observers_ need to be notified of a new UKM event with
  // event_hash in set of observed event_hashes even when UKM recording is
  // disabled.
  std::set<uint64_t> GetObservedEventHashes();
  // Update the MojoUkmRecorder clients about any update in parameters. This
  // method can be called on any thread.
  virtual void OnRecorderParametersChanged() {}

 private:
  friend ::metrics::UkmBrowserTestBase;
  friend ::ukm::debug::UkmDebugDataExtractor;
  friend ::ukm::UkmTestHelper;
  friend ::ukm::UkmUtilsForTest;
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, IsSampledIn);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, PurgeExtensionRecordings);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, WebApkSourceUrl);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, PaymentAppScopeUrl);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, WebIdentityScopeUrl);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, ObserverNotifiedOnNewEntry);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, AddRemoveObserver);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest,
                           ObserverNotifiedWhenNotRecording);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, WebDXFeaturesConsent);
  FRIEND_TEST_ALL_PREFIXES(UkmRecorderImplTest, WebDXFeaturesSampling);

  struct MetricAggregate {
    uint64_t total_count = 0;
    double value_sum = 0;
    double value_square_sum = 0.0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_filter = 0;
    uint64_t dropped_due_to_unconfigured = 0;
  };

  struct EventAggregate {
    EventAggregate();
    ~EventAggregate();

    // Fills the proto message from the struct.
    void FillProto(Aggregate* proto_aggregate) const;

    base::flat_map<uint64_t, MetricAggregate> metrics;
    uint64_t total_count = 0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_filter = 0;
    uint64_t dropped_due_to_unconfigured = 0;
  };

  using MetricAggregateMap = std::map<uint64_t, MetricAggregate>;

  // Marks for deletion if the |source_id| is of a certain type.
  void MaybeMarkForDeletion(SourceId source_id);

  // Checks if the given |sanitized_extension_url| should be dropped because of
  // invalid scheme, extension URL recording consent, or whether it's a webstore
  // extension, and records the dropped reason if so.
  bool ShouldDropExtensionUrl(const GURL& sanitized_extension_url,
                              bool has_recorded_reason) const;

  // Returns the result whether |sanitized_url| should be recorded.
  bool ShouldRecordUrl(SourceId source_id, const GURL& sanitized_url) const;

  void RecordSource(std::unique_ptr<UkmSource> source);

  // Determines if an UkmEntry should be dropped and records reason if so.
  bool ShouldDropEntry(mojom::UkmEntry* entry);

  // Applies UkmEntryFilter if there is one registered.
  bool ApplyEntryFilter(mojom::UkmEntry* entry);

  // Loads sampling configurations from field-trial information.
  void LoadExperimentSamplingInfo();

  // Loads sampling configuration from the key/value "params" of a field-trial.
  // This is separated from the above to ease testing.
  void LoadExperimentSamplingParams(
      const std::map<std::string, std::string>& params);

  // Called to notify interested observers about a newly added UKM entry.
  void NotifyObserversWithNewEntry(const mojom::UkmEntry& entry);

  // Helper method to notify all observers on UKM events.
  template <typename Method, typename... Params>
  void NotifyAllObservers(Method m, const Params&... params);

  // Whether recording new data is currently allowed.
  bool recording_enabled_ = false;

  // Whether recording new data is enabled and what type is allowed.
  ukm::UkmConsentState recording_state_;

  // Indicates whether recording continuity has been broken since last report.
  bool recording_is_continuous_ = true;

  // Indicates if sampling has been forced for testing.
  bool sampling_forced_for_testing_ = false;

  // A pseudo-random number used as the base for sampling choices. This
  // allows consistent "is sampled in" results for a given source and event
  // type throughout the life of this object.
  const uint32_t sampling_seed_;

  // Callback for checking extension IDs.
  IsWebstoreExtensionCallback is_webstore_extension_callback_;

  // Filter applied to AddEntry().
  std::unique_ptr<UkmEntryFilter> entry_filter_;

  // Map from hashes to entry and metric names.
  ukm::builders::DecodeMap decode_map_;

  // Sampling configurations, loaded from a field-trial.
  int default_sampling_rate_ = -1;  // -1 == not yet loaded
  int webdx_features_sampling_ = 1;  // by default, no downsampling
  base::flat_map<uint64_t, int> event_sampling_rates_;

  // If an event's sampling is "slaved" to another, the hashes of the slave
  // and the master are recorded here.
  base::flat_map<uint64_t, uint64_t> event_sampling_master_;

  // Contains data from various recordings which periodically get serialized
  // and cleared by StoreRecordingsInReport() and may be Purged().
  struct Recordings {
    Recordings();
    Recordings& operator=(Recordings&&);
    ~Recordings();

    // Data captured by UpdateSourceUrl().
    std::map<SourceId, std::unique_ptr<UkmSource>> sources;

    // Data captured by AddEntry().
    std::vector<mojom::UkmEntryPtr> entries;

    // Source ids that have been marked as no longer needed, to denote the
    // subset of |sources| that can be purged after next report.
    std::unordered_set<ukm::SourceId> obsolete_source_ids;

    // Web features usage data (data captured by RecordWebDXFeatures()).
    std::map<SourceId, BitSet> webdx_features;

    // URLs of sources that matched a allowlist url, but were not included in
    // the report generated by the last log rotation because we haven't seen any
    // events for that source yet.
    std::unordered_set<std::string> carryover_urls_allowlist;

    // Aggregate information for collected event metrics.
    std::map<uint64_t, EventAggregate> event_aggregations;

    // Aggregated counters about Sources recorded in the current log.
    struct SourceCounts {
      // Count of URLs recorded for all sources.
      size_t observed = 0;
      // Count of URLs recorded for all SourceIdType::NAVIGATION_ID Sources.
      size_t navigation_sources = 0;
      // Sources carried over (not recorded) from a previous logging rotation.
      size_t carryover_sources = 0;

      // Resets all of the data.
      void Reset();
    };
    SourceCounts source_counts;

    // Resets all of the data.
    void Reset();
  };
  Recordings recordings_;

  // The maximum number of Sources we'll keep in memory before discarding any
  // new ones being added.
  size_t max_sources_ = 500;

  // The maximum number of Sources we can keep in memory at the end of the
  // current reporting cycle that will stay accessible in the next reporting
  // interval.
  size_t max_kept_sources_ = 100;

  // The maximum number of Entries we'll keep in memory before discarding any
  // new ones being added.
  size_t max_entries_ = 5000;

  using UkmRecorderObserverList =
      base::ObserverListThreadSafe<UkmRecorderObserver>;
  // Map from event hashes to observers. The key is a set of event hashes that
  // their corresponding value pair will be norified when one of those events
  // is added. The value is a non-empty observer list whose members are
  // observing those events.
  using UkmRecorderObserverMap =
      base::flat_map<base::flat_set<uint64_t> /*event_hashes*/,
                     scoped_refptr<UkmRecorderObserverList>>;
  // Lock used to ensure mutual exclusive access to |observers_|.
  mutable base::Lock lock_;

  // Observers that will be notified on UKM events.
  UkmRecorderObserverMap observers_ GUARDED_BY(lock_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
