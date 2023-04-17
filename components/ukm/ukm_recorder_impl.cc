// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/crc32.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/ukm/scheme_constants.h"
#include "components/ukm/ukm_recorder_observer.h"
#include "components/variations/variations_associated_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder_impl_utils.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "ukm_consent_state.h"
#include "ukm_recorder_impl.h"
#include "url/gurl.h"

namespace ukm {

BASE_FEATURE(kUkmSamplingRateFeature,
             "UkmSamplingRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

bool IsAllowlistedSourceId(SourceId source_id) {
  SourceIdType type = GetSourceIdType(source_id);
  switch (type) {
    case ukm::SourceIdObj::Type::NAVIGATION_ID:
    case ukm::SourceIdObj::Type::APP_ID:
    case ukm::SourceIdObj::Type::HISTORY_ID:
    case ukm::SourceIdObj::Type::WEBAPK_ID:
    case ukm::SourceIdObj::Type::PAYMENT_APP_ID:
    case ukm::SourceIdObj::Type::NO_URL_ID:
    case ukm::SourceIdObj::Type::REDIRECT_ID:
    case ukm::SourceIdObj::Type::WEB_IDENTITY_ID:
    case ukm::SourceIdObj::Type::CHROMEOS_WEBSITE_ID:
    case ukm::SourceIdObj::Type::EXTENSION_ID:
    case ukm::SourceIdObj::Type::SOFT_NAVIGATION_ID: {
      return true;
    }
    case ukm::SourceIdObj::Type::DEFAULT:
    case ukm::SourceIdObj::Type::DESKTOP_WEB_APP_ID:
    case ukm::SourceIdObj::Type::WORKER_ID:
      return false;
  }
}

bool IsAppIdType(SourceId source_id) {
  SourceIdType type = GetSourceIdType(source_id);
  return type == SourceIdType::APP_ID;
}

// Returns whether |url| has one of the schemes supported for logging to UKM.
// URLs with other schemes will not be logged.
bool HasSupportedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kAboutScheme) ||
         url.SchemeIs(kChromeUIScheme) || url.SchemeIs(kExtensionScheme) ||
         url.SchemeIs(kAppScheme);
}

void RecordDroppedSource(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Sources.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void RecordDroppedSource(bool already_recorded_another_reason,
                         DroppedDataReason reason) {
  if (!already_recorded_another_reason)
    RecordDroppedSource(reason);
}

void StoreEntryProto(const mojom::UkmEntry& in, Entry* out) {
  DCHECK(!out->has_source_id());
  DCHECK(!out->has_event_hash());

  out->set_source_id(in.source_id);
  out->set_event_hash(in.event_hash);
  for (const auto& metric : in.metrics) {
    Entry::Metric* proto_metric = out->add_metrics();
    proto_metric->set_metric_hash(metric.first);
    proto_metric->set_value(metric.second);
  }
}

GURL SanitizeURL(const GURL& url) {
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  // chrome:// and about: URLs params are never used for navigation, only to
  // prepopulate data on the page, so don't include their params.
  if (url.SchemeIs(url::kAboutScheme) || url.SchemeIs("chrome")) {
    remove_params.ClearQuery();
  }
  if (url.SchemeIs(kExtensionScheme)) {
    remove_params.ClearPath();
    remove_params.ClearQuery();
    remove_params.ClearRef();
  }
  return url.ReplaceComponents(remove_params);
}

void AppendAllowlistedUrls(
    const std::map<SourceId, std::unique_ptr<UkmSource>>& sources,
    std::unordered_set<std::string>* urls) {
  for (const auto& kv : sources) {
    if (IsAllowlistedSourceId(kv.first)) {
      urls->insert(kv.second->url().spec());
      // Some non-navigation sources only record origin as a URL.
      // Add the origin from the navigation source to match those too.
      urls->insert(kv.second->url().DeprecatedGetOriginAsURL().spec());
    }
  }
}

// Returns true if the event corresponding to |event_hash| has a comprehensive
// decode map that includes all valid metrics.
bool HasComprehensiveDecodeMap(int64_t event_hash) {
  // All events other than "Identifiability" conforms to its decode map.
  // TODO(asanka): It is technically an abstraction violation for
  // //components/ukm to know this fact.
  return event_hash != builders::Identifiability::kEntryNameHash;
}

bool HasUnknownMetrics(const builders::DecodeMap& decode_map,
                       const mojom::UkmEntry& entry) {
  const auto it = decode_map.find(entry.event_hash);
  if (it == decode_map.end())
    return true;
  if (!HasComprehensiveDecodeMap(entry.event_hash))
    return false;
  const auto& metric_map = it->second.metric_map;
  for (const auto& metric : entry.metrics) {
    if (metric_map.count(metric.first) == 0)
      return true;
  }
  return false;
}

}  // namespace

UkmRecorderImpl::UkmRecorderImpl()
    : sampling_seed_(static_cast<uint32_t>(base::RandUint64())) {
  max_kept_sources_ =
      static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
          kUkmFeature, "MaxKeptSources", max_kept_sources_));
}

UkmRecorderImpl::~UkmRecorderImpl() = default;

UkmRecorderImpl::Recordings::Recordings() = default;
UkmRecorderImpl::Recordings& UkmRecorderImpl::Recordings::operator=(
    Recordings&&) = default;
UkmRecorderImpl::Recordings::~Recordings() = default;

void UkmRecorderImpl::Recordings::Reset() {
  *this = Recordings();
}

void UkmRecorderImpl::Recordings::SourceCounts::Reset() {
  *this = SourceCounts();
}

