// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/crc32.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/ukm/scheme_constants.h"
#include "components/variations/variations_associated_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

namespace ukm {

namespace {

const base::Feature kUkmSamplingRateFeature{"UkmSamplingRate",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Gets the list of whitelisted Entries as string. Format is a comma separated
// list of Entry names (as strings).
std::string GetWhitelistEntries() {
  return base::GetFieldTrialParamValueByFeature(kUkmFeature,
                                                "WhitelistEntries");
}

bool IsWhitelistedSourceId(SourceId source_id) {
  return GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID ||
         GetSourceIdType(source_id) == SourceIdType::APP_ID ||
         GetSourceIdType(source_id) == SourceIdType::HISTORY_ID;
}

// Gets the maximum number of Sources we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxSources() {
  constexpr size_t kDefaultMaxSources = 500;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxSources", kDefaultMaxSources));
}

// Gets the maximum number of Sources we can keep in memory at the end of the
// current reporting cycle that will stay accessible in the next reporting
// interval.
size_t GetMaxKeptSources() {
  constexpr size_t kDefaultMaxKeptSources = 100;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxKeptSources", kDefaultMaxKeptSources));
}

// Gets the maximum number of Entries we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxEntries() {
  constexpr size_t kDefaultMaxEntries = 5000;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxEntries", kDefaultMaxEntries));
}

// Returns whether |url| has one of the schemes supported for logging to UKM.
// URLs with other schemes will not be logged.
bool HasSupportedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIs(url::kAboutScheme) || url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kExtensionScheme) || url.SchemeIs(kAppScheme);
}

enum class DroppedDataReason {
  NOT_DROPPED = 0,
  RECORDING_DISABLED = 1,
  MAX_HIT = 2,
  NOT_WHITELISTED = 3,
  UNSUPPORTED_URL_SCHEME = 4,
  SAMPLED_OUT = 5,
  EXTENSION_URLS_DISABLED = 6,
  EXTENSION_NOT_SYNCED = 7,
  NOT_MATCHED = 8,
  EMPTY_URL = 9,
  NUM_DROPPED_DATA_REASONS
};

void RecordDroppedSource(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Sources.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void RecordDroppedEntry(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Entries.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
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

void AppendWhitelistedUrls(
    const std::map<SourceId, std::unique_ptr<UkmSource>>& sources,
    std::unordered_set<std::string>* urls) {
  for (const auto& kv : sources) {
    if (IsWhitelistedSourceId(kv.first)) {
      urls->insert(kv.second->url().spec());
      // Some non-navigation sources only record origin as a URL.
      // Add the origin from the navigation source to match those too.
      urls->insert(kv.second->url().GetOrigin().spec());
    }
  }
}

bool HasUnknownMetrics(const builders::DecodeMap& decode_map,
                       const mojom::UkmEntry& entry) {
  const auto it = decode_map.find(entry.event_hash);
  if (it == decode_map.end())
    return true;
  const auto& metric_map = it->second.metric_map;
  for (const auto& metric : entry.metrics) {
    if (metric_map.count(metric.first) == 0)
      return true;
  }
  return false;
}

}  // namespace

UkmRecorderImpl::UkmRecorderImpl()
    : recording_enabled_(false),
      sampling_seed_(static_cast<uint32_t>(base::RandUint64())) {}
UkmRecorderImpl::~UkmRecorderImpl() = default;

// static
void UkmRecorderImpl::CreateFallbackSamplingTrial(
    bool is_stable_channel,
    base::FeatureList* feature_list) {
  static const char kSampledGroup_Stable[] = "Sampled_NoSeed_Stable";
  static const char kSampledGroup_Other[] = "Sampled_NoSeed_Other";
  const char* sampled_group = kSampledGroup_Other;
  int default_sampling = 1;  // Sampling is 1-in-N; this is N.

  // Nothing is sampled out except for "stable" which omits almost everything
  // in this configuration. This is done so that clients that fail to receive
  // a configuration from the server do not bias aggregated results because
  // of a relatively large number of records from them.
  if (is_stable_channel) {
    sampled_group = kSampledGroup_Stable;
    default_sampling = 1000000;
  }

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kUkmSamplingRateFeature.name, 100, sampled_group,
          base::FieldTrial::ONE_TIME_RANDOMIZED, nullptr));

  // Everybody (100%) should have a sampling configuration.
  std::map<std::string, std::string> params = {
      {"_default_sampling", base::NumberToString(default_sampling)}};
  variations::AssociateVariationParams(trial->trial_name(), sampled_group,
                                       params);
  trial->AppendGroup(sampled_group, 100);

  // Setup the feature.
  feature_list->RegisterFieldTrialOverride(
      kUkmSamplingRateFeature.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
}

