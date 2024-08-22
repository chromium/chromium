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
  FeatureQuery();
  ~FeatureQuery();

  FeatureQuery(const FeatureQuery&) = delete;
  FeatureQuery& operator=(const FeatureQuery&) = delete;

  explicit FeatureQuery(MetadataWriter::UMAFeature uma_feature)
      : uma_feature(uma_feature) {}
  explicit FeatureQuery(MetadataWriter::SqlFeature sql_feature)
      : sql_feature(sql_feature) {}
  explicit FeatureQuery(MetadataWriter::CustomInput custom_input)
      : custom_input(custom_input) {}

  std::optional<MetadataWriter::UMAFeature> uma_feature;
  std::optional<MetadataWriter::SqlFeature> sql_feature;
  std::optional<MetadataWriter::CustomInput> custom_input;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_