void UkmRecorderImpl::UpdateRecording(ukm::UkmConsentState state) {
  DVLOG(1) << "UkmRecorderImpl::UpdateRecording: " << state.ToEnumBitmask();
  recording_state_ = state;
  EnableRecording();
}

void UkmRecorderImpl::EnableRecording() {
  recording_enabled_ = true;
  OnRecorderParametersChanged();
}

void UkmRecorderImpl::DisableRecording() {
  DVLOG(1) << "UkmRecorderImpl::DisableRecording";
  if (recording_enabled())
    recording_is_continuous_ = false;
  recording_enabled_ = false;
  OnRecorderParametersChanged();
}

void UkmRecorderImpl::SetSamplingForTesting(int rate) {
  sampling_forced_for_testing_ = true;
  default_sampling_rate_ = rate;
  event_sampling_rates_.clear();
}

bool UkmRecorderImpl::ShouldDropEntryForTesting(mojom::UkmEntry* entry) {
  return ShouldDropEntry(entry);
}

bool UkmRecorderImpl::IsSamplingConfigured() const {
  return sampling_forced_for_testing_ ||
         base::FeatureList::IsEnabled(kUkmSamplingRateFeature);
}

void UkmRecorderImpl::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recordings_.Reset();
  recording_is_continuous_ = false;

  NotifyAllObservers(&UkmRecorderObserver::OnPurge);
}

void UkmRecorderImpl::PurgeRecordingsWithUrlScheme(
    const std::string& url_scheme) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Discard all sources that have a URL with the given URL scheme as well as
  // all the entries associated with these sources.
  std::unordered_set<SourceId> relevant_source_ids;
  for (const auto& kv : recordings_.sources) {
    if (kv.second->url().SchemeIs(url_scheme)) {
      relevant_source_ids.insert(kv.first);
    }
  }

  PurgeSourcesAndEventsBySourceIds(relevant_source_ids);
  recording_is_continuous_ = false;

  NotifyAllObservers(&UkmRecorderObserver::OnPurgeRecordingsWithUrlScheme,
                     url_scheme);
}

void UkmRecorderImpl::PurgeRecordingsWithSourceIdType(
    ukm::SourceIdType source_id_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unordered_set<SourceId> relevant_source_ids;

  for (const auto& kv : recordings_.sources) {
    if (GetSourceIdType(kv.first) == source_id_type) {
      relevant_source_ids.insert(kv.first);
    }
  }

  PurgeSourcesAndEventsBySourceIds(relevant_source_ids);
  recording_is_continuous_ = false;
}

void UkmRecorderImpl::PurgeRecordingsWithMsbbSources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unordered_set<SourceId> relevant_source_ids;

  for (const auto& kv : recordings_.sources) {
    if (GetConsentType(GetSourceIdType(kv.first)) == MSBB) {
      relevant_source_ids.insert(kv.first);
    }
  }

  PurgeSourcesAndEventsBySourceIds(relevant_source_ids);
  recording_is_continuous_ = false;
}

void UkmRecorderImpl::PurgeSourcesAndEventsBySourceIds(
    const std::unordered_set<SourceId>& source_ids) {
  for (const auto source_id : source_ids) {
    recordings_.sources.erase(source_id);
  }

  std::vector<mojom::UkmEntryPtr>& events = recordings_.entries;

  events.erase(std::remove_if(events.begin(), events.end(),
                              [&](const auto& event) {
                                return source_ids.count(event->source_id);
                              }),
               events.end());
}

void UkmRecorderImpl::MarkSourceForDeletion(SourceId source_id) {
  if (source_id == kInvalidSourceId)
    return;
  recordings_.obsolete_source_ids.insert(source_id);
}

void UkmRecorderImpl::SetIsWebstoreExtensionCallback(
    const IsWebstoreExtensionCallback& callback) {
  is_webstore_extension_callback_ = callback;
}

void UkmRecorderImpl::SetEntryFilter(
    std::unique_ptr<UkmEntryFilter> entry_filter) {
  DCHECK(!entry_filter_ || !entry_filter);
  entry_filter_ = std::move(entry_filter);
}

void UkmRecorderImpl::AddUkmRecorderObserver(
    const base::flat_set<uint64_t>& event_hashes,
    UkmRecorderObserver* observer) {
  DCHECK(observer);
  {
    base::AutoLock auto_lock(lock_);
    if (!observers_.contains(event_hashes)) {
      observers_.insert(
          {event_hashes, base::MakeRefCounted<UkmRecorderObserverList>()});
    }

    observers_[event_hashes]->AddObserver(observer);
  }
  // Update the UkmRecorderParameters to capture a UKM event which is being
  // observed by any UkmRecorderObserver in |observers_|.
  OnRecorderParametersChanged();
}

void UkmRecorderImpl::RemoveUkmRecorderObserver(UkmRecorderObserver* observer) {
  {
    base::AutoLock auto_lock(lock_);
    for (auto it = observers_.begin(); it != observers_.end();) {
      if (it->second->RemoveObserver(observer) ==
          UkmRecorderObserverList::RemoveObserverResult::kWasOrBecameEmpty) {
        it = observers_.erase(it);
      } else {
        ++it;
      }
    }
  }
  OnRecorderParametersChanged();
}

void UkmRecorderImpl::OnUkmAllowedStateChanged(UkmConsentState state) {
  NotifyAllObservers(&UkmRecorderObserver::OnUkmAllowedStateChanged, state);
}

