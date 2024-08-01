// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"

#include <inttypes.h>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform::processing {

namespace {

proto::UMAFeature* GetAsUMA(Data& data) {
  DCHECK(data.input_feature.has_value() || data.output_feature.has_value());

  if (data.input_feature.has_value()) {
    return data.input_feature->mutable_uma_feature();
  }

  return data.output_feature->mutable_uma_output()->mutable_uma_feature();
}

// Create an SQL query based on the aggregation type for the UMA feature.
UkmDatabase::CustomSqlQuery MakeSqlQuery(
    proto::SignalType signal_type,
    const std::string& profile_id,
    uint64_t name_hash,
    proto::Aggregation aggregation,
    uint64_t bucket_count,
    const base::Time& start_time,
    const base::Time& end_time,
    const base::TimeDelta& bucket_duration,
    const std::vector<std::string>& accepted_enum_ids,
    const float default_value) {
  UkmDatabase::CustomSqlQuery query;

  constexpr char kQueryTemplate[] =
      // clang-format off
      "SELECT IFNULL(%s,0)FROM uma_metrics " // 0: AggregationOfMetrics
      "WHERE metric_hash='%" PRIX64 "' " // 1: MetricHashInHex
      "AND profile_id=? "  // ?: ProfileID
      "AND type=? " // ?: MetricType
      "%s" // 2: EnumIDClause
      "AND event_timestamp BETWEEN ? AND ?"; // ?,?: TimeRange
  // clang-format on

  constexpr char kBucketedQueryTemplate[] =
      // clang-format off
      // Bucket values have all possible bucket indices like "(0),(1)...(N)".
      "WITH all_buckets(bucket)AS(VALUES%s)" // 0: BucketValuesAsRows
      "SELECT IFNULL(%s,0)FROM " // 1: AggregationOfMetrics
        "(SELECT "
         "SUM(metric_value) AS sum_vals, "
         "COUNT(metric_value) AS count_vals, "
         "(event_timestamp-?)/? AS bucket " // ?: StartTime, ?: BucketDuration
         "FROM uma_metrics "
         "WHERE metric_hash='%" PRIX64 "' " // 2: MetricHashInHex
         "AND profile_id=? " // ?: ProfileID
         "AND type=? " // MetricType
         "%s" // 3: EnumIDClause
         "AND event_timestamp BETWEEN ? AND ? " // ?,?: TimeRange
         "GROUP BY bucket)"
         "RIGHT JOIN all_buckets USING(bucket)"
         "ORDER BY bucket";
  // clang-format on

  constexpr char kLatestQueryTemplate[] =
      // clang-format off
      "SELECT COALESCE("
        "(SELECT metric_value FROM uma_metrics "
         "WHERE metric_hash='%" PRIX64 "' " // 0: MetricHashInHex
         "AND profile_id=? " // ?: ProfileID
         "AND type=? " // ?: MetricType
         "%s" // 1: EnumIDClause
         "AND event_timestamp BETWEEN ? AND ? " // ?,?: TimeRange
         "ORDER BY event_timestamp DESC,id DESC "
         "LIMIT 1),"
        "%f)"; // 2: DefaultValue
  // clang-format on

  std::string enum_matcher;
  if (!accepted_enum_ids.empty()) {
    enum_matcher =
        "AND metric_value IN(" + base::JoinString(accepted_enum_ids, ",") + ")";
  }
  std::string bucket_values;
  if (bucket_count > 0) {
    std::ostringstream oss;
    for (uint64_t i = 0; i < bucket_count; ++i) {
      oss << "(" << i << ")";
      if (i != bucket_count - 1) {
        oss << ",";
      }
    }
    bucket_values = std::move(oss).str();
  }
  bool is_bucketed = false;
  switch (aggregation) {
    case proto::Aggregation::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      break;
    case proto::Aggregation::COUNT:
      query.query = base::StringPrintf(kQueryTemplate, "COUNT(metric_value)",
                                       name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::COUNT_BOOLEAN:
      query.query = base::StringPrintf(kQueryTemplate, "COUNT(metric_value)>0",
                                       name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_COUNT:
      is_bucketed = true;
      query.query =
          base::StringPrintf(kBucketedQueryTemplate, bucket_values.c_str(),
                             "count_vals", name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN:
      is_bucketed = true;
      query.query =
          base::StringPrintf(kBucketedQueryTemplate, bucket_values.c_str(),
                             "count_vals>0", name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT:
      is_bucketed = true;
      query.query = base::StringPrintf(
          kBucketedQueryTemplate, bucket_values.c_str(), "COUNT(count_vals>0)",
          name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_CUMULATIVE_COUNT:
      // TODO(ssid): Deprecate this type. Unused and complex to write query.
      NOTIMPLEMENTED();
      return query;
    case proto::Aggregation::SUM:
      query.query = base::StringPrintf(kQueryTemplate, "SUM(metric_value)",
                                       name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::SUM_BOOLEAN:
      query.query = base::StringPrintf(kQueryTemplate, "SUM(metric_value)>0",
                                       name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_SUM:
      is_bucketed = true;
      query.query =
          base::StringPrintf(kBucketedQueryTemplate, bucket_values.c_str(),
                             "sum_vals", name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN:
      is_bucketed = true;
      query.query =
          base::StringPrintf(kBucketedQueryTemplate, bucket_values.c_str(),
                             "sum_vals>0", name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT:
      is_bucketed = true;
      query.query = base::StringPrintf(
          kBucketedQueryTemplate, bucket_values.c_str(), "COUNT(sum_vals>0)",
          name_hash, enum_matcher.c_str());
      break;
    case proto::Aggregation::BUCKETED_CUMULATIVE_SUM:
      // TODO(ssid): Deprecate this type. Unused and complex to write query.
      NOTIMPLEMENTED();
      return query;
    case proto::Aggregation::LATEST_OR_DEFAULT:
      query.query = base::StringPrintf(kLatestQueryTemplate, name_hash,
                                       enum_matcher.c_str(), default_value);
      break;
  }
  if (is_bucketed) {
    query.bind_values.emplace_back(start_time);
    query.bind_values.emplace_back(bucket_duration.InMicroseconds());
  }
  query.bind_values.emplace_back(profile_id);
  query.bind_values.emplace_back(static_cast<int>(signal_type));
  query.bind_values.emplace_back(start_time);
  query.bind_values.emplace_back(end_time);
  return query;
}

}  // namespace

UmaFeatureProcessor::UmaFeatureProcessor(
    base::flat_map<FeatureIndex, Data>&& uma_features,
    StorageService* storage_service,
    const std::string& profile_id,
    FeatureAggregator* feature_aggregator,
    const base::Time prediction_time,
    const base::Time observation_time,
    const base::TimeDelta bucket_duration,
    const SegmentId segment_id,
    bool is_output)
    : uma_features_(std::move(uma_features)),
      weak_storage_service_(storage_service->GetWeakPtr()),
      profile_id_(profile_id),
      feature_aggregator_(feature_aggregator),
      prediction_time_(prediction_time),
      observation_time_(observation_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id),
      is_output_(is_output),
      is_batch_processing_enabled_(base::FeatureList::IsEnabled(
          features::kSegmentationPlatformSignalDbCache)),
      use_sql_database_(base::FeatureList::IsEnabled(
          features::kSegmentationPlatformUmaFromSqlDb)) {}

UmaFeatureProcessor::~UmaFeatureProcessor() = default;

void UmaFeatureProcessor::Process(
    FeatureProcessorState& feature_processor_state,
    QueryProcessorCallback callback) {
  callback_ = std::move(callback);

  size_t max_bucket_count = 0;
  for (auto& feature : uma_features_) {
    // Validate the proto::UMAFeature metadata.
    const proto::UMAFeature* uma_feature = GetAsUMA(feature.second);
    if (metadata_utils::ValidateMetadataUmaFeature(*uma_feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      feature_processor_state.SetError(
          stats::FeatureProcessingError::kUmaValidationError);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    std::move(result_)));
      return;
    }

    if (max_bucket_count < uma_feature->bucket_count()) {
      max_bucket_count = uma_feature->bucket_count();
    }
  }

  if (use_sql_database_) {
    CHECK(GetUkmDatabase());
    ProcessUsingSqlDatabase(feature_processor_state);
  } else if (is_batch_processing_enabled_) {
    ProcessOnGotAllSamples(feature_processor_state,
                           *GetSignalDatabase()->GetAllSamples());
  } else {
    ProcessNextFeature();
  }
}

void UmaFeatureProcessor::ProcessNextFeature() {
  // Processing of the feature list has completed.
  if (uma_features_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    return;
  }

  // Process the feature list.
  const auto& it = uma_features_.begin();
  proto::UMAFeature feature = std::move(*GetAsUMA(it->second));
  FeatureIndex index = it->first;
  uma_features_.erase(it);

  proto::SignalType signal_type = feature.type();
  const auto name_hash = feature.name_hash();
  base::Time start_time;
  base::Time end_time;
  GetStartAndEndTime(feature.bucket_count(), start_time, end_time);

  GetSignalDatabase()->GetSamples(
      signal_type, name_hash, start_time, end_time,
      base::BindOnce(&UmaFeatureProcessor::OnGetSamplesForUmaFeature,
                     weak_ptr_factory_.GetWeakPtr(), index, feature, end_time));
}

void UmaFeatureProcessor::OnGetSamplesForUmaFeature(
    FeatureIndex index,
    const proto::UMAFeature& feature,
    const base::Time end_time,
    std::vector<SignalDatabase::DbEntry> samples) {
  ProcessSingleUmaFeature(samples, index, feature);
  ProcessNextFeature();
}

void UmaFeatureProcessor::GetStartAndEndTime(size_t bucket_count,
                                             base::Time& start_time,
                                             base::Time& end_time) const {
  base::TimeDelta duration = bucket_duration_ * bucket_count;
  if (is_output_) {
    if (observation_time_ == base::Time()) {
      start_time = prediction_time_ - duration;
      end_time = prediction_time_;
    } else if (observation_time_ - prediction_time_ > duration) {
      start_time = observation_time_ - duration;
      end_time = observation_time_;
    } else {
      start_time = prediction_time_;
      end_time = observation_time_;
    }
  } else {
    start_time = prediction_time_ - duration;
    end_time = prediction_time_;
  }
}

void UmaFeatureProcessor::ProcessOnGotAllSamples(
    FeatureProcessorState& feature_processor_state,
    const std::vector<SignalDatabase::DbEntry>& samples) {
  while (!uma_features_.empty()) {
    if (feature_processor_state.error()) {
      break;
    }

    const auto& it = uma_features_.begin();
    proto::UMAFeature next_feature = std::move(*GetAsUMA(it->second));
    FeatureIndex index = it->first;
    uma_features_.erase(it);

    ProcessSingleUmaFeature(samples, index, next_feature);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
}

void UmaFeatureProcessor::ProcessUsingSqlDatabase(
    FeatureProcessorState& feature_processor_state) {
  UkmDatabase::QueryList queries;

  while (!uma_features_.empty()) {
    if (feature_processor_state.error()) {
      break;
    }

    const auto& it = uma_features_.begin();
    proto::UMAFeature feature = std::move(*GetAsUMA(it->second));
    FeatureIndex index = it->first;
    uma_features_.erase(it);

    base::Time start_time;
    base::Time end_time;
    GetStartAndEndTime(feature.bucket_count(), start_time, end_time);

    // Enum histograms can optionally only accept some of the enum values.
    // While the proto::UMAFeature is available, capture a vector of the
    // accepted enum values. An empty vector is ignored (all values are
    // considered accepted).
    std::vector<std::string> accepted_enum_ids{};
    if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
      for (int i = 0; i < feature.enum_ids_size(); ++i) {
        accepted_enum_ids.emplace_back(
            base::StringPrintf("%d", feature.enum_ids(i)));
      }
    }

    queries.emplace(
        index,
        MakeSqlQuery(
            feature.type(), profile_id_, feature.name_hash(),
            feature.aggregation(), feature.bucket_count(), start_time, end_time,
            bucket_duration_, accepted_enum_ids,
            feature.default_values_size() > 0 ? feature.default_values(0) : 0));
  }
  GetUkmDatabase()->RunReadOnlyQueries(
      std::move(queries), base::BindOnce(&UmaFeatureProcessor::OnSqlQueriesRun,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void UmaFeatureProcessor::OnSqlQueriesRun(bool success,
                                          processing::IndexedTensors tensor) {
  if (success) {
    for (const auto& it : tensor) {
      result_[it.first] = std::move(it.second);
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
}

void UmaFeatureProcessor::ProcessSingleUmaFeature(
    const std::vector<SignalDatabase::DbEntry>& samples,
    FeatureIndex index,
    const proto::UMAFeature& feature) {
  // Enum histograms can optionally only accept some of the enum values.
  // While the proto::UMAFeature is available, capture a vector of the
  // accepted enum values. An empty vector is ignored (all values are
  // considered accepted).
  std::vector<int32_t> accepted_enum_ids{};
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    for (int i = 0; i < feature.enum_ids_size(); ++i) {
      accepted_enum_ids.emplace_back(feature.enum_ids(i));
    }
  }

  base::Time start_time;
  base::Time end_time;
  GetStartAndEndTime(feature.bucket_count(), start_time, end_time);
  base::ElapsedTimer timer;

  // We now have all the data required to process a single feature, so we can
  // process it synchronously, and insert it into the
  // FeatureProcessorState::input_tensor so we can later pass it to the ML model
  // executor.
  std::optional<std::vector<float>> result = feature_aggregator_->Process(
      feature.type(), feature.name_hash(), feature.aggregation(),
      feature.bucket_count(), start_time, end_time, bucket_duration_,
      accepted_enum_ids, samples);

  // If no feature data is available, use the default values specified instead.
  if (result.has_value()) {
    const std::vector<float>& feature_data = result.value();
    DCHECK_EQ(feature.tensor_length(), feature_data.size());
    result_[index] =
        std::vector<ProcessedValue>(feature_data.begin(), feature_data.end());
  } else {
    DCHECK_EQ(feature.tensor_length(),
              static_cast<unsigned int>(feature.default_values_size()))
        << " Mismatch between expected value size and default value size for "
           "UMA feature '"
        << feature.name()
        << "'. Did you forget to specify a default value for this feature?";
    result_[index] = std::vector<ProcessedValue>(
        feature.default_values().begin(), feature.default_values().end());
  }

  stats::RecordModelExecutionDurationFeatureProcessing(segment_id_,
                                                       timer.Elapsed());
}

SignalDatabase* UmaFeatureProcessor::GetSignalDatabase() {
  // Crash if weak_storage_service_ is not valid, processing should not run in
  // this case.
  return weak_storage_service_->signal_database();
}

UkmDatabase* UmaFeatureProcessor::GetUkmDatabase() {
  // Crash if weak_storage_service_ is not valid, processing should not run in
  // this case.
  UkmDataManager* ukm_data_manager = weak_storage_service_->ukm_data_manager();
  return ukm_data_manager->HasUkmDatabase() ? ukm_data_manager->GetUkmDatabase()
                                            : nullptr;
}

}  // namespace segmentation_platform::processing
