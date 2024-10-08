// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_

#include <string_view>

#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/default.pb.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "components/optimization_guide/proto/features/history_query.pb.h"
#include "components/optimization_guide/proto/features/history_query_intent.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
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

class HistoryQueryFeatureTypeMap {
 public:
  using LoggingData = proto::HistoryQueryLoggingData;
  using Request = proto::HistoryQueryRequest;
  using Response = proto::HistoryQueryResponse;
  using Quality = proto::HistoryQueryQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_query();
  }

  static std::string_view ToString() { return "HistoryQuery"; }
};

class HistoryQueryIntentFeatureTypeMap {
 public:
  using LoggingData = proto::HistoryQueryIntentLoggingData;
  using Request = proto::HistoryQueryIntentRequest;
  using Response = proto::HistoryQueryIntentResponse;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_query_intent();
  }

  static std::string_view ToString() { return "HistoryQueryIntent"; }
};

class HistoryAnswerFeatureTypeMap {
 public:
  using LoggingData = proto::HistoryAnswerLoggingData;
  using Request = proto::HistoryAnswerRequest;
  using Response = proto::HistoryAnswerResponse;
  using Quality = proto::HistoryAnswerQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_answer();
  }

  static std::string_view ToString() { return "HistoryAnswer"; }
};

class ProductSpecificationsFeatureTypeMap {
 public:
  using LoggingData = proto::ProductSpecificationsLoggingData;
  using Quality = proto::ProductSpecificationsQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_product_specifications();
  }

  static std::string_view ToString() { return "ProductSpecifications"; }
};

class FormsAnnotationsFeatureTypeMap {
 public:
  using LoggingData = proto::FormsAnnotationsLoggingData;
  using Quality = proto::FormsAnnotationsQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_forms_annotations();
  }

  static std::string_view ToString() { return "FormsAnnotations"; }
};

class FormsPredictionsFeatureTypeMap {
 public:
  using LoggingData = proto::FormsPredictionsLoggingData;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_forms_predictions();
  }

  static std::string_view ToString() { return "FormsPredictions"; }
};

class ModelPrototypingFeatureTypeMap {
 public:
  using LoggingData = proto::ModelPrototypingLoggingData;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_model_prototyping();
  }

  static std::string_view ToString() { return "ModelPrototyping"; }
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