UkmRecorderImpl::EventAggregate::EventAggregate() = default;
UkmRecorderImpl::EventAggregate::~EventAggregate() = default;

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

void UkmRecorderImpl::EnableRecording(bool extensions) {
  DVLOG(1) << "UkmRecorderImpl::EnableRecording, extensions=" << extensions;
  recording_enabled_ = true;
  extensions_enabled_ = extensions;
}

void UkmRecorderImpl::DisableRecording() {
  DVLOG(1) << "UkmRecorderImpl::DisableRecording";
  if (recording_enabled_)
    recording_is_continuous_ = false;
  recording_enabled_ = false;
  extensions_enabled_ = false;
}

void UkmRecorderImpl::DisableSamplingForTesting() {
  sampling_enabled_ = false;
}

bool UkmRecorderImpl::IsSamplingEnabled() const {
  return sampling_enabled_ &&
         base::FeatureList::IsEnabled(kUkmSamplingRateFeature);
}

void UkmRecorderImpl::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recordings_.Reset();
  recording_is_continuous_ = false;
}

void UkmRecorderImpl::PurgeExtensionRecordings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Discard all sources that have an extension URL as well as all the entries
  // related to any of these sources.
  std::unordered_set<SourceId> extension_source_ids;
  for (const auto& kv : recordings_.sources) {
    if (kv.second->url().SchemeIs(kExtensionScheme)) {
      extension_source_ids.insert(kv.first);
    }
  }
  for (const auto source_id : extension_source_ids) {
    recordings_.sources.erase(source_id);
  }

  std::vector<mojom::UkmEntryPtr>& events = recordings_.entries;

  events.erase(
      std::remove_if(events.begin(), events.end(),
                     [&](const auto& event) {
                       return extension_source_ids.count(event->source_id);
                     }),
      events.end());

  recording_is_continuous_ = false;
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