void UkmRecorderImpl::StoreRecordingsInReport(Report* report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set of source ids seen by entries in recordings_.
  std::set<SourceId> source_ids_seen;
  for (const auto& entry : recordings_.entries) {
    Entry* proto_entry = report->add_entries();
    StoreEntryProto(*entry, proto_entry);
    source_ids_seen.insert(entry->source_id);
  }

  // Number of sources excluded from this report because no entries referred to
  // them.
  const int num_sources_unsent =
      recordings_.sources.size() - source_ids_seen.size();

  // Construct set of allowlisted URLs by merging those carried over from the
  // previous report cycle and those from sources recorded in this cycle.
  std::unordered_set<std::string> url_allowlist;
  recordings_.carryover_urls_allowlist.swap(url_allowlist);
  AppendAllowlistedUrls(recordings_.sources, &url_allowlist);

  // Number of sources discarded due to not matching a navigation URL.
  int num_sources_unmatched = 0;

  std::unordered_map<SourceIdType, int> serialized_source_type_counts;

  for (const auto& kv : recordings_.sources) {
    MaybeMarkForDeletion(kv.first);
    // If the source id is not allowlisted, don't send it unless it has
    // associated entries and the URL matches that of an allowlisted source.
    if (!IsAllowlistedSourceId(kv.first)) {
      // UkmSource should not keep initial_url for non-navigation source IDs.
      DCHECK_EQ(1u, kv.second->urls().size());
      if (!url_allowlist.count(kv.second->url().spec())) {
        RecordDroppedSource(DroppedDataReason::NOT_MATCHED);
        MarkSourceForDeletion(kv.first);
        num_sources_unmatched++;
        continue;
      }
      // Omit entryless sources from the report.
      if (!base::Contains(source_ids_seen, kv.first)) {
        continue;
      }

      // Non-allowlisted Source types will not be kept after entries are
      // logged.
      // We experimented with this in early 2023 and we found keeping sources
      // longer didn't decrease the percentage of sources with null url. See
      // crbug/1358334.
      MarkSourceForDeletion(kv.first);
    }
    // Minimal validations before serializing into a proto message.
    // See crbug/1274876.
    DCHECK_NE(kv.second->id(), ukm::kInvalidSourceId);
    DCHECK_NE(kv.second->urls().size(), 0u);
    Source* proto_source = report->add_sources();
    kv.second->PopulateProto(proto_source);

    serialized_source_type_counts[GetSourceIdType(kv.first)]++;
  }

  for (const auto& event_and_aggregate : recordings_.event_aggregations) {
    Aggregate* proto_aggregate = report->add_aggregates();
    proto_aggregate->set_event_hash(event_and_aggregate.first);

    const EventAggregate& event_aggregate = event_and_aggregate.second;
    event_aggregate.FillProto(proto_aggregate);
  }
  int num_serialized_sources = 0;
  for (const auto& source_type_and_count : serialized_source_type_counts) {
    num_serialized_sources += source_type_and_count.second;
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount2",
                            num_serialized_sources);
  UMA_HISTOGRAM_COUNTS_100000("UKM.Entries.SerializedCount2",
                              recordings_.entries.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnsentSourcesCount",
                            num_sources_unsent);
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnmatchedSourcesCount",
                            num_sources_unmatched);

  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.Default",
      serialized_source_type_counts[SourceIdType::DEFAULT]);
  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.Navigation",
      serialized_source_type_counts[SourceIdType::NAVIGATION_ID]);
  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.App",
      serialized_source_type_counts[SourceIdType::APP_ID]);

  // We record a UMA metric specifically for the number of serialized events
  // with the FCP metric. This is for data quality verification.
  const uint64_t pageload_hash =
      base::HashMetricName(ukm::builders::PageLoad::kEntryName);
  const uint64_t fcp_hash = base::HashMetricName(
      ukm::builders::PageLoad::
          kPaintTiming_NavigationToFirstContentfulPaintName);
  int num_recorded_fcp = 0;
  for (const auto& entry : recordings_.entries) {
    if (entry->event_hash == pageload_hash) {
      if (entry->metrics.find(fcp_hash) != entry->metrics.end()) {
        num_recorded_fcp++;
      }
    }
  }
  UMA_HISTOGRAM_COUNTS_100000("UKM.Entries.SerializedCountFCP",
                              num_recorded_fcp);

  // For each matching id in obsolete_source_ids, remove the Source from
  // recordings_.sources. The remaining sources form the deferred sources for
  // the next report.
  for (const SourceId& source_id : recordings_.obsolete_source_ids) {
    recordings_.sources.erase(source_id);
  }
  recordings_.obsolete_source_ids.clear();

  // Populate SourceCounts field on the report then clear the recordings.
  Report::SourceCounts* source_counts_proto = report->mutable_source_counts();
  source_counts_proto->set_observed(recordings_.source_counts.observed);
  source_counts_proto->set_navigation_sources(
      recordings_.source_counts.navigation_sources);
  source_counts_proto->set_unmatched_sources(num_sources_unmatched);
  source_counts_proto->set_carryover_sources(
      recordings_.source_counts.carryover_sources);

  recordings_.source_counts.Reset();
  recordings_.entries.clear();
  recordings_.event_aggregations.clear();

  report->set_is_continuous(recording_is_continuous_);
  recording_is_continuous_ = true;

  int pruned_sources_age_sec = PruneData(source_ids_seen);

  // Record how old the newest truncated source is.
  source_counts_proto->set_pruned_sources_age_seconds(pruned_sources_age_sec);

  // Set deferred sources count after pruning.
  source_counts_proto->set_deferred_sources(recordings_.sources.size());
  // Same value as the deferred source count, for setting the carryover count
  // in the next reporting cycle.
  recordings_.source_counts.carryover_sources = recordings_.sources.size();

  // We already matched these deferred sources against the URL allowlist.
  // Re-allowlist them for the next report.
  for (const auto& kv : recordings_.sources) {
    recordings_.carryover_urls_allowlist.insert(kv.second->url().spec());
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.KeptSourcesCount",
                            recordings_.sources.size());

  // Record number of sources after pruning that were carried over due to not
  // having any events in this reporting cycle.
  int num_sources_entryless = 0;
  for (const auto& kv : recordings_.sources) {
    if (!base::Contains(source_ids_seen, kv.first)) {
      num_sources_entryless++;
    }
  }
  source_counts_proto->set_entryless_sources(num_sources_entryless);

  // Notify observers that a report was generated.
  if (entry_filter_) {
    entry_filter_->OnStoreRecordingsInReport();
  }
}

