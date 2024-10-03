// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/feature_registration.h"

#include "base/feature_list.h"
#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/feature_registry/settings_ui_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "enterprise_policy_registry.h"
#include "mqls_feature_registry.h"

namespace optimization_guide {

namespace prefs {

const char kTabOrganizationEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.tab_organization_enterprise_policy_"
    "allowed";

const char kComposeEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.compose_enterprise_policy_allowed";

const char kWallpaperSearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.wallpaper_search_enterprise_policy_"
    "allowed";

const char kHistorySearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.history_search_"
    "enterprise_policy_allowed";

const char kProductSpecificationsEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.tab_compare_settings_enterprise_policy";

const char kFormsPredictionsEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.forms_predictions_enterprise_policy_"
    "allowed";

}  // namespace prefs

namespace features {
BASE_FEATURE(kComposeMqlsLogging,
             "ComposeMqlsLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationMqlsLogging,
             "TabOrganizationMqlsLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWallpaperSearchMqlsLogging,
             "WallpaperSearchMqlsLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHistorySearchMqlsLogging,
             "HistorySearchMqlsLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProductSpecificationsMqlsLogging,
             "ProductSpecificationsMqlsLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFormsPredictionsMqlsLogging,
             "FormsPredictionsMqlsLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFormsAnnotationsMqlsLogging,
             "FormsAnnotationsMqlsLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace {

void RegisterCompose() {
  const char* kComposeName = "Compose";
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kComposeEnterprisePolicyAllowed);

  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.compose().quality().user_feedback();
      });
  auto mqls_metadata = std::make_unique<MqlsFeatureMetadata>(
      kComposeName, proto::LogAiDataRequest::FeatureCase::kCompose,
      enterprise_policy, &features::kComposeMqlsLogging, logging_callback,
      UserVisibleFeatureKey::kCompose);
  MqlsFeatureRegistry::GetInstance().Register(std::move(mqls_metadata));

  auto ui_metadata = std::make_unique<SettingsUiMetadata>(
      kComposeName, UserVisibleFeatureKey::kCompose, enterprise_policy);
  SettingsUiRegistry::GetInstance().Register(std::move(ui_metadata));
}

void RegisterTabOrganization() {
  const char* kTabOrganizationName = "TabOrganization";
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kTabOrganizationEnterprisePolicyAllowed);

  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        // If there is no tab organization, we don't have any user_feedback mark
        // it as unspecified.
        const proto::TabOrganizationQuality& quality =
            request_proto.tab_organization().quality();
        if (quality.organizations().empty()) {
          return proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
        }
        if (quality.user_feedback()) {
          return quality.user_feedback();
        }
        // TODO(b/331852814): Remove this else case along with the multi tab
        // organization flag.
        return quality.organizations()[0].user_feedback();
      });
  auto mqls_metadata = std::make_unique<MqlsFeatureMetadata>(
      kTabOrganizationName,
      proto::LogAiDataRequest::FeatureCase::kTabOrganization, enterprise_policy,
      &features::kTabOrganizationMqlsLogging, logging_callback,
      UserVisibleFeatureKey::kTabOrganization);
  MqlsFeatureRegistry::GetInstance().Register(std::move(mqls_metadata));

  auto ui_metadata = std::make_unique<SettingsUiMetadata>(
      kTabOrganizationName, UserVisibleFeatureKey::kTabOrganization,
      enterprise_policy);
  SettingsUiRegistry::GetInstance().Register(std::move(ui_metadata));
}

void RegisterWallpaperSearch() {
  const char* kWallpaperSearchName = "WallpaperSearch";
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kWallpaperSearchEnterprisePolicyAllowed);

  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.wallpaper_search().quality().user_feedback();
      });
  auto mqls_metadata = std::make_unique<MqlsFeatureMetadata>(
      kWallpaperSearchName,
      proto::LogAiDataRequest::FeatureCase::kWallpaperSearch, enterprise_policy,
      &features::kWallpaperSearchMqlsLogging, logging_callback,
      UserVisibleFeatureKey::kWallpaperSearch);
  MqlsFeatureRegistry::GetInstance().Register(std::move(mqls_metadata));

  auto ui_metadata = std::make_unique<SettingsUiMetadata>(
      kWallpaperSearchName, UserVisibleFeatureKey::kWallpaperSearch,
      enterprise_policy);
  SettingsUiRegistry::GetInstance().Register(std::move(ui_metadata));
}