// TODO(rkaplow): This should be refactored.
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

  // Construct set of whitelisted URLs by merging those carried over from the
  // previous report cycle and those from sources recorded in this cycle.
  std::unordered_set<std::string> url_whitelist;
  recordings_.carryover_urls_whitelist.swap(url_whitelist);
  AppendWhitelistedUrls(recordings_.sources, &url_whitelist);

  // Number of sources discarded due to not matching a navigation URL.
  int num_sources_unmatched = 0;
  std::unordered_map<SourceIdType, int> serialized_source_type_counts;

  for (const auto& kv : recordings_.sources) {
    // Don't keep sources of these types after current report because their
    // entries are logged only at source creation time.
    if (GetSourceIdType(kv.first) == base::UkmSourceId::Type::APP_ID ||
        GetSourceIdType(kv.first) == base::UkmSourceId::Type::HISTORY_ID) {
      MarkSourceForDeletion(kv.first);
    }
    // If the source id is not whitelisted, don't send it unless it has
    // associated entries and the URL matches that of a whitelisted source.
    // Note: If ShouldRestrictToWhitelistedSourceIds() is true, this logic will
    // not be hit as the source would have already been filtered in
    // UpdateSourceURL().
    if (!IsWhitelistedSourceId(kv.first)) {
      // UkmSource should not keep initial_url for non-navigation source IDs.
      DCHECK_EQ(1u, kv.second->urls().size());
      if (!url_whitelist.count(kv.second->url().spec())) {
        RecordDroppedSource(DroppedDataReason::NOT_MATCHED);
        MarkSourceForDeletion(kv.first);
        num_sources_unmatched++;
        continue;
      }
      // Omit entryless sources from the report.
      if (!base::Contains(source_ids_seen, kv.first)) {
        continue;
      } else {
        // Source of base::UkmSourceId::Type::UKM type will not be kept after
        // entries are logged.
        MarkSourceForDeletion(kv.first);
      }
    }
    Source* proto_source = report->add_sources();
    kv.second->PopulateProto(proto_source);

    serialized_source_type_counts[GetSourceIdType(kv.first)]++;
  }

  for (const auto& event_and_aggregate : recordings_.event_aggregations) {
    const EventAggregate& event_aggregate = event_and_aggregate.second;
    Aggregate* proto_aggregate = report->add_aggregates();
    proto_aggregate->set_source_id(0);  // Across all sources.
    proto_aggregate->set_event_hash(event_and_aggregate.first);
    proto_aggregate->set_total_count(event_aggregate.total_count);
    proto_aggregate->set_dropped_due_to_limits(
        event_aggregate.dropped_due_to_limits);
    proto_aggregate->set_dropped_due_to_sampling(
        event_aggregate.dropped_due_to_sampling);
    proto_aggregate->set_dropped_due_to_whitelist(
        event_aggregate.dropped_due_to_whitelist);
    for (const auto& metric_and_aggregate : event_aggregate.metrics) {
      const MetricAggregate& aggregate = metric_and_aggregate.second;
      Aggregate::Metric* proto_metric = proto_aggregate->add_metrics();
      proto_metric->set_metric_hash(metric_and_aggregate.first);
      proto_metric->set_value_sum(aggregate.value_sum);
      proto_metric->set_value_square_sum(aggregate.value_square_sum);
      if (aggregate.total_count != event_aggregate.total_count) {
        proto_metric->set_total_count(aggregate.total_count);
      }
      if (aggregate.dropped_due_to_limits !=
          event_aggregate.dropped_due_to_limits) {
        proto_metric->set_dropped_due_to_limits(
            aggregate.dropped_due_to_limits);
      }
      if (aggregate.dropped_due_to_sampling !=
          event_aggregate.dropped_due_to_sampling) {
        proto_metric->set_dropped_due_to_sampling(
            aggregate.dropped_due_to_sampling);
      }
      if (aggregate.dropped_due_to_whitelist !=
          event_aggregate.dropped_due_to_whitelist) {
        proto_metric->set_dropped_due_to_whitelist(
            aggregate.dropped_due_to_whitelist);
      }
    }
  }
  int num_serialized_sources = 0;
  for (const auto& entry : serialized_source_type_counts) {
    num_serialized_sources += entry.second;
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount2",
                            num_serialized_sources);
  UMA_HISTOGRAM_COUNTS_100000("UKM.Entries.SerializedCount2",
                              recordings_.entries.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnsentSourcesCount",
                            num_sources_unsent);
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnmatchedSourcesCount",
                            num_sources_unmatched);

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount2.Ukm",
                            serialized_source_type_counts[SourceIdType::UKM]);
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

  // Defer at most GetMaxKeptSources() sources to the next report,
  // prioritizing most recently created ones.
  int pruned_sources_age = PruneOldSources(GetMaxKeptSources());
  // Record how old the newest truncated source is.
  source_counts_proto->set_pruned_sources_age_seconds(pruned_sources_age);

  // Set deferred sources count after pruning.
  source_counts_proto->set_deferred_sources(recordings_.sources.size());
  // Same value as the deferred source count, for setting the carryover count in
  // the next reporting cycle.
  recordings_.source_counts.carryover_sources = recordings_.sources.size();

  // We already matched these deferred sources against the URL whitelist.
  // Re-whitelist them for the next report.
  for (const auto& kv : recordings_.sources) {
    recordings_.carryover_urls_whitelist.insert(kv.second->url().spec());
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
}

