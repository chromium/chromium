// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_COMPRESSION_COMPRESSION_MODULE_H_
#define COMPONENTS_REPORTING_COMPRESSION_COMPRESSION_MODULE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Feature to enable/disable compression.
// By default compression is disabled, until server can support compression.
BASE_DECLARE_FEATURE(kCompressReportingPipeline);

class CompressionModule : public base::RefCountedThreadSafe<CompressionModule> {
 public:
  // Not copyable or movable
  CompressionModule(const CompressionModule& other) = delete;
  CompressionModule& operator=(const CompressionModule& other) = delete;

  // Factory method creates |CompressionModule| object.
  static scoped_refptr<CompressionModule> Create(
      uint64_t compression_threshold_,
      CompressionInformation::CompressionAlgorithm compression_type_);

  // CompressRecord will attempt to compress the provided |record| and respond
  // with the callback. On success the returned std::string sink will
  // contain a compressed WrappedRecord string. The sink string then can be
  // further updated by the caller. std::string is used instead of
  // std::string_view because ownership is taken of |record| through
  // std::move(record).
  void CompressRecord(
      std::string record,
      scoped_refptr<ResourceManager> memory_resource,
      base::OnceCallback<void(std::string,
                              std::optional<CompressionInformation>)> cb) const;

  // Returns 'true' if |kCompressReportingPipeline| feature is enabled.
  static bool is_enabled();

  // Variable which defines which compression type to use
  const CompressionInformation::CompressionAlgorithm compression_type_;

 protected:
  // Constructor can only be called by |Create| factory method.
  CompressionModule(
      uint64_t compression_threshold_,
      CompressionInformation::CompressionAlgorithm compression_type_);

  // Refcounted object must have destructor declared protected or private.
  virtual ~CompressionModule();

 private:
  friend base::RefCountedThreadSafe<CompressionModule>;

  // Compresses a record using snappy
  void CompressRecordSnappy(
      std::string record,
      base::OnceCallback<void(std::string,
                              std::optional<CompressionInformation>)> cb) const;

  // Minimum compression threshold (in bytes) for when a record will be
  // compressed
  const uint64_t compression_threshold_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_COMPRESSION_COMPRESSION_MODULE_H_