int UkmRecorderImpl::PruneData(std::set<SourceId>& source_ids_seen) {
  // Modify the set source_ids_seen by removing sources that aren't in
  // recordings_. We do this here as there is a few places for
  // recordings_.sources to be modified. The resulting set will be currently
  // existing sources that were seen in this report.
  auto it = source_ids_seen.begin();
  while (it != source_ids_seen.end()) {
    if (!base::Contains(recordings_.sources, *it)) {
      it = source_ids_seen.erase(it);
    } else {
      it++;
    }
  }

  // Build the set of sources that exist in recordings_.sources that were not
  // seen in this report.
  std::set<SourceId> source_ids_unseen;
  for (const auto& kv : recordings_.sources) {
    if (!base::Contains(source_ids_seen, kv.first)) {
      source_ids_unseen.insert(kv.first);
    }
  }

  // Special case APP_IDs. Ideally this is not going to exist for too long, as
  // it would be preferable to have a more general purpose solution.
  std::set<SourceId> source_ids_app_id;

  // Only done if we are in the experiment that will leave APP_ID metrics for
  // last when pruning. This block extracts out all source_ids from the
  // seen/unseen lists and stores them in |source_ids_app_id|.
  if (base::GetFieldTrialParamByFeatureAsBool(kUkmFeature, "PruneAppIdLast",
                                              false)) {
    it = source_ids_seen.begin();
    while (it != source_ids_seen.end()) {
      if (IsAppIdType(*it)) {
        source_ids_app_id.insert(*it);
        it = source_ids_seen.erase(it);
      } else {
        it++;
      }
    }

    it = source_ids_unseen.begin();
    while (it != source_ids_unseen.end()) {
      if (IsAppIdType(*it)) {
        source_ids_app_id.insert(*it);
        it = source_ids_unseen.erase(it);
      } else {
        it++;
      }
    }
  }

  int pruned_sources_age_sec = 0;
  int num_sources = recordings_.sources.size();
  // Setup an experiment to test what will occur if we prune unseen sources
  // first.
  if (base::GetFieldTrialParamByFeatureAsBool(
          kUkmFeature, "PruneUnseenSourcesFirst", false)) {
    int pruned_sources_age_from_unseen_sec =
        PruneOldSources(max_kept_sources_, source_ids_unseen);

    UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.NumUnseen",
                               num_sources - recordings_.sources.size());
    num_sources = recordings_.sources.size();

    // Prune again from seen sources. Note that if we've already pruned enough
    // from the unseen sources, this will be a noop.
    int pruned_sources_age_from_seen_sec =
        PruneOldSources(max_kept_sources_, source_ids_seen);

    UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.NumSeen",
                               num_sources - recordings_.sources.size());
    num_sources = recordings_.sources.size();

    int pruned_sources_age_from_app_id_sec = 0;

    // Technically this should be fine without the feature, since the group
    // will be empty, but might as well add the feature check.
    // Still prune the APP_ID entries. We don't want it to be unbounded, but
    // providing a higher default here in case.
    if (base::GetFieldTrialParamByFeatureAsBool(kUkmFeature, "PruneAppIdLast",
                                                false)) {
      pruned_sources_age_from_app_id_sec =
          PruneOldSources(500, source_ids_app_id);

      UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.NumAppId",
                                 num_sources - recordings_.sources.size());
    }

    // We're looking for the newest age, which will be the largest between the
    // two sets we pruned from.
    pruned_sources_age_sec = std::max({pruned_sources_age_from_unseen_sec,
                                       pruned_sources_age_from_seen_sec,
                                       pruned_sources_age_from_app_id_sec});

  } else {
    // In this case, we prune all sources without caring if they were seen or
    // not. Make a set of all existing sources so we can use the same
    // PruneOldSources method.
    std::set<SourceId> all_sources;
    for (const auto& kv : recordings_.sources) {
      all_sources.insert(kv.first);
    }
    if (base::GetFieldTrialParamByFeatureAsBool(kUkmFeature, "PruneAppIdLast",
                                                false)) {
      std::set<SourceId> all_sources_without_app_id;

      // This will put into |all_sources_without_app_id| the set of
      // |all_sources| - |source_ids_app_id|.
      std::set_difference(all_sources.begin(), all_sources.end(),
                          source_ids_app_id.begin(), source_ids_app_id.end(),
                          std::inserter(all_sources_without_app_id,
                                        all_sources_without_app_id.end()));

      // Now, prune the non-APP_ID, then the APP_ID.
      int pruned_sources_age_sec_non_app_id =
          PruneOldSources(max_kept_sources_, all_sources_without_app_id);

      UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.AppExpNumNonAppId",
                                 num_sources - recordings_.sources.size());
      num_sources = recordings_.sources.size();

      int pruned_sources_age_sec_app_id =
          PruneOldSources(500, source_ids_app_id);

      UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.AppExpNumAppId",
                                 num_sources - recordings_.sources.size());

      pruned_sources_age_sec = std::max(pruned_sources_age_sec_non_app_id,
                                        pruned_sources_age_sec_app_id);

    } else {
      pruned_sources_age_sec = PruneOldSources(max_kept_sources_, all_sources);
      UMA_HISTOGRAM_COUNTS_10000("UKM.PrunedSources.NoExp",
                                 num_sources - recordings_.sources.size());
    }
  }
  return pruned_sources_age_sec;
}