void RegisterHistorySearch() {
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kHistorySearchEnterprisePolicyAllowed);

  UserFeedbackCallback logging_callback_query =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.history_query().quality().user_feedback();
      });
  auto mqls_metadata_query = std::make_unique<MqlsFeatureMetadata>(
      "HistoryQuery", proto::LogAiDataRequest::FeatureCase::kHistoryQuery,
      enterprise_policy, &features::kHistorySearchMqlsLogging,
      logging_callback_query, UserVisibleFeatureKey::kHistorySearch);
  MqlsFeatureRegistry::GetInstance().Register(std::move(mqls_metadata_query));

  UserFeedbackCallback logging_callback_answer =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        // There is no user feedback on history answer. It's recorded on history
        // query.
        return proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      });
  auto mqls_metadata_answer = std::make_unique<MqlsFeatureMetadata>(
      "HistoryAnswer", proto::LogAiDataRequest::FeatureCase::kHistoryAnswer,
      enterprise_policy, &features::kHistorySearchMqlsLogging,
      logging_callback_answer, UserVisibleFeatureKey::kHistorySearch);
  MqlsFeatureRegistry::GetInstance().Register(std::move(mqls_metadata_answer));

  auto ui_metadata = std::make_unique<SettingsUiMetadata>(
      "HistorySearch", UserVisibleFeatureKey::kHistorySearch,
      enterprise_policy);
  SettingsUiRegistry::GetInstance().Register(std::move(ui_metadata));
}

void RegisterProductSpecifications() {
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kProductSpecificationsEnterprisePolicyAllowed);
  UserFeedbackCallback logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.product_specifications().quality().user_feedback();
      });
  auto metadata = std::make_unique<MqlsFeatureMetadata>(
      "ProductSpecifications",
      proto::LogAiDataRequest::FeatureCase::kProductSpecifications,
      enterprise_policy, &features::kProductSpecificationsMqlsLogging,
      logging_callback, std::nullopt);
  MqlsFeatureRegistry::GetInstance().Register(std::move(metadata));
}

void RegisterFormsPredictions() {
  EnterprisePolicyPref enterprise_policy =
      EnterprisePolicyRegistry::GetInstance().Register(
          prefs::kFormsPredictionsEnterprisePolicyAllowed);
  UserFeedbackCallback fp_logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.forms_predictions().quality().user_feedback();
      });
  auto fp_mqls_metadata = std::make_unique<MqlsFeatureMetadata>(
      "FormsPredictions",
      proto::LogAiDataRequest::FeatureCase::kFormsPredictions,
      enterprise_policy, &features::kFormsPredictionsMqlsLogging,
      fp_logging_callback, std::nullopt);
  MqlsFeatureRegistry::GetInstance().Register(std::move(fp_mqls_metadata));

  // Forms annotations. In the same block as forms predictions since it
  // leverages the same enterprise policy.
  UserFeedbackCallback fa_logging_callback =
      base::BindRepeating([](proto::LogAiDataRequest& request_proto) {
        return request_proto.forms_annotations().quality().user_feedback();
      });
  auto fa_mqls_metadata = std::make_unique<MqlsFeatureMetadata>(
      "FormsAnnotations",
      proto::LogAiDataRequest::FeatureCase::kFormsAnnotations,
      enterprise_policy, &features::kFormsAnnotationsMqlsLogging,
      fa_logging_callback, std::nullopt);
  MqlsFeatureRegistry::GetInstance().Register(std::move(fa_mqls_metadata));
}

}  // anonymous namespace

void RegisterGenAiFeatures(PrefRegistrySimple* pref_registry) {
  static bool features_registered = false;
  if (!features_registered) {
    // The registries are static and so should only be populated once for the
    // program (rather than once per profile).
    RegisterCompose();
    RegisterTabOrganization();
    RegisterWallpaperSearch();
    RegisterHistorySearch();
    RegisterProductSpecifications();
    RegisterFormsPredictions();
    features_registered = true;
  }
  EnterprisePolicyRegistry::GetInstance().RegisterProfilePrefs(pref_registry);
}

}  // namespace optimization_guide
