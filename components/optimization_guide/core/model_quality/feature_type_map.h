// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_

#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/default.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/features/wallpaper_search.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

class DefaultFeatureTypeMap {
 public:
  using LoggingData = proto::DefaultLoggingData;
  using Request = proto::DefaultRequest;
  using Response = proto::DefaultResponse;
  using Quality = proto::DefaultQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_default_();
  }

  std::string_view ToString() { return "Unknown"; }
};

class ComposeFeatureTypeMap {
 public:
  using LoggingData = proto::ComposeLoggingData;
  using Request = proto::ComposeRequest;
  using Response = proto::ComposeResponse;
  using Quality = proto::ComposeQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_compose();
  }

  static std::string_view ToString() { return "Compose"; }
};

class TabOrganizationFeatureTypeMap {
 public:
  using LoggingData = proto::TabOrganizationLoggingData;
  using Request = proto::TabOrganizationRequest;
  using Response = proto::TabOrganizationResponse;
  using Quality = proto::TabOrganizationQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_tab_organization();
  }

  static std::string_view ToString() { return "TabOrganization"; }
};

class WallpaperSearchFeatureTypeMap {
 public:
  using LoggingData = proto::WallpaperSearchLoggingData;
  using Request = proto::WallpaperSearchRequest;
  using Response = proto::WallpaperSearchResponse;
  using Quality = proto::WallpaperSearchQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_wallpaper_search();
  }

  static std::string_view ToString() { return "WallpaperSearch"; }
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