bool UkmRecorderImpl::ShouldDropEntry(mojom::UkmEntry* entry) {
  if (!recording_enabled()) {
    RecordDroppedEntry(entry->event_hash,
                       DroppedDataReason::RECORDING_DISABLED);
    return true;
  }

  const auto required_consent =
      GetConsentType(GetSourceIdType(entry->source_id));

  if (!recording_enabled(required_consent)) {
    if (required_consent == UkmConsentType::MSBB) {
      RecordDroppedEntry(entry->event_hash,
                         DroppedDataReason::MSBB_CONSENT_DISABLED);

    } else {
      RecordDroppedEntry(entry->event_hash,
                         DroppedDataReason::APPS_CONSENT_DISABLED);
    }
    return true;
  }

  if (!ApplyEntryFilter(entry)) {
    RecordDroppedEntry(entry->event_hash,
                       DroppedDataReason::REJECTED_BY_FILTER);
    return true;
  }

  return false;
}

bool UkmRecorderImpl::ApplyEntryFilter(mojom::UkmEntry* entry) {
  base::flat_set<uint64_t> dropped_metric_hashes;

  if (!entry_filter_)
    return true;

  bool keep_entry = entry_filter_->FilterEntry(entry, &dropped_metric_hashes);

  for (auto metric : dropped_metric_hashes) {
    recordings_.event_aggregations[entry->event_hash]
        .metrics[metric]
        .dropped_due_to_filter++;
  }

  if (!keep_entry) {
    recordings_.event_aggregations[entry->event_hash].dropped_due_to_filter++;
    return false;
  }
  return true;
}

int UkmRecorderImpl::PruneOldSources(size_t max_kept_sources,
                                     const std::set<SourceId>& pruning_set) {
  long num_prune_required = recordings_.sources.size() - max_kept_sources;
  // In either case here, nothing to be done.
  if (num_prune_required <= 0 || pruning_set.size() == 0)
    return 0;

  // We can prune everything, so let's do that directly.
  if (static_cast<unsigned long>(num_prune_required) >= pruning_set.size()) {
    base::TimeTicks pruned_sources_age = base::TimeTicks();
    for (const auto& source_id : pruning_set) {
      auto creation_time = recordings_.sources[source_id]->creation_time();
      if (creation_time > pruned_sources_age)
        pruned_sources_age = creation_time;

      recordings_.sources.erase(source_id);
    }
    base::TimeDelta age_delta = base::TimeTicks::Now() - pruned_sources_age;
    // Technically the age we return here isn't quite right, this is the age of
    // the newest element of the pruned set, while we actually want the age of
    // the last one kept. However it's very unlikely to make a difference in
    // practice as if all are pruned here, it is very likely we'll need to prune
    // from the seen set next. Since it would be logically quite a bit more
    // complex to get this exactly right, it's ok for this to be very slightly
    // off in an edge case just to keep complexity down.
    return age_delta.InSeconds();
  }

  // In this case we cannot prune everything, so we will select only the oldest
  // sources to prune.

  // Build a list of timestamp->source pairs for all source we consider for
  // pruning.
  std::vector<std::pair<base::TimeTicks, SourceId>> timestamp_source_id_pairs;
  for (const auto& source_id : pruning_set) {
    auto creation_time = recordings_.sources[source_id]->creation_time();
    timestamp_source_id_pairs.emplace_back(
        std::make_pair(creation_time, source_id));
  }

  // Partially sort so that the last |num_prune_required| elements are the
  // newest.
  std::nth_element(timestamp_source_id_pairs.begin(),
                   timestamp_source_id_pairs.end() - num_prune_required,
                   timestamp_source_id_pairs.end());

  // Actually prune |num_prune_required| sources.
  for (int i = 0; i < num_prune_required; i++) {
    auto source_id = timestamp_source_id_pairs[i].second;
    recordings_.sources.erase(source_id);
  }

  base::TimeDelta pruned_sources_age =
      base::TimeTicks::Now() -
      (timestamp_source_id_pairs.end() - (num_prune_required + 1))->first;

  return pruned_sources_age.InSeconds();
}

void UkmRecorderImpl::UpdateSourceURL(SourceId source_id,
                                      const GURL& unsanitized_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetSourceIdType(source_id) != SourceIdType::NO_URL_ID);

  if (base::Contains(recordings_.sources, source_id))
    return;

  const GURL sanitized_url = SanitizeURL(unsanitized_url);
  if (ShouldRecordUrl(source_id, sanitized_url) ==
      ShouldRecordUrlResult::kDropped) {
    return;
  }
  RecordSource(std::make_unique<UkmSource>(source_id, sanitized_url));
}

void UkmRecorderImpl::UpdateAppURL(SourceId source_id,
                                   const GURL& url,
                                   const AppType app_type) {
  if (app_type != AppType::kPWA && !recording_enabled(ukm::EXTENSIONS)) {
    RecordDroppedSource(DroppedDataReason::EXTENSION_URLS_DISABLED);
    return;
  }
  UpdateSourceURL(source_id, url);
}