bool UkmRecorderImpl::ShouldRestrictToWhitelistedSourceIds() const {
  return base::GetFieldTrialParamByFeatureAsBool(
      kUkmFeature, "RestrictToWhitelistedSourceIds", false);
}

bool UkmRecorderImpl::ShouldRestrictToWhitelistedEntries() const {
  return true;
}

int UkmRecorderImpl::PruneOldSources(size_t max_kept_sources) {
  if (recordings_.sources.size() <= max_kept_sources)
    return 0;

  std::vector<std::pair<base::TimeTicks, SourceId>> timestamp_source_id_pairs;
  for (const auto& kv : recordings_.sources) {
    timestamp_source_id_pairs.push_back(
        std::make_pair(kv.second->creation_time(), kv.first));
  }
  // Partially sort so that the last |max_kept_sources| elements are the
  // newest.
  std::nth_element(timestamp_source_id_pairs.begin(),
                   timestamp_source_id_pairs.end() - max_kept_sources,
                   timestamp_source_id_pairs.end());

  for (auto kv = timestamp_source_id_pairs.begin();
       kv != timestamp_source_id_pairs.end() - max_kept_sources; ++kv) {
    recordings_.sources.erase(kv->second);
  }

  base::TimeDelta pruned_sources_age =
      base::TimeTicks::Now() -
      (timestamp_source_id_pairs.end() - (max_kept_sources + 1))->first;
  return pruned_sources_age.InSeconds();
}

void UkmRecorderImpl::UpdateSourceURL(SourceId source_id,
                                      const GURL& unsanitized_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::Contains(recordings_.sources, source_id))
    return;

  const GURL sanitized_url = SanitizeURL(unsanitized_url);
  if (!ShouldRecordUrl(source_id, sanitized_url))
    return;

  RecordSource(std::make_unique<UkmSource>(source_id, sanitized_url));
}

