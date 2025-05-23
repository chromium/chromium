// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/permission_uma_util.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"

namespace safe_browsing {

namespace {

std::string GetFormattedNotificationContentsForModelInput(
    blink::PlatformNotificationData& notification_data) {
  // Enforce this crash on debug builds only.
  DCHECK_LE(notification_data.actions.size(),
            blink::mojom::NotificationData::kMaximumActions)
      << "There can only be at most "
      << blink::mojom::NotificationData::kMaximumActions << " actions but "
      << notification_data.actions.size() << " were provided.";
  std::vector<std::u16string> action_titles;
  for (const auto& action : notification_data.actions) {
    action_titles.push_back(action->title);
  }
  // Format notification content as comma-separated string value.
  return base::UTF16ToUTF8(
      base::JoinString({notification_data.title, notification_data.body,
                        base::JoinString(action_titles, u",")},
                       u","));
}

}  // namespace

// static
std::string NotificationContentDetectionModel::GetSerializedMetadata(
    bool is_on_global_cache_list,
    bool is_allowlisted_by_user,
    std::optional<double> suspicious_score) {
  auto metadata_dict =
      base::Value::Dict()
          .Set(kMetadataIsOriginOnGlobalCacheListKey, is_on_global_cache_list)
          .Set(kMetadataIsOriginAllowlistedByUserKey, is_allowlisted_by_user);
  if (suspicious_score.has_value()) {
    metadata_dict.Set(kMetadataSuspiciousKey, suspicious_score.value());
  }
  std::string serialized_metadata;
  JSONStringValueSerializer serializer(&serialized_metadata);
  serializer.Serialize(metadata_dict);
  return serialized_metadata;
}

NotificationContentDetectionModel::NotificationContentDetectionModel(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    content::BrowserContext* browser_context)
    : BertModelHandler(model_provider,
                       background_task_runner,
                       optimization_guide::proto::
                           OPTIMIZATION_TARGET_NOTIFICATION_CONTENT_DETECTION,
                       std::nullopt) {
  browser_context_ = browser_context;
}

NotificationContentDetectionModel::~NotificationContentDetectionModel() =
    default;

void NotificationContentDetectionModel::Execute(
    blink::PlatformNotificationData& notification_data,
    const GURL& origin,
    bool is_allowlisted_by_user,
    bool did_match_allowlist,
    ModelVerdictCallback model_verdict_callback) {
  // If there is no model version, then there is no valid notification content
  // detection model loaded from the server so don't check the model.
  if (!GetModelInfo() || !GetModelInfo()->GetVersion()) {
    std::move(model_verdict_callback)
        .Run(/*is_suspicious=*/false,
             GetSerializedMetadata(did_match_allowlist, is_allowlisted_by_user,
                                   std::nullopt));
    return;
  }

  // Invoke parent to execute the notification content detection tflite model
  // with `contents` as input.
  ExecuteModelWithInput(
      base::BindOnce(&NotificationContentDetectionModel::PostprocessCategories,
                     weak_ptr_factory_.GetWeakPtr(), origin,
                     is_allowlisted_by_user, did_match_allowlist,
                     std::move(model_verdict_callback)),
      GetFormattedNotificationContentsForModelInput(notification_data));
}

void NotificationContentDetectionModel::PostprocessCategories(
    const GURL& origin,
    bool is_allowlisted_by_user,
    bool did_match_allowlist,
    ModelVerdictCallback model_verdict_callback,
    const std::optional<std::vector<tflite::task::core::Category>>& output) {
  // If the model does not have an output, return without collecting metrics.
  // This can happen if the model times out and this should not cause a crash.
  if (!output.has_value()) {
    std::move(model_verdict_callback)
        .Run(/*is_suspicious=*/false,
             GetSerializedMetadata(did_match_allowlist, is_allowlisted_by_user,
                                   std::nullopt));
    return;
  }
  // Validate model response and obtain suspicious and not suspicious confidence
  // scores. Crash on debug builds only.
  DCHECK_EQ(output->size(), 2UL);
  for (const auto& category : *output) {
    if (category.class_name == kSuspiciousVerdictLabel) {
      // Log "suspicious" score from model's response.
      base::UmaHistogramPercentage(kSuspiciousScoreHistogram,
                                   100 * category.score);
      // Since blink::mojom::SiteEngagementLevel::NONE corresponds to 0, use as
      // default engagement level.
      uint64_t site_engagement_level = 0;
      if (site_engagement::SiteEngagementService::Get(browser_context_)) {
        site_engagement_level = static_cast<uint64_t>(
            site_engagement::SiteEngagementService::Get(browser_context_)
                ->GetEngagementLevel(origin));
      }
      permissions::PermissionUmaUtil::RecordPermissionUsageNotificationShown(
          is_allowlisted_by_user, did_match_allowlist, 100 * category.score,
          browser_context_, origin, site_engagement_level);
      bool is_suspicious =
          (100 * category.score >
           kShowWarningsForSuspiciousNotificationsScoreThreshold.Get()) &&
          !is_allowlisted_by_user && !did_match_allowlist;
      std::move(model_verdict_callback)
          .Run(is_suspicious, GetSerializedMetadata(did_match_allowlist,
                                                    is_allowlisted_by_user,
                                                    100 * category.score));
      return;
    }
  }
  std::move(model_verdict_callback)
      .Run(/*is_suspicious=*/false,
           GetSerializedMetadata(did_match_allowlist, is_allowlisted_by_user,
                                 std::nullopt));
  // Enforce this crash on debug builds only.
  DCHECK(false) << "Could not find the right class name in the model response";
}

}  // namespace safe_browsing