void UkmRecorderImpl::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& unsanitized_navigation_data) {
  DCHECK(GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID);
  DCHECK(!base::Contains(recordings_.sources, source_id));
  // TODO(csharrison): Consider changing this behavior so the Source isn't even
  // recorded at all if the final URL in |unsanitized_navigation_data| should
  // not be recorded.
  std::vector<GURL> urls;
  for (const GURL& url : unsanitized_navigation_data.urls) {
    const GURL sanitized_url = SanitizeURL(url);
    if (ShouldRecordUrl(source_id, sanitized_url) !=
        ShouldRecordUrlResult::kDropped) {
      urls.push_back(std::move(sanitized_url));
    }
  }

  // None of the URLs passed the ShouldRecordUrl check, so do not create a new
  // Source for them.
  if (urls.empty())
    return;

  UkmSource::NavigationData sanitized_navigation_data =
      unsanitized_navigation_data.CopyWithSanitizedUrls(urls);
  RecordSource(
      std::make_unique<UkmSource>(source_id, sanitized_navigation_data));
}

// static:
UkmConsentType UkmRecorderImpl::GetConsentType(SourceIdType type) {
  switch (type) {
    case SourceIdType::APP_ID:
      return UkmConsentType::APPS;
    case SourceIdType::DEFAULT:
    case SourceIdType::NAVIGATION_ID:
    case SourceIdType::HISTORY_ID:
    case SourceIdType::WEBAPK_ID:
    case SourceIdType::PAYMENT_APP_ID:
    case SourceIdType::DESKTOP_WEB_APP_ID:
    case SourceIdType::WORKER_ID:
    case SourceIdType::NO_URL_ID:
    case SourceIdType::REDIRECT_ID:
    case SourceIdType::WEB_IDENTITY_ID:
    case SourceIdType::CHROMEOS_WEBSITE_ID:
    case SourceIdType::EXTENSION_ID:
    case SourceIdType::SOFT_NAVIGATION_ID:
      return UkmConsentType::MSBB;
  }
  return UkmConsentType::MSBB;
}

UkmRecorderImpl::EventAggregate::EventAggregate() = default;
UkmRecorderImpl::EventAggregate::~EventAggregate() = default;

void UkmRecorderImpl::EventAggregate::FillProto(
    Aggregate* proto_aggregate) const {
  proto_aggregate->set_source_id(0);  // Across all sources.
  proto_aggregate->set_total_count(total_count);
  proto_aggregate->set_dropped_due_to_limits(dropped_due_to_limits);
  proto_aggregate->set_dropped_due_to_sampling(dropped_due_to_sampling);
  proto_aggregate->set_dropped_due_to_filter(dropped_due_to_filter);
  proto_aggregate->set_dropped_due_to_unconfigured(dropped_due_to_unconfigured);
  for (const auto& metric_and_aggregate : metrics) {
    const MetricAggregate& aggregate = metric_and_aggregate.second;
    Aggregate::Metric* proto_metric = proto_aggregate->add_metrics();
    proto_metric->set_metric_hash(metric_and_aggregate.first);
    proto_metric->set_value_sum(aggregate.value_sum);
    proto_metric->set_value_square_sum(aggregate.value_square_sum);
    if (aggregate.total_count != total_count) {
      proto_metric->set_total_count(aggregate.total_count);
    }
    if (aggregate.dropped_due_to_limits != dropped_due_to_limits) {
      proto_metric->set_dropped_due_to_limits(aggregate.dropped_due_to_limits);
    }
    if (aggregate.dropped_due_to_sampling != dropped_due_to_sampling) {
      proto_metric->set_dropped_due_to_sampling(
          aggregate.dropped_due_to_sampling);
    }
    if (aggregate.dropped_due_to_filter != dropped_due_to_filter) {
      proto_metric->set_dropped_due_to_filter(aggregate.dropped_due_to_filter);
    }
    if (aggregate.dropped_due_to_unconfigured != dropped_due_to_unconfigured) {
      proto_metric->set_dropped_due_to_unconfigured(
          aggregate.dropped_due_to_unconfigured);
    }
  }
}

void UkmRecorderImpl::MaybeMarkForDeletion(SourceId source_id) {
  SourceIdType type = GetSourceIdType(source_id);
  switch (type) {
    case ukm::SourceIdObj::Type::HISTORY_ID:
    case ukm::SourceIdObj::Type::WEBAPK_ID:
    case ukm::SourceIdObj::Type::PAYMENT_APP_ID:
    case ukm::SourceIdObj::Type::NO_URL_ID:
    case ukm::SourceIdObj::Type::WEB_IDENTITY_ID:
    case ukm::SourceIdObj::Type::CHROMEOS_WEBSITE_ID:
    case ukm::SourceIdObj::Type::EXTENSION_ID: {
      // Don't keep sources of these types after current report because their
      // entries are logged only at source creation time.
      MarkSourceForDeletion(source_id);
      break;
    }
    case ukm::SourceIdObj::Type::DEFAULT:
    case ukm::SourceIdObj::Type::APP_ID:
    case ukm::SourceIdObj::Type::DESKTOP_WEB_APP_ID:
    case ukm::SourceIdObj::Type::NAVIGATION_ID:
    case ukm::SourceIdObj::Type::WORKER_ID:
    case ukm::SourceIdObj::Type::REDIRECT_ID:
    case ukm::SourceIdObj::Type::SOFT_NAVIGATION_ID:
      break;
  }
}

