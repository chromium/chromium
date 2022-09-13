// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/reporting/compression/compression_module.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_interface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/snappy/src/snappy.h"

namespace reporting {

const base::Feature kCompressReportingPipeline{
    CompressionModule::kCompressReportingFeature,
    base::FEATURE_ENABLED_BY_DEFAULT};

// static
const char CompressionModule::kCompressReportingFeature[] =
    "CompressReportingPipeline";

namespace {

constexpr char kCompressionThresholdCountMetricsName[] =
    "Enterprise.CloudReportingCompressionThresholdCount";

enum class CompressedRecordThresholdMetricEvent {
  kNotCompressed = 0,
  kCompressed = 1,
  kMaxValue = kCompressed
};

constexpr char kSnappyUncompressedRecordSizeMetricsName[] =
    "Enterprise.CloudReportingSnappyUncompressedRecordSize";

constexpr char kSnappyCompressedRecordSizeMetricsName[] =
    "Enterprise.CloudReportingSnappyCompressedRecordSize";

}  // namespace

// static
scoped_refptr<CompressionModule> CompressionModule::Create(
    uint64_t compression_threshold,
    CompressionInformation::CompressionAlgorithm compression_type) {
  return base::WrapRefCounted(
      new CompressionModule(compression_threshold, compression_type));
}

void CompressionModule::CompressRecord(
    std::string record,
    scoped_refptr<ResourceInterface> memory_resource,
    base::OnceCallback<void(std::string,
                            absl::optional<CompressionInformation>)> cb) const {
  if (!is_enabled()) {
    // Compression disabled, don't compress and don't return compression
    // information.
    std::move(cb).Run(std::move(record), absl::nullopt);
    return;
  }
  // Compress if record is larger than the compression threshold and compression
  // enabled
  switch (compression_type_) {
    case CompressionInformation::COMPRESSION_NONE: {
      // Don't compress, simply return serialized record
      CompressionInformation compression_information;
      compression_information.set_compression_algorithm(
          CompressionInformation::COMPRESSION_NONE);
      std::move(cb).Run(std::move(record), std::move(compression_information));
      break;
    }
    case CompressionInformation::COMPRESSION_SNAPPY: {
      if (record.length() < compression_threshold_) {
        // Record size is smaller than threshold, don't compress.
        base::UmaHistogramEnumeration(
            kCompressionThresholdCountMetricsName,
            CompressedRecordThresholdMetricEvent::kNotCompressed);
        CompressionInformation compression_information;
        compression_information.set_compression_algorithm(
            CompressionInformation::COMPRESSION_NONE);
        std::move(cb).Run(std::move(record),
                          std::move(compression_information));
        return;
      }
      // Before doing compression, we must make sure there is enough memory - we
      // are going to temporarily double the record.
      ScopedReservation scoped_reservation(record.size(), memory_resource);
      if (!scoped_reservation.reserved()) {
        base::UmaHistogramEnumeration(
            kCompressionThresholdCountMetricsName,
            CompressedRecordThresholdMetricEvent::kNotCompressed);
        CompressionInformation compression_information;
        compression_information.set_compression_algorithm(
            CompressionInformation::COMPRESSION_NONE);
        std::move(cb).Run(std::move(record),
                          std::move(compression_information));
        return;
      }
      // Perform compression.
      base::UmaHistogramEnumeration(
          kCompressionThresholdCountMetricsName,
          CompressedRecordThresholdMetricEvent::kCompressed);
      CompressionModule::CompressRecordSnappy(std::move(record), std::move(cb));
      break;
    }
  }
}

// static
bool CompressionModule::is_enabled() {
  return base::FeatureList::IsEnabled(kCompressReportingPipeline);
}

CompressionModule::CompressionModule(
    uint64_t compression_threshold,
    CompressionInformation::CompressionAlgorithm compression_type)
    : compression_type_(compression_type),
      compression_threshold_(compression_threshold) {}
CompressionModule::~CompressionModule() = default;

void CompressionModule::CompressRecordSnappy(
    std::string record,
    base::OnceCallback<void(std::string,
                            absl::optional<CompressionInformation>)> cb) const {
  // Log record size before compression.
  const size_t uncompressed_record_size = record.size();
  base::UmaHistogramMemoryKB(kSnappyUncompressedRecordSizeMetricsName,
                             uncompressed_record_size / 1024u);

  // Compression is enabled and crosses the threshold,
  std::string output;
  snappy::Compress(record.data(), record.size(), &output);

  // Log record size after compression.
  const size_t compressed_record_size = record.size();
  base::UmaHistogramMemoryKB(kSnappyCompressedRecordSizeMetricsName,
                             compressed_record_size / 1024u);

  // Return compressed string
  CompressionInformation compression_information;
  compression_information.set_compression_algorithm(
      CompressionInformation::COMPRESSION_SNAPPY);
  std::move(cb).Run(output, compression_information);
}
}  // namespace reporting