void UkmRecorderImpl::UpdateAppURL(SourceId source_id, const GURL& url) {
  if (!extensions_enabled_) {
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
    if (ShouldRecordUrl(source_id, sanitized_url))
      urls.push_back(std::move(sanitized_url));
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

bool UkmRecorderImpl::ShouldRecordUrl(SourceId source_id,
                                      const GURL& sanitized_url) const {
  if (!recording_enabled_) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    return false;
  }

  if (recordings_.sources.size() >= GetMaxSources()) {
    RecordDroppedSource(DroppedDataReason::MAX_HIT);
    return false;
  }

  if (ShouldRestrictToWhitelistedSourceIds() &&
      !IsWhitelistedSourceId(source_id)) {
    RecordDroppedSource(DroppedDataReason::NOT_WHITELISTED);
    return false;
  }

  if (sanitized_url.is_empty()) {
    RecordDroppedSource(DroppedDataReason::EMPTY_URL);
    return false;
  }

  if (!HasSupportedScheme(sanitized_url)) {
    RecordDroppedSource(DroppedDataReason::UNSUPPORTED_URL_SCHEME);
    DVLOG(2) << "Dropped Unsupported UKM URL:" << source_id << ":"
             << sanitized_url.spec();
    return false;
  }

  // Extension URLs need to be specifically enabled and the extension synced.
  if (sanitized_url.SchemeIs(kExtensionScheme)) {
    DCHECK_EQ(sanitized_url.GetWithEmptyPath(), sanitized_url);
    if (!extensions_enabled_) {
      RecordDroppedSource(DroppedDataReason::EXTENSION_URLS_DISABLED);
      return false;
    }
    if (!is_webstore_extension_callback_ ||
        !is_webstore_extension_callback_.Run(sanitized_url.host_piece())) {
      RecordDroppedSource(DroppedDataReason::EXTENSION_NOT_SYNCED);
      return false;
    }
  }
  return true;
}

void UkmRecorderImpl::RecordSource(std::unique_ptr<UkmSource> source) {
  SourceId source_id = source->id();
  if (GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID)
    recordings_.source_counts.navigation_sources++;
  recordings_.source_counts.observed++;
  recordings_.sources.emplace(source_id, std::move(source));
}

void UkmRecorderImpl::AddEntry(mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!HasUnknownMetrics(decode_map_, *entry));

  if (!recording_enabled_) {
    RecordDroppedEntry(DroppedDataReason::RECORDING_DISABLED);
    return;
  }

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

  if (ShouldRestrictToWhitelistedEntries() &&
      !base::Contains(whitelisted_entry_hashes_, entry->event_hash)) {
    RecordDroppedEntry(DroppedDataReason::NOT_WHITELISTED);
    event_aggregate.dropped_due_to_whitelist++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_whitelist++;
    return;
  }

  if (IsSamplingEnabled()) {
    if (default_sampling_rate_ == 0) {
      LoadExperimentSamplingInfo();
    }

    auto found = event_sampling_rates_.find(entry->event_hash);
    int sampling_rate = (found != event_sampling_rates_.end())
                            ? found->second
                            : default_sampling_rate_;
    bool sampled_in =
        IsSampledIn(entry->source_id, entry->event_hash, sampling_rate);

    if (!sampled_in) {
      RecordDroppedEntry(DroppedDataReason::SAMPLED_OUT);
      event_aggregate.dropped_due_to_sampling++;
      for (auto& metric : entry->metrics)
        event_aggregate.metrics[metric.first].dropped_due_to_sampling++;
      return;
    }
  }

  if (recordings_.entries.size() >= GetMaxEntries()) {
    RecordDroppedEntry(DroppedDataReason::MAX_HIT);
    event_aggregate.dropped_due_to_limits++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_limits++;
    return;
  }

  recordings_.entries.push_back(std::move(entry));
}

void UkmRecorderImpl::LoadExperimentSamplingInfo() {
  DCHECK_EQ(0, default_sampling_rate_);

  // Default rate must be > 0 to indicate that load is complete.
  default_sampling_rate_ = 1;

  // If we don't have the feature, no parameters to load.
  if (!base::FeatureList::IsEnabled(kUkmSamplingRateFeature)) {
    return;
  }

  // Check the parameters for sampling controls.
  std::map<std::string, std::string> params;
  if (base::GetFieldTrialParamsByFeature(kUkmSamplingRateFeature, &params)) {
    for (const auto& kv : params) {
      const std::string& key = kv.first;
      if (key.length() == 0)
        continue;

      // Keys starting with an underscore are global configuration.
      if (key.at(0) == '_') {
        if (key == "_default_sampling") {
          int sampling;
          // We only load positive global sampling rates. If the global sampling
          // is 0, then we stick to the default rate of '1' (unsampled).
          if (base::StringToInt(kv.second, &sampling) && sampling > 0)
            default_sampling_rate_ = sampling;
        }
        continue;
      }

      // Anything else is an event name.
      int sampling;
      if (base::StringToInt(kv.second, &sampling) && sampling >= 0)
        event_sampling_rates_[base::HashMetricName(key)] = sampling;
    }
  }
}

bool UkmRecorderImpl::IsSampledIn(int64_t source_id,
                                  uint64_t event_id,
                                  int sampling_rate) {
  // A sampling rate of 0 is "never"; everything else is 1-in-N but calculated
  // deterministically based on a seed, the source-id, and the event-id. Skip
  // the calculation, though, if N==1 because it will always be true.
  if (sampling_rate == 0)
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

void UkmRecorderImpl::StoreWhitelistedEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto entries =
      base::SplitString(GetWhitelistEntries(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& entry_string : entries)
    whitelisted_entry_hashes_.insert(base::HashMetricName(entry_string));
  decode_map_ = builders::CreateDecodeMap();
}

}  // namespace ukm
