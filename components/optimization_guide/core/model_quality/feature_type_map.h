// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_

#include <string_view>

#include "components/optimization_guide/proto/features/bling_prototyping.pb.h"
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
  using LoggingData = proto::features::ComposeLoggingData;
  using Request = proto::features::ComposeRequest;
  using Response = proto::features::ComposeResponse;
  using Quality = proto::features::ComposeQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_compose();
  }

  static std::string_view ToString() { return "Compose"; }
};

class TabOrganizationFeatureTypeMap {
 public:
  using LoggingData = proto::features::TabOrganizationLoggingData;
  using Request = proto::features::TabOrganizationRequest;
  using Response = proto::features::TabOrganizationResponse;
  using Quality = proto::features::TabOrganizationQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_tab_organization();
  }

  static std::string_view ToString() { return "TabOrganization"; }
};

class WallpaperSearchFeatureTypeMap {
 public:
  using LoggingData = proto::features::WallpaperSearchLoggingData;
  using Request = proto::features::WallpaperSearchRequest;
  using Response = proto::features::WallpaperSearchResponse;
  using Quality = proto::features::WallpaperSearchQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_wallpaper_search();
  }

  static std::string_view ToString() { return "WallpaperSearch"; }
};

class HistoryQueryFeatureTypeMap {
 public:
  using LoggingData = proto::features::HistoryQueryLoggingData;
  using Request = proto::features::HistoryQueryRequest;
  using Response = proto::features::HistoryQueryResponse;
  using Quality = proto::features::HistoryQueryQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_query();
  }

  static std::string_view ToString() { return "HistoryQuery"; }
};

class HistoryQueryIntentFeatureTypeMap {
 public:
  using LoggingData = proto::features::HistoryQueryIntentLoggingData;
  using Request = proto::features::HistoryQueryIntentRequest;
  using Response = proto::features::HistoryQueryIntentResponse;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_query_intent();
  }

  static std::string_view ToString() { return "HistoryQueryIntent"; }
};

class HistoryAnswerFeatureTypeMap {
 public:
  using LoggingData = proto::features::HistoryAnswerLoggingData;
  using Request = proto::features::HistoryAnswerRequest;
  using Response = proto::features::HistoryAnswerResponse;
  using Quality = proto::features::HistoryAnswerQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_history_answer();
  }

  static std::string_view ToString() { return "HistoryAnswer"; }
};

class ProductSpecificationsFeatureTypeMap {
 public:
  using LoggingData = proto::features::ProductSpecificationsLoggingData;
  using Quality = proto::features::ProductSpecificationsQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_product_specifications();
  }

  static std::string_view ToString() { return "ProductSpecifications"; }
};

class FormsAnnotationsFeatureTypeMap {
 public:
  using LoggingData = proto::features::FormsAnnotationsLoggingData;
  using Request = proto::features::FormsAnnotationsRequest;
  using Response = proto::features::FormsAnnotationsResponse;
  using Quality = proto::features::FormsAnnotationsQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_forms_annotations();
  }

  static std::string_view ToString() { return "FormsAnnotations"; }
};

class FormsPredictionsFeatureTypeMap {
 public:
  using LoggingData = proto::features::FormsPredictionsLoggingData;
  using Request = proto::features::FormsPredictionsRequest;
  using Response = proto::features::FormsPredictionsResponse;
  using Quality = proto::features::FormsPredictionsQuality;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_forms_predictions();
  }

  static std::string_view ToString() { return "FormsPredictions"; }
};

class BlingPrototypingFeatureTypeMap {
 public:
  using LoggingData = proto::features::BlingPrototypingLoggingData;
  using Request = proto::features::BlingPrototypingRequest;
  using Response = proto::features::BlingPrototypingResponse;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_bling_prototyping();
  }

  static std::string_view ToString() { return "BlingPrototyping"; }
};

class ModelPrototypingFeatureTypeMap {
 public:
  using LoggingData = proto::features::ModelPrototypingLoggingData;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_model_prototyping();
  }

  static std::string_view ToString() { return "ModelPrototyping"; }
};

class PasswordChangeSubmissionFeatureTypeMap {
 public:
  using LoggingData = proto::features::PasswordChangeSubmissionLoggingData;
  using Request = proto::features::PasswordChangeRequest;
  using Response = proto::features::PasswordChangeResponse;

  static LoggingData* GetLoggingData(proto::LogAiDataRequest& ai_data_request) {
    return ai_data_request.mutable_password_change_submission();
  }

  static std::string_view ToString() { return "PasswordChangeSubmission"; }
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_FEATURE_TYPE_MAP_H_
