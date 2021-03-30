// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_

#include "base/logging.h"
#include "base/optional.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// Contains metadata that could be attached to an optimization provided by the
// Optimization Guide.
//
// Note: If a new optimization metadata is added,
// |OptimizationGuideHintsManager::AddHintsForTesting| should be updated
// to handle it.
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
  base::Optional<T> ParsedMetadata() const {
    if (!any_metadata_)
      return base::nullopt;
    return ParsedAnyMetadata<T>(*any_metadata_);
  }
  const base::Optional<proto::Any>& any_metadata() const {
    return any_metadata_;
  }
  void set_any_metadata(const proto::Any& any_metadata) {
    any_metadata_ = any_metadata;
  }
  // Sets |any_metadata_| to be validly parsed as |metadata|. Should only be
  // used for testing purposes.
  void SetAnyMetadataForTesting(const google::protobuf::MessageLite& metadata);

  const base::Optional<proto::PerformanceHintsMetadata>&
  performance_hints_metadata() const {
    return performance_hints_metadata_;
  }
  void set_performance_hints_metadata(
      const proto::PerformanceHintsMetadata& performance_hints_metadata) {
    performance_hints_metadata_ = performance_hints_metadata;
  }

  const base::Optional<proto::PublicImageMetadata>& public_image_metadata()
      const {
    return public_image_metadata_;
  }
  void set_public_image_metadata(
      const proto::PublicImageMetadata& public_image_metadata) {
    public_image_metadata_ = public_image_metadata;
  }

  const base::Optional<proto::LoadingPredictorMetadata>&
  loading_predictor_metadata() const {
    return loading_predictor_metadata_;
  }
  void set_loading_predictor_metadata(
      const proto::LoadingPredictorMetadata& loading_predictor_metadata) {
    loading_predictor_metadata_ = loading_predictor_metadata;
  }

 private:
  // Metadata applicable to the optimization type.
  //
  // Optimization types that are not specifically specified below will have
  // metadata populated with this field.
  base::Optional<proto::Any> any_metadata_;

  // Only applicable for the PERFORMANCE_HINTS optimization type.
  base::Optional<proto::PerformanceHintsMetadata> performance_hints_metadata_;

  // Only applicable for the COMPRESS_PUBLIC_IMAGES optimization type.
  base::Optional<proto::PublicImageMetadata> public_image_metadata_;

  // Only applicable for the LOADING_PREDICTOR optimization type.
  base::Optional<proto::LoadingPredictorMetadata> loading_predictor_metadata_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_METADATA_H_
