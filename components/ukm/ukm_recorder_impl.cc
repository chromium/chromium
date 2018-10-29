// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/variations/variations_associated_data.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

namespace ukm {

namespace {

// Note: kChromeUIScheme is defined in content, which this code can't
// depend on - since it's used by iOS too. kExtensionScheme is defined
// in extensions which also isn't always available here. kAppScheme
// will be defined in code that isn't available here.
const char kChromeUIScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";
const char kAppScheme[] = "app";

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
         GetSourceIdType(source_id) == SourceIdType::APP_ID;
}

// Gets the maximum number of Sources we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxSources() {
  constexpr size_t kDefaultMaxSources = 500;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxSources", kDefaultMaxSources));
}

// Gets the maximum number of unreferenced Sources kept after purging sources
// that were added to the log.
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

// True if we should record the initial_url field of the UKM Source proto.
bool ShouldRecordInitialUrl() {
  return base::GetFieldTrialParamByFeatureAsBool(kUkmFeature,
                                                 "RecordInitialUrl", false);
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

bool HasUnknownMetrics(const ukm::builders::DecodeMap& decode_map,
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

UkmRecorderImpl::UkmRecorderImpl() : recording_enabled_(false) {}
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
          base::FieldTrialList::kNoExpirationYear, 1, 1,
          base::FieldTrial::ONE_TIME_RANDOMIZED, nullptr));

  // Everybody (100%) should have a sampling configuration.
  std::map<std::string, std::string> params = {
      {"_default_sampling", base::IntToString(default_sampling)}};
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

void UkmRecorderImpl::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recordings_.Reset();
  recording_is_continuous_ = false;
}

void UkmRecorderImpl::SetIsWebstoreExtensionCallback(
    const IsWebstoreExtensionCallback& callback) {
  is_webstore_extension_callback_ = callback;
}

void UkmRecorderImpl::StoreRecordingsInReport(Report* report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<SourceId> ids_seen;
  for (const auto& entry : recordings_.entries) {
    Entry* proto_entry = report->add_entries();
    StoreEntryProto(*entry, proto_entry);
    ids_seen.insert(entry->source_id);
  }

  std::unordered_set<std::string> url_whitelist;
  recordings_.carryover_urls_whitelist.swap(url_whitelist);
  AppendWhitelistedUrls(recordings_.sources, &url_whitelist);

  std::vector<std::unique_ptr<UkmSource>> unsent_sources;
  int unmatched_sources = 0;
  std::unordered_map<ukm::SourceIdType, int> serialized_source_type_counts;
  for (auto& kv : recordings_.sources) {
    // If the source id is not whitelisted, don't send it unless it has
    // associated entries and the URL matches a URL of a whitelisted source.
    // Note: If ShouldRestrictToWhitelistedSourceIds() is true, this logic will
    // not be hit as the source would have already been filtered in
    // UpdateSourceURL().
    if (!IsWhitelistedSourceId(kv.first)) {
      // UkmSource should not keep initial_url for non-navigation source IDs.
      DCHECK_EQ(1u, kv.second->urls().size());
      if (!url_whitelist.count(kv.second->url().spec())) {
        RecordDroppedSource(DroppedDataReason::NOT_MATCHED);
        unmatched_sources++;
        continue;
      }
      if (!base::ContainsKey(ids_seen, kv.first)) {
        unsent_sources.push_back(std::move(kv.second));
        continue;
      }
    }
    Source* proto_source = report->add_sources();
    kv.second->PopulateProto(proto_source);
    if (!ShouldRecordInitialUrl())
      proto_source->clear_initial_url();

    serialized_source_type_counts[GetSourceIdType(kv.first)]++;
  }
  for (const auto& event_and_aggregate : recordings_.event_aggregations) {
    if (event_and_aggregate.second.metrics.empty())
      continue;
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
                            unsent_sources.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnmatchedSourcesCount",
                            unmatched_sources);

  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.Ukm",
      serialized_source_type_counts[ukm::SourceIdType::UKM]);
  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.Navigation",
      serialized_source_type_counts[ukm::SourceIdType::NAVIGATION_ID]);
  UMA_HISTOGRAM_COUNTS_1000(
      "UKM.Sources.SerializedCount2.App",
      serialized_source_type_counts[ukm::SourceIdType::APP_ID]);

  Report::SourceCounts* source_counts_proto = report->mutable_source_counts();
  source_counts_proto->set_observed(recordings_.source_counts.observed);
  source_counts_proto->set_navigation_sources(
      recordings_.source_counts.navigation_sources);
  source_counts_proto->set_unmatched_sources(unmatched_sources);
  source_counts_proto->set_deferred_sources(unsent_sources.size());
  source_counts_proto->set_carryover_sources(
      recordings_.source_counts.carryover_sources);

  recordings_.sources.clear();
  recordings_.source_counts.Reset();
  recordings_.entries.clear();
  recordings_.event_aggregations.clear();

  report->set_is_continuous(recording_is_continuous_);
  recording_is_continuous_ = true;

  // Keep at most |max_kept_sources|, prioritizing most-recent entries (by
  // creation time).
  const size_t max_kept_sources = GetMaxKeptSources();
  if (unsent_sources.size() > max_kept_sources) {
    std::nth_element(unsent_sources.begin(),
                     unsent_sources.begin() + max_kept_sources,
                     unsent_sources.end(),
                     [](const std::unique_ptr<ukm::UkmSource>& lhs,
                        const std::unique_ptr<ukm::UkmSource>& rhs) {
                       return lhs->creation_time() > rhs->creation_time();
                     });
    unsent_sources.resize(max_kept_sources);
  }

  for (auto& source : unsent_sources) {
    // We already matched these sources against the URL whitelist.
    // Re-whitelist them for the next report.
    recordings_.carryover_urls_whitelist.insert(source->url().spec());
    recordings_.sources.emplace(source->id(), std::move(source));
  }
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.KeptSourcesCount",
                            recordings_.sources.size());
  recordings_.source_counts.carryover_sources = recordings_.sources.size();
}

