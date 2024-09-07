// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_

#include "base/memory/stack_allocated.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"

namespace segmentation_platform {

struct FeatureQuery {
  STACK_ALLOCATED();

 public:
  static constexpr FeatureQuery FromUMAFeature(
      MetadataWriter::UMAFeature uma_feature) {
    return FeatureQuery{.uma_feature = uma_feature};
  }
  static constexpr FeatureQuery FromSqlFeature(
      MetadataWriter::SqlFeature sql_feature) {
    return FeatureQuery{.sql_feature = sql_feature};
  }
  static constexpr FeatureQuery FromCustomInput(
      MetadataWriter::CustomInput custom_input) {
    return FeatureQuery{.custom_input = custom_input};
  }

  const std::optional<MetadataWriter::UMAFeature> uma_feature;
  const std::optional<MetadataWriter::SqlFeature> sql_feature;
  const std::optional<MetadataWriter::CustomInput> custom_input;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_
