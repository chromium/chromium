// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_component_util.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_filter.h"
#include "components/optimization_guide/optimization_guide_features.h"

namespace optimization_guide {

namespace {

const char kProcessHintsComponentResultHistogramString[] =
    "OptimizationGuide.ProcessHintsResult";

// Populates |out_result| with |result| if |out_result| is provided.
void PopulateProcessHintsComponentResultIfSet(
    ProcessHintsComponentResult result,
    ProcessHintsComponentResult* out_result) {
  if (out_result)
    *out_result = result;
}

// Populates |out_status| with |status| if |out_status| is provided.
void PopulateOptimizationFilterStatusIfSet(
    OptimizationFilterStatus status,
    OptimizationFilterStatus* out_status) {
  if (out_status)
    *out_status = status;
}

// Attempts to construct a valid bloom filter from the given
// |optimization_filter|. If given, |out_status| will be populated with the
// status of the operation. If a valid bloom filter cannot be constructed,
// nullptr is returned.
std::unique_ptr<BloomFilter> ProcessBloomFilter(
    const proto::OptimizationFilter& optimization_filter,
    OptimizationFilterStatus* out_status) {
  const auto& bloom_filter_proto = optimization_filter.bloom_filter();
  DCHECK_GT(bloom_filter_proto.num_hash_functions(), 0u);
  DCHECK_GT(bloom_filter_proto.num_bits(), 0u);
  DCHECK(bloom_filter_proto.has_data());

  if (!bloom_filter_proto.has_data() || bloom_filter_proto.num_bits() <= 0 ||
      bloom_filter_proto.num_bits() > bloom_filter_proto.data().size() * 8) {
    DLOG(ERROR) << "Bloom filter config issue";
    PopulateOptimizationFilterStatusIfSet(
        OptimizationFilterStatus::kFailedServerBlacklistBadConfig, out_status);
    return nullptr;
  }

  if (static_cast<int>(bloom_filter_proto.num_bits()) >
      features::MaxServerBloomFilterByteSize() * 8) {
    DLOG(ERROR) << "Bloom filter data exceeds maximum size of "
                << optimization_guide::features::MaxServerBloomFilterByteSize()
                << " bytes";
    PopulateOptimizationFilterStatusIfSet(
        OptimizationFilterStatus::kFailedServerBlacklistTooBig, out_status);
    return nullptr;
  }

  std::unique_ptr<BloomFilter> bloom_filter = std::make_unique<BloomFilter>(
      bloom_filter_proto.num_hash_functions(), bloom_filter_proto.num_bits(),
      bloom_filter_proto.data());
  PopulateOptimizationFilterStatusIfSet(
      OptimizationFilterStatus::kCreatedServerBlacklist, out_status);
  return bloom_filter;
}

// Attempts to construct a valid RegexpList from the given
// |optimization_filter|. If given, |out_status| will be populated with the
// status of the operation. If a valid RegexpList cannot be constructed, nullptr
// is returned.
std::unique_ptr<RegexpList> ProcessRegexps(
    const proto::OptimizationFilter& optimization_filter,
    OptimizationFilterStatus* out_status) {
  std::unique_ptr<RegexpList> regexps = std::make_unique<RegexpList>();
  for (int i = 0; i < optimization_filter.regexps_size(); ++i) {
    regexps->emplace_back(
        std::make_unique<re2::RE2>(optimization_filter.regexps(i)));
    if (!regexps->at(i)->ok()) {
      PopulateOptimizationFilterStatusIfSet(
          OptimizationFilterStatus::kInvalidRegexp, out_status);
      return nullptr;
    }
  }

  PopulateOptimizationFilterStatusIfSet(
      OptimizationFilterStatus::kCreatedServerBlacklist, out_status);
  return regexps;
}

}  // namespace

const char kComponentHintsUpdatedResultHistogramString[] =
    "OptimizationGuide.UpdateComponentHints.Result";

void RecordProcessHintsComponentResult(ProcessHintsComponentResult result) {
  UMA_HISTOGRAM_ENUMERATION(kProcessHintsComponentResultHistogramString,
                            result);
}

std::unique_ptr<proto::Configuration> ProcessHintsComponent(
    const HintsComponentInfo& component_info,
    ProcessHintsComponentResult* out_result) {
  if (!component_info.version.IsValid() || component_info.path.empty()) {
    PopulateProcessHintsComponentResultIfSet(
        ProcessHintsComponentResult::kFailedInvalidParameters, out_result);
    return nullptr;
  }

  std::string binary_pb;
  if (!base::ReadFileToString(component_info.path, &binary_pb)) {
    PopulateProcessHintsComponentResultIfSet(
        ProcessHintsComponentResult::kFailedReadingFile, out_result);
    return nullptr;
  }

  std::unique_ptr<proto::Configuration> proto_configuration =
      std::make_unique<proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    PopulateProcessHintsComponentResultIfSet(
        ProcessHintsComponentResult::kFailedInvalidConfiguration, out_result);
    return nullptr;
  }

  PopulateProcessHintsComponentResultIfSet(
      ProcessHintsComponentResult::kSuccess, out_result);
  return proto_configuration;
}

void RecordOptimizationFilterStatus(proto::OptimizationType optimization_type,
                                    OptimizationFilterStatus status) {
  base::UmaHistogramExactLinear(
      base::StringPrintf(
          "OptimizationGuide.OptimizationFilterStatus.%s",
          GetStringNameForOptimizationType(optimization_type).c_str()),
      static_cast<int>(status),
      static_cast<int>(OptimizationFilterStatus::kMaxValue));
}

std::unique_ptr<OptimizationFilter> ProcessOptimizationFilter(
    const proto::OptimizationFilter& optimization_filter,
    OptimizationFilterStatus* out_status) {
  std::unique_ptr<BloomFilter> bloom_filter;
  if (optimization_filter.has_bloom_filter()) {
    bloom_filter = ProcessBloomFilter(optimization_filter, out_status);
    if (!bloom_filter)
      return nullptr;
  }

  std::unique_ptr<RegexpList> regexps;
  if (optimization_filter.regexps_size() > 0) {
    regexps = ProcessRegexps(optimization_filter, out_status);
    if (!regexps)
      return nullptr;
  }

  return std::make_unique<OptimizationFilter>(std::move(bloom_filter),
                                              std::move(regexps));
}

}  // namespace optimization_guide
