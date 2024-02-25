// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_

#include <optional>

#include "base/logging.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Contains metadata that could be attached to an optimization provided by the
// Optimization Guide.
class OptimizationMetadata {
 public:
  OptimizationMetadata();
  OptimizationMetadata(const OptimizationMetadata&);
  ~OptimizationMetadata();

  // Validates that the metadata stored in |any_metadata_| is of the same type
  // and is parseable as |T|. Will return metadata if all checks pass.
  template <
      class T,
      class = typename std::enable_if<
          std::is_convertible<T*, google::protobuf::MessageLite*>{}>::type>
  std::optional<T> ParsedMetadata() const {
    if (!any_metadata_)
      return std::nullopt;
    return ParsedAnyMetadata<T>(*any_metadata_);
  }
  const std::optional<proto::Any>& any_metadata() const {
    return any_metadata_;
  }
  void set_any_metadata(const proto::Any& any_metadata) {
    any_metadata_ = any_metadata;
  }
  // Sets |any_metadata_| to be validly parsed as |metadata|. Should only be
  // used for testing purposes.
  void SetAnyMetadataForTesting(const google::protobuf::MessageLite& metadata);

  const std::optional<proto::LoadingPredictorMetadata>&
  loading_predictor_metadata() const {
    return loading_predictor_metadata_;
  }
  void set_loading_predictor_metadata(
      const proto::LoadingPredictorMetadata& loading_predictor_metadata) {
    loading_predictor_metadata_ = loading_predictor_metadata;
  }

  // Returns true if |this| contains no metadata.
  bool empty() const {
    return !any_metadata_ && !loading_predictor_metadata_;
  }

 private:
  // Metadata applicable to the optimization type.
  //
  // Optimization types that are not specifically specified below will have
  // metadata populated with this field.
  std::optional<proto::Any> any_metadata_;

  // Only applicable for the LOADING_PREDICTOR optimization type.
  std::optional<proto::LoadingPredictorMetadata> loading_predictor_metadata_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_