// Extension URLs need to be specifically enabled and the extension synced.
bool UkmRecorderImpl::ShouldDropExtensionUrl(
    const GURL& sanitized_extension_url,
    bool has_recorded_reason) const {
  DCHECK_EQ(sanitized_extension_url.GetWithEmptyPath(),
            sanitized_extension_url);

  // If the URL scheme is not extension scheme, drop the record with
  // `EXTENSION_URL_INVALID`.
  if (!sanitized_extension_url.SchemeIs(kExtensionScheme)) {
    RecordDroppedSource(has_recorded_reason,
                        DroppedDataReason::EXTENSION_URL_INVALID);
    return true;
  }
  // If the recording is not enabled for extensions, drop the record with
  // `EXTENSION_URLS_DISABLED`.
  if (!recording_enabled(ukm::EXTENSIONS)) {
    RecordDroppedSource(has_recorded_reason,
                        DroppedDataReason::EXTENSION_URLS_DISABLED);
    return true;
  }
  // If the extension is not a webstore extension, drop the record with
  // `EXTENSION_NOT_SYNCED`.
  if (!is_webstore_extension_callback_ ||
      !is_webstore_extension_callback_.Run(
          sanitized_extension_url.host_piece())) {
    RecordDroppedSource(has_recorded_reason,
                        DroppedDataReason::EXTENSION_NOT_SYNCED);
    return true;
  }

  return false;
}

UkmRecorderImpl::ShouldRecordUrlResult UkmRecorderImpl::ShouldRecordUrl(
    SourceId source_id,
    const GURL& sanitized_url) const {
  ShouldRecordUrlResult result = ShouldRecordUrlResult::kOk;
  bool has_recorded_reason = false;
  if (!recording_enabled()) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    // Don't return the result yet. Check if the we are allowed to notify
    // observers, as they may rely on the not uploaded metrics to determine
    // how some features should work.
    result = ShouldRecordUrlResult::kObserverOnly;
    has_recorded_reason = true;
  }

  const auto required_consent = GetConsentType(GetSourceIdType(source_id));

  if (!recording_enabled(required_consent)) {
    if (required_consent == UkmConsentType::MSBB) {
      RecordDroppedSource(has_recorded_reason,
                          DroppedDataReason::MSBB_CONSENT_DISABLED);

    } else {
      RecordDroppedSource(has_recorded_reason,
                          DroppedDataReason::APPS_CONSENT_DISABLED);
    }
    return ShouldRecordUrlResult::kDropped;
  }

  if (recordings_.sources.size() >= max_sources_) {
    RecordDroppedSource(has_recorded_reason, DroppedDataReason::MAX_HIT);
    return ShouldRecordUrlResult::kDropped;
  }

  if (sanitized_url.is_empty()) {
    RecordDroppedSource(has_recorded_reason, DroppedDataReason::EMPTY_URL);
    return ShouldRecordUrlResult::kDropped;
  }

  if (!HasSupportedScheme(sanitized_url)) {
    RecordDroppedSource(has_recorded_reason,
                        DroppedDataReason::UNSUPPORTED_URL_SCHEME);
    DVLOG(2) << "Dropped Unsupported UKM URL:" << source_id << ":"
             << sanitized_url.spec();
    return ShouldRecordUrlResult::kDropped;
  }

  if (GetSourceIdType(source_id) == SourceIdType::EXTENSION_ID) {
    if (ShouldDropExtensionUrl(sanitized_url, has_recorded_reason)) {
      return ShouldRecordUrlResult::kDropped;
    }
  }

  // Ideally, this check should be covered by the above block for
  // `EXTENSION_ID` type. For backward compatibility we still keep it here so
  // the UKMs recorded without `EXTENSION_ID` type are also properly checked.
  // TODO(https://crbug.com/1393445): clean up all the UKM metrics with
  // extension URL to use the dedicated source ID type, and remove this check.
  if (sanitized_url.SchemeIs(kExtensionScheme)) {
    if (ShouldDropExtensionUrl(sanitized_url, has_recorded_reason)) {
      return ShouldRecordUrlResult::kDropped;
    }
  }
  return result;
}

void UkmRecorderImpl::RecordSource(std::unique_ptr<UkmSource> source) {
  SourceId source_id = source->id();
  // If UKM recording is disabled due to |recording_enabled|,
  // still notify observers as they might be interested in it.
  NotifyAllObservers(&UkmRecorderObserver::OnUpdateSourceURL, source_id,
                     source->urls());

  if (!recording_enabled()) {
    return;
  }

  const auto required_consent = GetConsentType(GetSourceIdType(source_id));

  if (!recording_enabled(required_consent)) {
    return;
  }

  if (GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID)
    recordings_.source_counts.navigation_sources++;
  recordings_.source_counts.observed++;
  recordings_.sources.emplace(source_id, std::move(source));
}

void UkmRecorderImpl::AddEntry(mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!HasUnknownMetrics(decode_map_, *entry));

  NotifyObserversWithNewEntry(*entry);

  if (ShouldDropEntry(entry.get()))
    return;

  EventAggregate& event_aggregate =
      recordings_.event_aggregations[entry->event_hash];
  event_aggregate.total_count++;
  for (const auto& metric : entry->metrics) {
    MetricAggregate& aggregate = event_aggregate.metrics[metric.first];
    double value = metric.second;
    aggregate.total_count++;
    aggregate.value_sum += value;
    aggregate.value_square_sum += value * value;
  }

  if (!IsSamplingConfigured()) {
    RecordDroppedEntry(entry->event_hash,
                       DroppedDataReason::SAMPLING_UNCONFIGURED);
    event_aggregate.dropped_due_to_unconfigured++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_unconfigured++;
    return;
  }

  if (default_sampling_rate_ < 0) {
    LoadExperimentSamplingInfo();
  }

  bool sampled_in = IsSampledIn(entry->source_id, entry->event_hash);

  if (!sampled_in) {
    RecordDroppedEntry(entry->event_hash, DroppedDataReason::SAMPLED_OUT);
    event_aggregate.dropped_due_to_sampling++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_sampling++;
    return;
  }

  if (recordings_.entries.size() >= max_entries_) {
    RecordDroppedEntry(entry->event_hash, DroppedDataReason::MAX_HIT);
    event_aggregate.dropped_due_to_limits++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_limits++;
    return;
  }

  // Log a corresponding entry to UMA so we get a per-metric breakdown of UKM
  // entry counts.
  // Truncate the unsigned 64-bit hash to 31 bits, to
  // make it a suitable histogram sample.
  UMA_HISTOGRAM_SPARSE("UKM.Entries.Recorded.ByEntryHash",
                       entry->event_hash & 0x7fffffff);

  recordings_.entries.push_back(std::move(entry));
}