bool UkmRecorderImpl::ShouldRestrictToWhitelistedSourceIds() const {
  return base::GetFieldTrialParamByFeatureAsBool(
      kUkmFeature, "RestrictToWhitelistedSourceIds", false);
}

bool UkmRecorderImpl::ShouldRestrictToWhitelistedEntries() const {
  return true;
}

void UkmRecorderImpl::UpdateSourceURL(SourceId source_id,
                                      const GURL& unsanitized_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::ContainsKey(recordings_.sources, source_id))
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
  DCHECK(!base::ContainsKey(recordings_.sources, source_id));
  // TODO(csharrison): Consider changing this behavior so the Source isn't event
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
      !base::ContainsKey(whitelisted_entry_hashes_, entry->event_hash)) {
    RecordDroppedEntry(DroppedDataReason::NOT_WHITELISTED);
    event_aggregate.dropped_due_to_whitelist++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_whitelist++;
    return;
  }

  if (default_sampling_rate_ == 0)
    LoadExperimentSamplingInfo();

  auto found = event_sampling_rates_.find(entry->event_hash);
  int sampling_rate = (found != event_sampling_rates_.end())
                          ? found->second
                          : default_sampling_rate_;
  if (sampling_enabled_ &&
      (sampling_rate == 0 ||
       (sampling_rate > 1 && base::RandInt(1, sampling_rate) != 1))) {
    RecordDroppedEntry(DroppedDataReason::SAMPLED_OUT);
    event_aggregate.dropped_due_to_sampling++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric.first].dropped_due_to_sampling++;
    return;
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
  std::map<std::string, std::string> params;

  if (base::FeatureList::IsEnabled(kUkmSamplingRateFeature)) {
    // Enabled may have various parameters to control sampling.
    if (base::GetFieldTrialParamsByFeature(kUkmSamplingRateFeature, &params)) {
      for (const auto& kv : params) {
        const std::string& key = kv.first;
        if (key.length() == 0)
          continue;

        // Keys starting with an underscore are global configuration.
        if (key.at(0) == '_') {
          if (key == "_default_sampling") {
            int sampling;
            if (base::StringToInt(kv.second, &sampling) && sampling >= 0)
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

  // Default rate must be >0 to indicate that load is complete.
  if (default_sampling_rate_ == 0)
    default_sampling_rate_ = 1;
}

void UkmRecorderImpl::StoreWhitelistedEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto entries =
      base::SplitString(GetWhitelistEntries(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& entry_string : entries)
    whitelisted_entry_hashes_.insert(base::HashMetricName(entry_string));
  decode_map_ = ::ukm::builders::CreateDecodeMap();
}

}  // namespace ukm