void UkmRecorderImpl::LoadExperimentSamplingInfo() {
  // This should be called only if a sampling rate hasn't been loaded.
  DCHECK_LT(default_sampling_rate_, 0);

  // Default rate must be >= 0 to indicate that load is complete.
  default_sampling_rate_ = 1;

  // If we don't have the feature, no parameters to load.
  if (!base::FeatureList::IsEnabled(kUkmSamplingRateFeature)) {
    return;
  }

  // Check the parameters for sampling controls.
  std::map<std::string, std::string> params;
  if (base::GetFieldTrialParamsByFeature(kUkmSamplingRateFeature, &params)) {
    LoadExperimentSamplingParams(params);
  }
}

void UkmRecorderImpl::LoadExperimentSamplingParams(
    const std::map<std::string, std::string>& params) {
  for (const auto& kv : params) {
    const std::string& key = kv.first;
    if (key.length() == 0)
      continue;

    // Keys starting with an underscore are global configuration.
    if (key.at(0) == '_') {
      if (key == "_default_sampling") {
        int sampling;
        // We only load non-negative global sampling rates.
        if (base::StringToInt(kv.second, &sampling) && sampling >= 0)
          default_sampling_rate_ = sampling;
      }
      continue;
    }

    // Anything else is an event name.
    int sampling;
    auto hash = base::HashMetricName(key);
    if (base::StringToInt(kv.second, &sampling)) {
      // If the parameter is a number then that's the sampling rate.
      if (sampling >= 0)
        event_sampling_rates_[hash] = sampling;
    } else {
      // If the parameter is a string then it's the name of another metric
      // to which it should be slaved. This allows different metrics to be
      // sampled in or out together.
      event_sampling_master_[hash] = base::HashMetricName(kv.second);
    }
  }
}

bool UkmRecorderImpl::IsSampledIn(int64_t source_id, uint64_t event_id) {
  // Determine the sampling rate. It's one of:
  // - the default
  // - an explicit sampling rate
  // - a group sampling rate
  int sampling_rate = default_sampling_rate_;
  uint64_t sampling_hash = event_id;
  auto master_found = event_sampling_master_.find(sampling_hash);
  if (master_found != event_sampling_master_.end()) {
    sampling_hash = master_found->second;
  }
  auto rate_found = event_sampling_rates_.find(sampling_hash);
  if (rate_found != event_sampling_rates_.end()) {
    sampling_rate = rate_found->second;
  }

  return IsSampledIn(source_id, sampling_hash, sampling_rate);
}

bool UkmRecorderImpl::IsSampledIn(int64_t source_id,
                                  uint64_t event_id,
                                  int sampling_rate) {
  // A sampling rate of 0 is "never"; everything else is 1-in-N but calculated
  // deterministically based on a seed, the source-id, and the event-id. Skip
  // the calculation, though, if N==1 because it will always be true. A negative
  // rate means "unset"; treat it like "never".
  if (sampling_rate <= 0)
    return false;
  if (sampling_rate == 1)
    return true;

  // Mutate the "sampling seed" number in a predictable manner based on the
  // source and event IDs. This makes the result of this function be always
  // the same for the same input parameters (since the seed is fixed during
  // construction of this object) which is important for proper sampling
  // behavior. CRC32 is fast and statistically random enough for these
  // purposes.
  uint32_t sampled_num = sampling_seed_;
  sampled_num = base::Crc32(sampled_num, &source_id, sizeof(source_id));
  sampled_num = base::Crc32(sampled_num, &event_id, sizeof(event_id));

  return sampled_num % sampling_rate == 0;
}

void UkmRecorderImpl::InitDecodeMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decode_map_ = builders::CreateDecodeMap();
}

void UkmRecorderImpl::NotifyObserversWithNewEntry(
    const mojom::UkmEntry& entry) {
  TRACE_EVENT("toplevel", "UkmRecorderImpl::NotifyObserversWithNewEntry");

  base::AutoLock auto_lock(lock_);

  for (const auto& observer : observers_) {
    if (observer.first.contains(entry.event_hash)) {
      TRACE_EVENT(
          "toplevel",
          "UkmRecorderImpl::NotifyObserversWithNewEntry NotifyObserver");
      mojom::UkmEntryPtr cloned = entry.Clone();
      observer.second->Notify(FROM_HERE, &UkmRecorderObserver::OnEntryAdded,
                              base::Passed(&cloned));
    }
  }
}

template <typename Method, typename... Params>
void UkmRecorderImpl::NotifyAllObservers(Method m, Params&&... params) {
  base::AutoLock auto_lock(lock_);
  for (const auto& observer : observers_) {
    observer.second->Notify(FROM_HERE, m, std::forward<Params>(params)...);
  }
}

std::set<uint64_t> UkmRecorderImpl::GetObservedEventHashes() {
  base::AutoLock lock(lock_);
  std::set<uint64_t> hashes;
  for (const auto& observer : observers_) {
    hashes.insert(observer.first.begin(), observer.first.end());
  }
  return hashes;
}

}  // namespace ukm
