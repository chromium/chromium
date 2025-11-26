// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include <cstdint>
#include <utility>
#include <variant>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/prefs/pref_service.h"
#include "components/safety_check/safety_check.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#endif

namespace {
bool scoped_revocation_reporter_in_scope = false;
}  // namespace

namespace permissions {

#define PERMISSION_BUBBLE_TYPE_UMA(metric_name, request_type_for_uma) \
  base::UmaHistogramEnumeration(metric_name, request_type_for_uma,    \
                                RequestTypeForUma::NUM)

#define PERMISSION_BUBBLE_GESTURE_TYPE_UMA(                                  \
    gesture_metric_name, no_gesture_metric_name, gesture_type,               \
    permission_bubble_type)                                                  \
  if (gesture_type == PermissionRequestGestureType::GESTURE) {               \
    PERMISSION_BUBBLE_TYPE_UMA(gesture_metric_name, permission_bubble_type); \
  } else if (gesture_type == PermissionRequestGestureType::NO_GESTURE) {     \
    PERMISSION_BUBBLE_TYPE_UMA(no_gesture_metric_name,                       \
                               permission_bubble_type);                      \
  }

using blink::PermissionType;

namespace {

const int kPriorCountCap = 10;

std::string GetPermissionRequestString(RequestTypeForUma type) {
  switch (type) {
    case RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE:
      return "AudioAndVideoCapture";
    case RequestTypeForUma::QUOTA:
      return "Quota";
    case RequestTypeForUma::DOWNLOAD:
      return "MultipleDownload";
    case RequestTypeForUma::REGISTER_PROTOCOL_HANDLER:
      return "RegisterProtocolHandler";
    case RequestTypeForUma::PERMISSION_GEOLOCATION:
      return "Geolocation";
    case RequestTypeForUma::PERMISSION_MIDI_SYSEX:
      return "MidiSysEx";
    case RequestTypeForUma::PERMISSION_NOTIFICATIONS:
      return "Notifications";
    case RequestTypeForUma::PERMISSION_PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMedia";
    case RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC:
      return "AudioCapture";
    case RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case RequestTypeForUma::PERMISSION_PAYMENT_HANDLER:
      return "PaymentHandler";
    case RequestTypeForUma::PERMISSION_NFC:
      return "Nfc";
    case RequestTypeForUma::PERMISSION_CLIPBOARD_READ_WRITE:
      return "ClipboardReadWrite";
    case RequestTypeForUma::PERMISSION_VR:
      return "VR";
    case RequestTypeForUma::PERMISSION_AR:
      return "AR";
    case RequestTypeForUma::PERMISSION_HAND_TRACKING:
      return "HandTracking";
    case RequestTypeForUma::PERMISSION_STORAGE_ACCESS:
      return "StorageAccess";
    case RequestTypeForUma::PERMISSION_TOP_LEVEL_STORAGE_ACCESS:
      return "TopLevelStorageAccess";
    case RequestTypeForUma::PERMISSION_CAMERA_PAN_TILT_ZOOM:
      return "CameraPanTiltZoom";
    case RequestTypeForUma::PERMISSION_WINDOW_MANAGEMENT:
      return "WindowManagement";
    case RequestTypeForUma::PERMISSION_LOCAL_FONTS:
      return "LocalFonts";
    case RequestTypeForUma::PERMISSION_IDLE_DETECTION:
      return "IdleDetection";
    case RequestTypeForUma::PERMISSION_FILE_SYSTEM_ACCESS:
      return "FileSystemAccess";
    case RequestTypeForUma::CAPTURED_SURFACE_CONTROL:
      return "CapturedSurfaceControl";
    case RequestTypeForUma::PERMISSION_SMART_CARD:
      return "SmartCard";
    case RequestTypeForUma::PERMISSION_WEB_PRINTING:
      return "WebPrinting";
    case RequestTypeForUma::PERMISSION_IDENTITY_PROVIDER:
      return "IdentityProvider";
    case RequestTypeForUma::PERMISSION_KEYBOARD_LOCK:
      return "KeyboardLock";
    case RequestTypeForUma::PERMISSION_POINTER_LOCK:
      return "PointerLock";
    case RequestTypeForUma::MULTIPLE_KEYBOARD_AND_POINTER_LOCK:
      return "KeyboardAndPointerLock";
    case RequestTypeForUma::PERMISSION_WEB_APP_INSTALLATION:
      return "WebAppInstallation";
    case RequestTypeForUma::PERMISSION_LOCAL_NETWORK_ACCESS:
      return "LocalNetworkAccess";

    case RequestTypeForUma::UNKNOWN:
    case RequestTypeForUma::PERMISSION_FLASH:
    case RequestTypeForUma::PERMISSION_FILE_HANDLING:
    case RequestTypeForUma::NUM:
      NOTREACHED();
  }
}

// Helper to check if the current render frame host is cross-origin with top
// level frame. Note: in case of nested frames like A(B(A)), the bottom frame A
// will get |IsCrossOriginSubframe| returns false.
bool IsCrossOriginSubframe(content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  // Permissions are denied for fenced frames and other inner pages.
  // |GetMainFrame| should be enough to get top level frame.
  auto current_origin = render_frame_host->GetLastCommittedOrigin();
  return !render_frame_host->GetMainFrame()
              ->GetLastCommittedOrigin()
              .IsSameOriginWith(current_origin);
}

// Helper to check if the current render frame host is cross-origin with any of
// its parents.
bool IsCrossOriginWithAnyParent(content::RenderFrameHost* render_frame_host) {
  const url::Origin& current_origin =
      render_frame_host->GetLastCommittedOrigin();
  content::RenderFrameHost* parent = render_frame_host->GetParent();
  while (parent) {
    const url::Origin& parent_origin = parent->GetLastCommittedOrigin();
    if (!parent_origin.IsSameOriginWith(current_origin)) {
      return true;
    }
    parent = parent->GetParent();
  }
  return false;
}

// Helper to get permission policy header policy for the top-level frame.
// render_frame_host could be the top-level frame or a descendant of top-level
// frame.
PermissionHeaderPolicyForUMA GetTopLevelPermissionHeaderPolicyForUMA(
    content::RenderFrameHost* render_frame_host,
    network::mojom::PermissionsPolicyFeature feature) {
  const auto& parsed_permission_policy_header =
      render_frame_host->GetMainFrame()->GetPermissionsPolicyHeader();
  if (parsed_permission_policy_header.empty()) {
    return PermissionHeaderPolicyForUMA::HEADER_NOT_PRESENT_OR_INVALID;
  }

  const auto* permissions_policy =
      render_frame_host->GetMainFrame()->GetPermissionsPolicy();
  const auto& allowlists = permissions_policy->allowlists();
  auto allowlist = allowlists.find(feature);
  if (allowlist == allowlists.end()) {
    return PermissionHeaderPolicyForUMA::FEATURE_NOT_PRESENT;
  }

  if (allowlist->second.MatchesAll()) {
    return PermissionHeaderPolicyForUMA::FEATURE_ALLOWLIST_IS_WILDCARD;
  }

  const auto& origin = render_frame_host->GetLastCommittedOrigin();
  return allowlist->second.Contains(origin)
             ? PermissionHeaderPolicyForUMA::
                   FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN
             : PermissionHeaderPolicyForUMA::
                   FEATURE_ALLOWLIST_DOES_NOT_MATCH_ORIGIN;
}

void RecordEngagementMetric(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    content::WebContents* web_contents,
    const std::string& action) {
  CHECK(!requests.empty());

  RequestTypeForUma type = PermissionUtil::GetUmaValueForRequests(requests);

  DCHECK(action == "Accepted" || action == "Denied" || action == "Dismissed" ||
         action == "Ignored" || action == "AcceptedOnce");
  std::string name = base::StrCat({"Permissions.Engagement.", action, ".",
                                   GetPermissionRequestString(type)});

  double engagement_score = PermissionsClient::Get()->GetSiteEngagementScore(
      web_contents->GetBrowserContext(), requests[0]->requesting_origin());
  base::UmaHistogramPercentageObsoleteDoNotUse(name, engagement_score);
}

// Records in a UMA histogram whether we should expect to see an event in UKM,
// to allow for evaluating if the current constraints on UKM recording work well
// in practice.
void RecordUmaForWhetherRevocationUkmWasRecorded(
    ContentSettingsType permission_type,
    bool has_source_id) {
  if (permission_type == ContentSettingsType::NOTIFICATIONS) {
    base::UmaHistogramBoolean(
        "Permissions.Revocation.Notifications.DidRecordUkm", has_source_id);
  }
}

// Records in a UMA histogram whether we should expect to see an event in UKM,
// to allow for evaluating if the current constraints on UKM recording work well
// in practice.
void RecordUmaForWhetherUsageUkmWasRecorded(ContentSettingsType permission_type,
                                            bool has_source_id) {
  if (permission_type == ContentSettingsType::NOTIFICATIONS) {
    base::UmaHistogramBoolean("Permissions.Usage.Notifications.DidRecordUkm",
                              has_source_id);
  }
}

void RecordUmaForRevocationSourceUI(ContentSettingsType permission_type,
                                    PermissionSourceUI source_ui) {
  if (permission_type == ContentSettingsType::NOTIFICATIONS) {
    base::UmaHistogramEnumeration(
        "Permissions.Revocation.Notifications.SourceUI", source_ui);
  }
}

void RecordPermissionUsageUkm(ContentSettingsType permission_type,
                              std::optional<ukm::SourceId> source_id) {
  RecordUmaForWhetherUsageUkmWasRecorded(permission_type,
                                         source_id.has_value());
  if (!source_id.has_value()) {
    return;
  }

  ukm::builders::PermissionUsage builder(source_id.value());
  builder.SetPermissionType(static_cast<int64_t>(
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          permission_type)));
  builder.Record(ukm::UkmRecorder::Get());
}

void RecordPermissionUsageNotificationShownUkm(
    bool did_user_always_allow_notifications,
    bool is_allowlisted,
    int suspicious_score,
    uint64_t site_engagement_level,
    std::optional<ukm::SourceId> source_id) {
  if (!source_id.has_value()) {
    return;
  }

  ukm::builders::PermissionUsage_NotificationShown builder(source_id.value());
  builder.SetDidUserAlwaysAllowNotifications(
      did_user_always_allow_notifications);
  builder.SetIsAllowlisted(is_allowlisted);
  builder.SetSuspiciousScore(suspicious_score);
  builder.SetSiteEngagementLevel(site_engagement_level);
  builder.Record(ukm::UkmRecorder::Get());
}

// This enum backs the UKM Permission.PromptOptions, so it must be treated as
// append-only.
enum class UkmPromptOptions {
  APPROXIMATE_LOCATION = 1,
  PRECISE_LOCATION = 2,
};

UkmPromptOptions ToUkmPromptOptions(GeolocationAccuracy accuracy) {
  switch (accuracy) {
    case GeolocationAccuracy::kPrecise:
      return UkmPromptOptions::PRECISE_LOCATION;
    case GeolocationAccuracy::kApproximate:
      return UkmPromptOptions::APPROXIMATE_LOCATION;
  }
}

void RecordPermissionActionUkm(
    PermissionAction action,
    PermissionRequestGestureType gesture_type,
    ContentSettingsType permission,
    int dismiss_count,
    int ignore_count,
    PermissionSourceUI source_ui,
    base::TimeDelta time_to_action,
    PermissionPromptDisposition ui_disposition,
    std::optional<PermissionPromptDispositionReason> ui_reason,
    std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
    std::optional<bool> has_three_consecutive_denies,
    std::optional<bool> has_previously_revoked_permission,
    std::optional<PermissionUiSelector::PredictionGrantLikelihood>
        predicted_grant_likelihood,
    std::optional<PermissionRequestRelevance> permission_request_relevance,
    std::optional<permissions::PermissionAiRelevanceModel>
        permission_ai_relevance_model,
    PredictionRequestFeatures::ActionCounts
        loud_ui_actions_counts_for_request_type,
    PredictionRequestFeatures::ActionCounts loud_ui_actions_counts,
    PredictionRequestFeatures::ActionCounts actions_counts_for_request_type,
    PredictionRequestFeatures::ActionCounts actions_counts,
    std::optional<bool> prediction_decision_held_back,
    std::optional<UkmPromptOptions> prompt_options,
    std::optional<ukm::SourceId> source_id) {
  if (action == PermissionAction::REVOKED) {
    RecordUmaForWhetherRevocationUkmWasRecorded(permission,
                                                source_id.has_value());
  }

  // Only record the permission change if the origin is in the history.
  if (!source_id.has_value()) {
    return;
  }

  const int loud_ui_prompts_count_for_request_type =
      loud_ui_actions_counts_for_request_type.total();
  const int loud_ui_prompts_count = loud_ui_actions_counts.total();
  const int prompts_count_for_request_type =
      actions_counts_for_request_type.total();
  const int prompts_count = actions_counts.total();
  ukm::builders::Permission builder(source_id.value());
  builder.SetAction(static_cast<int64_t>(action))
      .SetGesture(static_cast<int64_t>(gesture_type))
      .SetPermissionType(static_cast<int64_t>(
          content_settings_uma_util::ContentSettingTypeToHistogramValue(
              permission)))
      .SetPriorDismissals(std::min(kPriorCountCap, dismiss_count))
      .SetPriorIgnores(std::min(kPriorCountCap, ignore_count))
      .SetSource(static_cast<int64_t>(source_ui))
      .SetPromptDisposition(static_cast<int64_t>(ui_disposition));

  if (prompt_options) {
    builder.SetPromptOptions(static_cast<int64_t>(prompt_options.value()));
  }

  builder
      .SetStats_LoudPromptsOfType_DenyRate(
          GetRoundedRatioForUkm(loud_ui_actions_counts_for_request_type.denies,
                                loud_ui_prompts_count_for_request_type))
      .SetStats_LoudPromptsOfType_DismissRate(GetRoundedRatioForUkm(
          loud_ui_actions_counts_for_request_type.dismissals,
          loud_ui_prompts_count_for_request_type))
      .SetStats_LoudPromptsOfType_GrantRate(
          GetRoundedRatioForUkm(loud_ui_actions_counts_for_request_type.grants,
                                loud_ui_prompts_count_for_request_type))
      .SetStats_LoudPromptsOfType_IgnoreRate(
          GetRoundedRatioForUkm(loud_ui_actions_counts_for_request_type.ignores,
                                loud_ui_prompts_count_for_request_type))
      .SetStats_LoudPromptsOfType_Count(
          BucketizeValue(loud_ui_prompts_count_for_request_type));

  builder
      .SetStats_LoudPrompts_DenyRate(GetRoundedRatioForUkm(
          loud_ui_actions_counts.denies, loud_ui_prompts_count))
      .SetStats_LoudPrompts_DismissRate(GetRoundedRatioForUkm(
          loud_ui_actions_counts.dismissals, loud_ui_prompts_count))
      .SetStats_LoudPrompts_GrantRate(GetRoundedRatioForUkm(
          loud_ui_actions_counts.grants, loud_ui_prompts_count))
      .SetStats_LoudPrompts_IgnoreRate(GetRoundedRatioForUkm(
          loud_ui_actions_counts.ignores, loud_ui_prompts_count))
      .SetStats_LoudPrompts_Count(BucketizeValue(loud_ui_prompts_count));

  builder
      .SetStats_AllPromptsOfType_DenyRate(
          GetRoundedRatioForUkm(actions_counts_for_request_type.denies,
                                prompts_count_for_request_type))
      .SetStats_AllPromptsOfType_DismissRate(
          GetRoundedRatioForUkm(actions_counts_for_request_type.dismissals,
                                prompts_count_for_request_type))
      .SetStats_AllPromptsOfType_GrantRate(
          GetRoundedRatioForUkm(actions_counts_for_request_type.grants,
                                prompts_count_for_request_type))
      .SetStats_AllPromptsOfType_IgnoreRate(
          GetRoundedRatioForUkm(actions_counts_for_request_type.ignores,
                                prompts_count_for_request_type))
      .SetStats_AllPromptsOfType_Count(
          BucketizeValue(prompts_count_for_request_type));

  builder
      .SetStats_AllPrompts_DenyRate(
          GetRoundedRatioForUkm(actions_counts.denies, prompts_count))
      .SetStats_AllPrompts_DismissRate(
          GetRoundedRatioForUkm(actions_counts.dismissals, prompts_count))
      .SetStats_AllPrompts_GrantRate(
          GetRoundedRatioForUkm(actions_counts.grants, prompts_count))
      .SetStats_AllPrompts_IgnoreRate(
          GetRoundedRatioForUkm(actions_counts.ignores, prompts_count))
      .SetStats_AllPrompts_Count(BucketizeValue(prompts_count));

  if (ui_reason.has_value()) {
    builder.SetPromptDispositionReason(static_cast<int64_t>(ui_reason.value()));
  }

  if (predicted_grant_likelihood.has_value()) {
    builder.SetPredictionsApiResponse_GrantLikelihood(
        static_cast<int64_t>(predicted_grant_likelihood.value()));
  }

  if (permission_request_relevance.has_value()) {
    builder.SetPermissionRequestRelevance(
        static_cast<int64_t>(permission_request_relevance.value()));
  }

  if (permission_ai_relevance_model.has_value()) {
    builder.SetPermissionAiRelevanceModel(
        static_cast<int64_t>(permission_ai_relevance_model.value()));
  }

  if (prediction_decision_held_back.has_value()) {
    builder.SetPredictionsApiResponse_Heldback(
        prediction_decision_held_back.value());
  }

  if (has_three_consecutive_denies.has_value()) {
    int64_t satisfied_adaptive_triggers = 0;
    if (has_three_consecutive_denies.value()) {
      satisfied_adaptive_triggers |=
          static_cast<int64_t>(AdaptiveTriggers::THREE_CONSECUTIVE_DENIES);
    }
    builder.SetSatisfiedAdaptiveTriggers(satisfied_adaptive_triggers);
  }

  if (has_previously_revoked_permission.has_value()) {
    int64_t previously_revoked_permission = 0;
    if (has_previously_revoked_permission.value()) {
      previously_revoked_permission = static_cast<int64_t>(
          PermissionAutoRevocationHistory::PREVIOUSLY_AUTO_REVOKED);
    }
    builder.SetPermissionAutoRevocationHistory(previously_revoked_permission);
  }

  if (ui_disposition == PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE &&
      variants.has_value()) {
    // Variant can have a maximum of 3 values, one per site level and 2 for OS
    // level.
    CHECK_LE(variants->size(), 3U);

    const std::vector<ElementAnchoredBubbleVariant>& variant_array =
        variants.value();

    for (ElementAnchoredBubbleVariant variant : variant_array) {
      switch (variant) {
        case ElementAnchoredBubbleVariant::kAdministratorGranted:
        case ElementAnchoredBubbleVariant::kPreviouslyGranted:
        case ElementAnchoredBubbleVariant::kAsk:
        case ElementAnchoredBubbleVariant::kPreviouslyDenied:
        case ElementAnchoredBubbleVariant::kAdministratorDenied:
          builder.SetSiteLevelScreen(static_cast<int64_t>(variant));
          break;
        case ElementAnchoredBubbleVariant::kOsPrompt:
          builder.SetOsPromptScreen(static_cast<int64_t>(variant));
          break;

        case ElementAnchoredBubbleVariant::kOsSystemSettings:
          builder.SetOsSystemSettingsScreen(static_cast<int64_t>(variant));
          break;
        case ElementAnchoredBubbleVariant::kUninitialized:
          break;
      }
    }
  }
  if (!time_to_action.is_zero()) {
    builder.SetTimeToDecision(ukm::GetExponentialBucketMinForUserTiming(
        time_to_action.InMilliseconds()));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void RecordElementAnchoredPermissionPromptActionUkm(
    RequestTypeForUma permission,
    RequestTypeForUma screen_permission,
    ElementAnchoredBubbleAction action,
    ElementAnchoredBubbleVariant variant,
    int screen_counter,
    std::optional<ukm::SourceId> source_id) {
  ukm::builders::Permissions_EmbeddedPromptAction builder(source_id.value());

  builder.SetVariant(static_cast<int64_t>(variant))
      .SetAction(static_cast<int64_t>(action))
      .SetPermissionType(static_cast<int64_t>(permission))
      .SetScreenPermissionType(static_cast<int64_t>(screen_permission))
      .SetPreviousScreens(std::min(kPriorCountCap, screen_counter));

  builder.Record(ukm::UkmRecorder::Get());
}

// |full_version| represented in the format `YYYY.M.D.m`, where m is the
// minute-of-day. Return int represented in the format `YYYYMMDD`.
// CrowdDeny versions published before 2020 will be reported as 1.
// Returns 0 if no version available.
// Returns 1 if a version has invalid format.
int ConvertCrowdDenyVersionToInt(const std::optional<base::Version>& version) {
  if (!version.has_value() || !version.value().IsValid()) {
    return 0;
  }

  const std::vector<uint32_t>& full_version = version.value().components();
  if (full_version.size() != 4) {
    return 1;
  }

  const int kCrowdDenyMinYearLimit = 2020;
  const int year = base::checked_cast<int>(full_version.at(0));
  if (year < kCrowdDenyMinYearLimit) {
    return 1;
  }

  const int month = base::checked_cast<int>(full_version.at(1));
  const int day = base::checked_cast<int>(full_version.at(2));

  int short_version = year;

  short_version *= 100;
  short_version += month;
  short_version *= 100;
  short_version += day;

  return short_version;
}

AutoDSEPermissionRevertTransition GetAutoDSEPermissionRevertedTransition(
    ContentSetting backed_up_setting,
    ContentSetting effective_setting,
    ContentSetting end_state_setting) {
  if (backed_up_setting == CONTENT_SETTING_ASK &&
      effective_setting == CONTENT_SETTING_ALLOW &&
      end_state_setting == CONTENT_SETTING_ASK) {
    return AutoDSEPermissionRevertTransition::NO_DECISION_ASK;
  } else if (backed_up_setting == CONTENT_SETTING_ALLOW &&
             effective_setting == CONTENT_SETTING_ALLOW &&
             end_state_setting == CONTENT_SETTING_ALLOW) {
    return AutoDSEPermissionRevertTransition::PRESERVE_ALLOW;
  } else if (backed_up_setting == CONTENT_SETTING_BLOCK &&
             effective_setting == CONTENT_SETTING_ALLOW &&
             end_state_setting == CONTENT_SETTING_ASK) {
    return AutoDSEPermissionRevertTransition::CONFLICT_ASK;
  } else if (backed_up_setting == CONTENT_SETTING_ASK &&
             effective_setting == CONTENT_SETTING_BLOCK &&
             end_state_setting == CONTENT_SETTING_BLOCK) {
    return AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ASK;
  } else if (backed_up_setting == CONTENT_SETTING_ALLOW &&
             effective_setting == CONTENT_SETTING_BLOCK &&
             end_state_setting == CONTENT_SETTING_BLOCK) {
    return AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ALLOW;
  } else if (backed_up_setting == CONTENT_SETTING_BLOCK &&
             effective_setting == CONTENT_SETTING_BLOCK &&
             end_state_setting == CONTENT_SETTING_BLOCK) {
    return AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_BLOCK;
  } else {
    return AutoDSEPermissionRevertTransition::INVALID_END_STATE;
  }
}

void RecordTopLevelPermissionsHeaderPolicy(
    ContentSettingsType content_settings_type,
    const std::string& histogram,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(IsCrossOriginSubframe(render_frame_host));

  // We only care about about permission types that have a corresponding
  // permission policy
  const auto feature =
      PermissionUtil::GetPermissionsPolicyFeature(content_settings_type);
  if (!feature.has_value()) {
    return;
  }

  // This function will only be called when we use/prompt a permission requested
  // from a cross-origin subframe. Being allowed by permission policy is a
  // necessary condition to use a permission in sub-frame.
  DCHECK(render_frame_host->IsFeatureEnabled(feature.value()));
  base::UmaHistogramEnumeration(histogram,
                                GetTopLevelPermissionHeaderPolicyForUMA(
                                    render_frame_host, feature.value()),
                                PermissionHeaderPolicyForUMA::NUM);
}

PermissionChangeInfo GetChangeInfo(bool is_used,
                                   bool show_infobar,
                                   bool page_reload) {
  if (show_infobar) {
    if (page_reload) {
      if (is_used) {
        return PermissionChangeInfo::kInfobarShownPageReloadPermissionUsed;
      } else {
        return PermissionChangeInfo::kInfobarShownPageReloadPermissionNotUsed;
      }

    } else {
      if (is_used) {
        return PermissionChangeInfo::kInfobarShownNoPageReloadPermissionUsed;
      } else {
        return PermissionChangeInfo::kInfobarShownNoPageReloadPermissionNotUsed;
      }
    }
  } else {
    if (page_reload) {
      if (is_used) {
        return PermissionChangeInfo::kInfobarNotShownPageReloadPermissionUsed;
      } else {
        return PermissionChangeInfo::
            kInfobarNotShownPageReloadPermissionNotUsed;
      }

    } else {
      if (is_used) {
        return PermissionChangeInfo::kInfobarNotShownNoPageReloadPermissionUsed;
      } else {
        return PermissionChangeInfo::
            kInfobarNotShownNoPageReloadPermissionNotUsed;
      }
    }
  }
}

std::string GetPermissionStringForUma(
    ContentSettingsType content_setting_type) {
  switch (content_setting_type) {
    case ContentSettingsType::GEOLOCATION:
      if (!base::FeatureList::IsEnabled(
              content_settings::features::kApproximateGeolocationPermission)) {
        return "Geolocation";
      }
      break;
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      if (base::FeatureList::IsEnabled(
              content_settings::features::kApproximateGeolocationPermission)) {
        return "Geolocation";
      }
      break;
    case ContentSettingsType::NOTIFICATIONS:
      return "Notifications";
    case ContentSettingsType::MIDI_SYSEX:
      return "MidiSysEx";
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMedia";
    case ContentSettingsType::MEDIASTREAM_MIC:
      return "AudioCapture";
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return "ClipboardReadWrite";
    case ContentSettingsType::PAYMENT_HANDLER:
      return "PaymentHandler";
    case ContentSettingsType::NFC:
      return "Nfc";
    case ContentSettingsType::VR:
      return "VR";
    case ContentSettingsType::AR:
      return "AR";
    case ContentSettingsType::HAND_TRACKING:
      return "HandTracking";
    case ContentSettingsType::STORAGE_ACCESS:
      return "StorageAccess";
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return "TopLevelStorageAccess";
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return "CameraPanTiltZoom";
    case ContentSettingsType::WINDOW_MANAGEMENT:
      return "WindowPlacement";
    case ContentSettingsType::LOCAL_FONTS:
      return "LocalFonts";
    case ContentSettingsType::IDLE_DETECTION:
      return "IdleDetection";
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      return "CapturedSurfaceControl";
    case ContentSettingsType::SMART_CARD_DATA:
      return "SmartCard";
    case ContentSettingsType::WEB_PRINTING:
      return "WebPrinting";
    case ContentSettingsType::POINTER_LOCK:
      return "PointerLock";
    case ContentSettingsType::KEYBOARD_LOCK:
      return "KeyboardLock";
    case ContentSettingsType::WEB_APP_INSTALLATION:
      return "WebAppInstallation";
    case ContentSettingsType::LOCAL_NETWORK_ACCESS:
      return "LocalNetworkAccess";
    default:
      break;
  }
  // The user is not prompted for these permissions thus there is no
  // permission action recorded for them.
  NOTREACHED() << "PERMISSION "
               << PermissionUtil::GetPermissionString(content_setting_type)
               << " not accounted for";
}

const char* GetPredictionGrantLikelihoodString(
    PermissionUiSelector::PredictionGrantLikelihood likelihood) {
  switch (likelihood) {
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED:
      return "Unspecified";
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY:
      return "VeryUnlikely";
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY:
      return "Unlikely";
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL:
      return "Neutral";
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY:
      return "Likely";
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY:
      return "VeryLikely";
  }
  NOTREACHED();
}

const char* GetProminenceString(PermissionPromptDisposition disposition) {
  if (PermissionUmaUtil::IsPromptDispositionQuiet(disposition)) {
    return "Quiet";
  } else if (PermissionUmaUtil::IsPromptDispositionLoud(disposition)) {
    return "Loud";
  }

  DUMP_WILL_BE_NOTREACHED();
  return "";
}

PermissionRequestLikelihood
ConvertPredictionGrantLikelihoodToPermissionRequestLikelihood(
    PermissionUiSelector::PredictionGrantLikelihood likelihood) {
  switch (likelihood) {
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED:
      return PermissionRequestLikelihood::kUnspecified;
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY:
      return PermissionRequestLikelihood::kVeryUnlikely;
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY:
      return PermissionRequestLikelihood::kUnlikely;
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL:
      return PermissionRequestLikelihood::kNeutral;
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY:
      return PermissionRequestLikelihood::kLikely;
    case PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY:
      return PermissionRequestLikelihood::kVeryLikely;
  }
}

}  // anonymous namespace

// PermissionUmaUtil ----------------------------------------------------------

const char PermissionUmaUtil::kPermissionsPromptShown[] =
    "Permissions.Prompt.Shown";
const char PermissionUmaUtil::kPermissionsPromptShownGesture[] =
    "Permissions.Prompt.Shown.Gesture";
const char PermissionUmaUtil::kPermissionsPromptShownNoGesture[] =
    "Permissions.Prompt.Shown.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptAccepted[] =
    "Permissions.Prompt.Accepted";
const char PermissionUmaUtil::kPermissionsPromptAcceptedGesture[] =
    "Permissions.Prompt.Accepted.Gesture";
const char PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture[] =
    "Permissions.Prompt.Accepted.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptAcceptedOnce[] =
    "Permissions.Prompt.AcceptedOnce";
const char PermissionUmaUtil::kPermissionsPromptAcceptedOnceGesture[] =
    "Permissions.Prompt.AcceptedOnce.Gesture";
const char PermissionUmaUtil::kPermissionsPromptAcceptedOnceNoGesture[] =
    "Permissions.Prompt.AcceptedOnce.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptDenied[] =
    "Permissions.Prompt.Denied";
const char PermissionUmaUtil::kPermissionsPromptDeniedGesture[] =
    "Permissions.Prompt.Denied.Gesture";
const char PermissionUmaUtil::kPermissionsPromptDeniedNoGesture[] =
    "Permissions.Prompt.Denied.NoGesture";
const char PermissionUmaUtil::kPermissionsPromptDismissed[] =
    "Permissions.Prompt.Dismissed";
const char PermissionUmaUtil::kPermissionsExperimentalUsagePrefix[] =
    "Permissions.Experimental.Usage.";
const char PermissionUmaUtil::kPermissionsActionPrefix[] =
    "Permissions.Action.";

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(ContentSettingsType content_type) {
  PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  base::UmaHistogramEnumeration("ContentSettings.PermissionRequested",
                                permission, PermissionType::NUM);
}

void PermissionUmaUtil::RecordActivityIndicator(
    std::set<ContentSettingsType> permissions,
    bool blocked,
    bool blocked_system_level,
    bool clicked) {
  DCHECK(!permissions.empty());
  DCHECK(permissions.contains(ContentSettingsType::MEDIASTREAM_CAMERA) ||
         permissions.contains(ContentSettingsType::MEDIASTREAM_MIC));

  ActivityIndicatorState state;
  if (blocked) {
    if (blocked_system_level) {
      state = ActivityIndicatorState::kBlockedOnSystemLevel;
    } else {
      state = ActivityIndicatorState::kBlockedOnSiteLevel;
    }
  } else {
    state = ActivityIndicatorState::kInUse;
  }

  std::string indicators_type;

  if (permissions.size() > 1) {
    indicators_type = "AudioAndVideoCapture";
  } else if (permissions.contains(ContentSettingsType::MEDIASTREAM_CAMERA)) {
    indicators_type = "VideoCapture";
  } else {
    indicators_type = "AudioCapture";
  }

  std::string action;
  if (clicked) {
    action = "Click";
  } else {
    action = "Show";
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Permissions.ActivityIndicator.LHS.", indicators_type, ".", action}),
      state);
}

void PermissionUmaUtil::RecordDismissalType(
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionPromptDisposition ui_disposition,
    DismissalType dismissalType) {
  std::optional<RequestType> request_type =
      ContentSettingsTypeToRequestTypeIfExists(content_settings_types[0]);
  if (!request_type.has_value()) {
    return;
  }
  RequestTypeForUma type =
      PermissionUtil::GetUmaValueForRequestType(request_type.value());

  if (content_settings_types.size() > 1) {
    type = RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE;
  }

  std::string permission_type = GetPermissionRequestString(type);
  std::string permission_disposition =
      GetPromptDispositionString(ui_disposition);
  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Prompt.", permission_type, ".",
                    permission_disposition, ".Dismissed.Method"}),
      dismissalType);
}

void PermissionUmaUtil::RecordPermissionRequestedFromFrame(
    ContentSettingsType content_settings_type,
    content::RenderFrameHost* rfh) {
  PermissionType permission;
  bool success =
      PermissionUtil::GetPermissionType(content_settings_type, &permission);
  DCHECK(success);

  if (IsCrossOriginWithAnyParent(rfh)) {
    base::UmaHistogramEnumeration("Permissions.Request.CrossOrigin", permission,
                                  PermissionType::NUM);
  } else {
    std::string frame_level =
        rfh->IsInPrimaryMainFrame() ? "MainFrame" : "SubFrame";

    base::UmaHistogramEnumeration(
        "Permissions.Request.SameOrigin." + frame_level, permission,
        PermissionType::NUM);
  }
}

void PermissionUmaUtil::PermissionRequestPreignored(PermissionType permission) {
  base::UmaHistogramEnumeration("Permissions.QuietPrompt.Preignore", permission,
                                PermissionType::NUM);
}

void PermissionUmaUtil::PermissionRevoked(
    ContentSettingsType permission,
    PermissionSourceUI source_ui,
    const GURL& revoked_origin,
    content::BrowserContext* browser_context) {
  DCHECK(PermissionUtil::IsPermission(permission));
  // An unknown gesture type is passed in since gesture type is only
  // applicable in prompt UIs where revocations are not possible.
  RecordPermissionAction(
      permission, PermissionAction::REVOKED, source_ui,
      PermissionRequestGestureType::UNKNOWN,
      /*time_to_action=*/base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /*ui_reason=*/std::nullopt, /*variants=*/std::nullopt, revoked_origin,
      /*web_contents=*/nullptr, browser_context,
      /*render_frame_host*/ nullptr,
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt, std::monostate());
}

void PermissionUmaUtil::RecordEmbargoPromptSuppression(
    PermissionEmbargoStatus embargo_status) {
  base::UmaHistogramEnumeration(
      "Permissions.AutoBlocker.EmbargoPromptSuppression", embargo_status,
      PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(
    content::PermissionStatusSource source) {
  // Explicitly switch to ensure that any new
  // PermissionStatusSource values are dealt with appropriately.
  switch (source) {
    case content::PermissionStatusSource::MULTIPLE_DISMISSALS:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_DISMISSALS);
      break;
    case content::PermissionStatusSource::MULTIPLE_IGNORES:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_IGNORES);
      break;
    case content::PermissionStatusSource::RECENT_DISPLAY:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::RECENT_DISPLAY);
      break;
    case content::PermissionStatusSource::UNSPECIFIED:
    case content::PermissionStatusSource::KILL_SWITCH:
    case content::PermissionStatusSource::ACTOR_OVERRIDE:
    case content::PermissionStatusSource::INSECURE_ORIGIN:
    case content::PermissionStatusSource::FEATURE_POLICY:
    case content::PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
    case content::PermissionStatusSource::FENCED_FRAME:
    case content::PermissionStatusSource::HEURISTIC_GRANT:
    case content::PermissionStatusSource::APP_LEVEL_SETTINGS:
      // The permission wasn't under embargo, so don't record anything. We may
      // embargo it later.
      break;
  }
}

void PermissionUmaUtil::RecordEmbargoStatus(
    PermissionEmbargoStatus embargo_status) {
  base::UmaHistogramEnumeration("Permissions.AutoBlocker.EmbargoStatus",
                                embargo_status, PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
    ContentSettingsType permission,
    bool is_used,
    bool show_infobar,
    bool page_reload) {
  PermissionChangeInfo change_info =
      GetChangeInfo(is_used, show_infobar, page_reload);

  std::string permission_string =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(permission)));

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.PageInfo.Changed.", permission_string,
                    ".Reallowed.Outcome"}),
      change_info);
}

void PermissionUmaUtil::RecordPermissionPromptAttempt(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    bool can_display_prompt) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type =
      PermissionUtil::GetUmaValueForRequests(requests);
  PermissionRequestGestureType gesture_type =
      requests.size() == 1 ? requests[0]->GetGestureType()
                           : PermissionRequestGestureType::UNKNOWN;

  std::string permission_type = GetPermissionRequestString(request_type);

  std::string gesture;

  switch (gesture_type) {
    case PermissionRequestGestureType::UNKNOWN: {
      gesture = "Unknown";
      break;
    }
    case PermissionRequestGestureType::GESTURE: {
      gesture = "Gesture";
      break;
    }
    case PermissionRequestGestureType::NO_GESTURE: {
      gesture = "NoGesture";
      break;
    }
    default:
      NOTREACHED();
  }

  std::string histogram_name = base::StrCat(
      {"Permissions.Prompt.", permission_type, ".", gesture, ".Attempt"});

  base::UmaHistogramBoolean(histogram_name, can_display_prompt);
}

void PermissionUmaUtil::PermissionPromptShown(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type =
      PermissionUtil::GetUmaValueForRequests(requests);
  PermissionRequestGestureType gesture_type =
      requests.size() == 1 ? requests[0]->GetGestureType()
                           : PermissionRequestGestureType::UNKNOWN;

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptShown, request_type);
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptShownGesture,
                                     kPermissionsPromptShownNoGesture,
                                     gesture_type, request_type);
}

void PermissionUmaUtil::PermissionPromptResolved(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    content::WebContents* web_contents,
    PermissionAction permission_action,
    base::TimeDelta time_to_action,
    PermissionPromptDisposition ui_disposition,
    std::optional<PermissionPromptDispositionReason> ui_reason,
    std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
    std::optional<PermissionUiSelector::PredictionGrantLikelihood>
        predicted_grant_likelihood,
    std::optional<PermissionRequestRelevance> permission_request_relevance,
    std::optional<permissions::PermissionAiRelevanceModel>
        permission_ai_relevance_model,
    std::optional<bool> prediction_decision_held_back,
    std::optional<permissions::PermissionIgnoredReason> ignored_reason,
    bool did_show_prompt,
    bool did_click_managed,
    bool did_click_learn_more) {
  switch (permission_action) {
    case PermissionAction::GRANTED:
      RecordPromptDecided(requests, /*accepted=*/true, /*is_one_time=*/false);
      break;
    case PermissionAction::DENIED:
      RecordPromptDecided(requests, /*accepted=*/false, /*is_one_time*/ false);
      break;
    case PermissionAction::DISMISSED:
    case PermissionAction::IGNORED:
      RecordIgnoreReason(requests, ui_disposition,
                         ignored_reason.value_or(
                             permissions::PermissionIgnoredReason::UNKNOWN));
      break;
    case PermissionAction::GRANTED_ONCE:
      RecordPromptDecided(requests, /*accepted=*/true, /*is_one_time*/ true);
      break;
    default:
      NOTREACHED();
  }
  std::string action_string = GetPermissionActionString(permission_action);
  RecordEngagementMetric(requests, web_contents, action_string);

  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          web_contents->GetBrowserContext());

  for (const auto& request : requests) {
    ContentSettingsType permission = request->GetContentSettingsType();
    // TODO(timloh): We only record these metrics for permissions which have a
    // ContentSettingsType, as otherwise they don't support GetGestureType.
    if (permission == ContentSettingsType::DEFAULT) {
      continue;
    }

    PermissionRequestGestureType gesture_type = request->GetGestureType();
    const GURL& requesting_origin = request->requesting_origin();

    RecordPermissionAction(
        permission, permission_action, PermissionSourceUI::PROMPT, gesture_type,
        time_to_action, ui_disposition, ui_reason, variants, requesting_origin,
        web_contents, web_contents->GetBrowserContext(),
        content::RenderFrameHost::FromID(request->get_requesting_frame_id()),
        predicted_grant_likelihood, permission_request_relevance,
        permission_ai_relevance_model, prediction_decision_held_back,
        request->prompt_options());

    std::string priorDismissPrefix = base::StrCat(
        {"Permissions.Prompt.", action_string, ".PriorDismissCount2."});
    std::string priorIgnorePrefix = base::StrCat(
        {"Permissions.Prompt.", action_string, ".PriorIgnoreCount2."});
    RecordPermissionPromptPriorCount(
        permission, priorDismissPrefix,
        autoblocker->GetDismissCount(requesting_origin, permission));
    RecordPermissionPromptPriorCount(
        permission, priorIgnorePrefix,
        autoblocker->GetIgnoreCount(requesting_origin, permission));
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Action.WithDisposition.",
                    GetPromptDispositionString(ui_disposition)}),
      permission_action, PermissionAction::NUM);

  RequestTypeForUma type = PermissionUtil::GetUmaValueForRequests(requests);

  std::string permission_type = GetPermissionRequestString(type);
  std::string permission_disposition =
      GetPromptDispositionString(ui_disposition);

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Prompt.", permission_type, ".",
                    permission_disposition, ".Action"}),
      permission_action, PermissionAction::NUM);

  if (!time_to_action.is_zero()) {
    base::UmaHistogramLongTimes(
        base::StrCat({"Permissions.Prompt.", permission_type, ".",
                      permission_disposition, ".", action_string,
                      ".TimeToAction"}),
        time_to_action);
  }

  if (requests.size() == 1 &&
      requests[0]->request_type() == RequestType::kGeolocation) {
    if (const auto* geolocation_options = std::get_if<GeolocationPromptOptions>(
            &requests[0]->prompt_options())) {
      base::UmaHistogramEnumeration(
          base::StrCat(
              {"Permissions.Prompt.Geolocation.", action_string, ".Accuracy"}),
          geolocation_options->selected_accuracy);
    }
  }

  if (permission_action == PermissionAction::IGNORED &&
      ui_disposition !=
          PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE &&
      ui_disposition != PermissionPromptDisposition::ANCHORED_BUBBLE) {
    base::UmaHistogramBoolean(
        base::StrCat({"Permissions.Prompt.", permission_type, ".",
                      permission_disposition, ".Ignored.DidShowBubble"}),
        did_show_prompt);
  }

  if (requests[0]->request_type() == RequestType::kGeolocation ||
      requests[0]->request_type() == RequestType::kNotifications) {
    if (ui_disposition ==
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP ||
        ui_disposition == PermissionPromptDisposition::MESSAGE_UI ||
        ui_disposition == PermissionPromptDisposition::MINI_INFOBAR) {
      base::UmaHistogramBoolean(
          base::StrCat({"Permissions.Prompt.", permission_type, ".",
                        permission_disposition, ".", action_string,
                        ".DidClickManage"}),
          did_click_managed);
    } else if (ui_disposition == PermissionPromptDisposition::
                                     LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP ||
               ui_disposition == PermissionPromptDisposition::MESSAGE_UI ||
               ui_disposition == PermissionPromptDisposition::MINI_INFOBAR) {
      base::UmaHistogramBoolean(
          base::StrCat({"Permissions.Prompt.", permission_type, ".",
                        permission_disposition, ".", action_string,
                        ".DidClickLearnMore"}),
          did_click_learn_more);
    }
  }

  // Record the permission action for the prediction service if the prediction
  // is UNLIKELY or VERY_UNLIKELY. `predicted_grant_likelihood` is only
  // populated by the prediction service for Notification and Geolocation
  // permissions.
  if (predicted_grant_likelihood.has_value() &&
      (predicted_grant_likelihood.value() ==
           PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY ||
       predicted_grant_likelihood.value() ==
           PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY) &&
      ui_disposition != PermissionPromptDisposition::NONE_VISIBLE &&
      ui_disposition != PermissionPromptDisposition::NOT_APPLICABLE) {
    const char* prominence_string = GetProminenceString(ui_disposition);
    std::string histogram_name = base::StrCat(
        {"Permissions.PredictionService.Action.", permission_type, ".",
         GetPredictionGrantLikelihoodString(predicted_grant_likelihood.value()),
         ".", prominence_string});
    base::UmaHistogramEnumeration(histogram_name, permission_action,
                                  PermissionAction::NUM);
  }

  if (predicted_grant_likelihood.has_value() &&
      (requests[0]->request_type() == RequestType::kGeolocation ||
       requests[0]->request_type() == RequestType::kNotifications)) {
    PermissionRequestGestureType gesture_type =
        requests.size() == 1 ? requests[0]->GetGestureType()
                             : PermissionRequestGestureType::UNKNOWN;
    if (gesture_type != PermissionRequestGestureType::UNKNOWN) {
      std::string gesture_suffix;
      if (gesture_type == PermissionRequestGestureType::GESTURE) {
        gesture_suffix = ".Gesture";
      } else {
        gesture_suffix = ".NoGesture";
      }

      PrefService* prefs =
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext());
      const bool is_msbb_enabled = prefs->GetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);

      std::string histogram_name;
      if (is_msbb_enabled) {
        histogram_name = base::StrCat({"Permissions.PredictionService.",
                                       permission_type, gesture_suffix});
      } else {
        histogram_name = base::StrCat({"Permissions.PredictionService.NoMSBB.",
                                       permission_type, gesture_suffix});
      }

      base::UmaHistogramEnumeration(
          histogram_name,
          ConvertPredictionGrantLikelihoodToPermissionRequestLikelihood(
              predicted_grant_likelihood.value()));
    }
  }

  // `NOT_APPLICABLE` and `NONE_VISIBLE` are special types of disposition that
  // do not really represent a visible prompt, hence they should be skipped.
  if ((requests[0]->request_type() == RequestType::kGeolocation ||
       requests[0]->request_type() == RequestType::kNotifications) &&
      ui_disposition != PermissionPromptDisposition::NOT_APPLICABLE &&
      ui_disposition != PermissionPromptDisposition::NONE_VISIBLE) {
    PermissionRequestGestureType gesture_type =
        requests.size() == 1 ? requests[0]->GetGestureType()
                             : PermissionRequestGestureType::UNKNOWN;
    if (gesture_type != PermissionRequestGestureType::UNKNOWN) {
      std::string gesture_suffix;
      if (gesture_type == PermissionRequestGestureType::GESTURE) {
        gesture_suffix = ".Gesture";
      } else {
        gesture_suffix = ".NoGesture";
      }
      const char* prominence_string = GetProminenceString(ui_disposition);
      std::string histogram_name = base::StrCat(
          {"Permissions.PredictionService.Action.", permission_type,
           gesture_suffix, ".", prominence_string});
      base::UmaHistogramEnumeration(histogram_name, permission_action,
                                    PermissionAction::NUM);
    }
  }
}  // namespace permissions

void PermissionUmaUtil::RecordPermissionPromptPriorCount(
    ContentSettingsType permission,
    const std::string& prefix,
    int count) {
  // The user is not prompted for this permissions, thus there is no prompt
  // event to record a prior count for.
  DCHECK_NE(ContentSettingsType::BACKGROUND_SYNC, permission);

  // Expand UMA_HISTOGRAM_COUNTS_100 so that we can use a dynamically suffixed
  // histogram name.
  base::Histogram::FactoryGet(
      prefix + PermissionUtil::GetPermissionString(permission), 1, 100, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(count);
}

void PermissionUmaUtil::RecordCrowdDenyDelayedPushNotification(
    base::TimeDelta delay) {
  base::UmaHistogramTimes(
      "Permissions.CrowdDeny.PreloadData.DelayedPushNotification", delay);
}

void PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
    const std::optional<base::Version>& version) {
  base::UmaHistogramSparse(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime",
      ConvertCrowdDenyVersionToInt(version));
}

void PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
    bool should_show,
    const std::vector<ContentSettingsType>& content_settings_types) {
  for (const auto& content_settings_type : content_settings_types) {
    base::UmaHistogramBoolean(
        "Permissions.MissingOSLevelPermission.ShouldShow." +
            PermissionUtil::GetPermissionString(content_settings_type),
        should_show);
  }
}

void PermissionUmaUtil::RecordMissingPermissionInfobarAction(
    PermissionAction action,
    const std::vector<ContentSettingsType>& content_settings_types) {
  for (const auto& content_settings_type : content_settings_types) {
    base::UmaHistogramEnumeration(
        "Permissions.MissingOSLevelPermission.Action." +
            PermissionUtil::GetPermissionString(content_settings_type),
        action, PermissionAction::NUM);
  }
}

PermissionUmaUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    content::BrowserContext* browser_context,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : browser_context_(browser_context),
      primary_url_(primary_url),
      secondary_url_(secondary_url),
      content_type_(content_type),
      source_ui_(source_ui) {
  if (!primary_url_.is_valid() ||
      (!secondary_url_.is_valid() && !secondary_url_.is_empty()) ||
      !IsRequestablePermissionType(content_type_) ||
      !PermissionUtil::IsPermission(content_type_)) {
    is_initially_allowed_ = false;
    return;
  }
  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      content_type);
  CHECK(info);
  auto* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  content_settings::SettingInfo setting_info;
  PermissionSetting initial_setting = settings_map->GetPermissionSetting(
      primary_url_, secondary_url_, content_type_, &setting_info);
  is_initially_allowed_ =
      info->delegate().IsAnyPermissionAllowed(initial_setting);
  last_modified_date_ = setting_info.metadata.last_modified();
  scoped_revocation_reporter_in_scope = true;
}

PermissionUmaUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    content::BrowserContext* browser_context,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : ScopedRevocationReporter(
          browser_context,
          GURL(primary_pattern.ToString()),
          GURL((secondary_pattern == ContentSettingsPattern::Wildcard())
                   ? primary_pattern.ToString()
                   : secondary_pattern.ToString()),
          content_type,
          source_ui) {}

PermissionUmaUtil::ScopedRevocationReporter::~ScopedRevocationReporter() {
  scoped_revocation_reporter_in_scope = false;
  if (!is_initially_allowed_) {
    return;
  }
  if (!IsRequestablePermissionType(content_type_) ||
      !PermissionUtil::IsPermission(content_type_)) {
    return;
  }
  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      content_type_);
  HostContentSettingsMap* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  PermissionSetting final_setting = settings_map->GetPermissionSetting(
      primary_url_, secondary_url_, content_type_);
  if (!info->delegate().IsAnyPermissionAllowed(final_setting)) {
    // PermissionUmaUtil takes origins, even though they're typed as GURL.
    GURL requesting_origin = primary_url_.DeprecatedGetOriginAsURL();
    PermissionRevoked(content_type_, source_ui_, requesting_origin,
                      browser_context_);
    if ((content_type_ == ContentSettingsType::GEOLOCATION ||
         content_type_ == ContentSettingsType::GEOLOCATION_WITH_OPTIONS ||
         content_type_ == ContentSettingsType::MEDIASTREAM_CAMERA ||
         content_type_ == ContentSettingsType::MEDIASTREAM_MIC) &&
        !last_modified_date_.is_null()) {
      RecordTimeElapsedBetweenGrantAndRevoke(
          content_type_, base::Time::Now() - last_modified_date_);
    }
  }
}

bool PermissionUmaUtil::ScopedRevocationReporter::IsInstanceInScope() {
  return scoped_revocation_reporter_in_scope;
}

void PermissionUmaUtil::RecordPermissionUsage(
    ContentSettingsType permission_type,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& requesting_origin) {
  PermissionsClient::Get()->GetUkmSourceId(
      permission_type, browser_context, web_contents, requesting_origin,
      base::BindOnce(&RecordPermissionUsageUkm, permission_type));
}

void PermissionUmaUtil::RecordPermissionUsageNotificationShown(
    bool did_user_always_allow_notifications,
    bool is_allowlisted,
    int suspicious_score,
    content::BrowserContext* browser_context,
    const GURL& requesting_origin,
    uint64_t site_engagement_level) {
  PermissionsClient::Get()->GetUkmSourceId(
      ContentSettingsType::NOTIFICATIONS, browser_context, nullptr,
      requesting_origin,
      base::BindOnce(&RecordPermissionUsageNotificationShownUkm,
                     did_user_always_allow_notifications, is_allowlisted,
                     suspicious_score, site_engagement_level));
}

void PermissionUmaUtil::RecordPermissionAction(
    ContentSettingsType permission,
    PermissionAction action,
    PermissionSourceUI source_ui,
    PermissionRequestGestureType gesture_type,
    base::TimeDelta time_to_action,
    PermissionPromptDisposition ui_disposition,
    std::optional<PermissionPromptDispositionReason> ui_reason,
    std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
    const GURL& requesting_origin,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host,
    std::optional<PermissionUiSelector::PredictionGrantLikelihood>
        predicted_grant_likelihood,
    std::optional<PermissionRequestRelevance> permission_request_relevance,
    std::optional<permissions::PermissionAiRelevanceModel>
        permission_ai_relevance_model,
    std::optional<bool> prediction_decision_held_back,
    const PromptOptions& prompt_options) {
  DCHECK(PermissionUtil::IsPermission(permission));
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context);
  int dismiss_count =
      autoblocker->GetDismissCount(requesting_origin, permission);
  int ignore_count = autoblocker->GetIgnoreCount(requesting_origin, permission);

  const base::Time cutoff = base::Time::Now() - base::Days(28);
  PermissionActionsHistory* permission_actions_history =
      PermissionsClient::Get()->GetPermissionActionsHistory(browser_context);

  PredictionRequestFeatures::ActionCounts
      loud_ui_actions_counts_per_request_type;
  PredictionRequestFeatures::ActionCounts loud_ui_actions_counts;
  PredictionRequestFeatures::ActionCounts actions_counts_per_request_type;
  PredictionRequestFeatures::ActionCounts actions_counts;

  if (permission_actions_history != nullptr) {
    DCHECK(IsRequestablePermissionType(permission));
    auto loud_ui_actions_per_request_type =
        permission_actions_history->GetHistory(
            cutoff, ContentSettingsTypeToRequestType(permission),
            PermissionActionsHistory::EntryFilter::WANT_LOUD_PROMPTS_ONLY);
    PermissionActionsHistory::FillInActionCounts(
        &loud_ui_actions_counts_per_request_type,
        loud_ui_actions_per_request_type);

    auto loud_ui_actions = permission_actions_history->GetHistory(
        cutoff, PermissionActionsHistory::EntryFilter::WANT_LOUD_PROMPTS_ONLY);
    PermissionActionsHistory::FillInActionCounts(&loud_ui_actions_counts,
                                                 loud_ui_actions);

    auto actions_per_request_type = permission_actions_history->GetHistory(
        cutoff, ContentSettingsTypeToRequestType(permission),
        PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
    PermissionActionsHistory::FillInActionCounts(
        &actions_counts_per_request_type, actions_per_request_type);

    auto actions = permission_actions_history->GetHistory(
        cutoff, PermissionActionsHistory::EntryFilter::WANT_ALL_PROMPTS);
    PermissionActionsHistory::FillInActionCounts(&actions_counts, actions);
  }

  if (action == PermissionAction::REVOKED) {
    RecordUmaForRevocationSourceUI(permission, source_ui);
  }

  std::optional<UkmPromptOptions> ukm_prompt_options;
  if (permission == ContentSettingsType::GEOLOCATION_WITH_OPTIONS) {
    if (const auto* geolocation_options =
            std::get_if<GeolocationPromptOptions>(&prompt_options)) {
      ukm_prompt_options =
          ToUkmPromptOptions(geolocation_options->selected_accuracy);
    }
  }

  PermissionsClient::Get()->GetUkmSourceId(
      permission, browser_context, web_contents, requesting_origin,
      base::BindOnce(
          &RecordPermissionActionUkm, action, gesture_type, permission,
          dismiss_count, ignore_count, source_ui, time_to_action,
          ui_disposition, ui_reason, variants,
          permission == ContentSettingsType::NOTIFICATIONS
              ? PermissionsClient::Get()
                    ->HadThreeConsecutiveNotificationPermissionDenies(
                        browser_context)
              : std::nullopt,
          PermissionsClient::Get()->HasPreviouslyAutoRevokedPermission(
              browser_context, requesting_origin, permission),
          predicted_grant_likelihood, permission_request_relevance,
          permission_ai_relevance_model,
          loud_ui_actions_counts_per_request_type, loud_ui_actions_counts,
          actions_counts_per_request_type, actions_counts,
          prediction_decision_held_back, ukm_prompt_options));

  if (render_frame_host && IsCrossOriginSubframe(render_frame_host)) {
    RecordCrossOriginFrameActionAndPolicyConfiguration(permission, action,
                                                       render_frame_host);
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Permissions.Action.", GetPermissionStringForUma(permission)}),
      action, PermissionAction::NUM);
}

// static
void PermissionUmaUtil::RecordPromptDecided(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    bool accepted,
    bool is_one_time) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type =
      PermissionUtil::GetUmaValueForRequests(requests);
  PermissionRequestGestureType gesture_type =
      requests.size() == 1 ? requests[0]->GetGestureType()
                           : PermissionRequestGestureType::UNKNOWN;

  if (accepted) {
    if (is_one_time) {
      PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptAcceptedOnce, request_type);
      PERMISSION_BUBBLE_GESTURE_TYPE_UMA(
          kPermissionsPromptAcceptedOnceGesture,
          kPermissionsPromptAcceptedOnceNoGesture, gesture_type, request_type);
    } else {
      PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptAccepted, request_type);
      PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptAcceptedGesture,
                                         kPermissionsPromptAcceptedNoGesture,
                                         gesture_type, request_type);
    }
  } else {
    PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptDenied, request_type);
    PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptDeniedGesture,
                                       kPermissionsPromptDeniedNoGesture,
                                       gesture_type, request_type);
  }
}

void PermissionUmaUtil::RecordTimeElapsedBetweenGrantAndUse(
    ContentSettingsType type,
    base::TimeDelta delta,
    content_settings::SettingSource source) {
  using content_settings::SettingSource;
  std::string base_histogram = "Permissions.Usage.ElapsedTimeSinceGrant." +
                               PermissionUtil::GetPermissionString(type);
  std::string source_suffix;
  switch (source) {
    case SettingSource::kNone:
    case SettingSource::kTest:
      source_suffix = "FromNone";
      break;
    case SettingSource::kPolicy:
      source_suffix = "FromPolicy";
      break;
    case SettingSource::kExtension:
      source_suffix = "FromExtension";
      break;
    case SettingSource::kUser:
      source_suffix = "FromUser";
      break;
    case SettingSource::kAllowList:
      source_suffix = "FromAllowlist";
      break;
    case SettingSource::kSupervised:
      source_suffix = "FromSupervised";
      break;
    case SettingSource::kInstalledWebApp:
      source_suffix = "FromInstalledWebApp";
      break;
    case SettingSource::kTpcdGrant:
      source_suffix = "FromSourceTpcdGrant";
      break;
    case SettingSource::kOsJavascriptOptimizer:
      source_suffix = "FromOsJavascriptOptimizer";
      break;
  }
  base::UmaHistogramCustomCounts(base_histogram, delta.InSeconds(), 1,
                                 base::Days(365).InSeconds(), 100);
  if (!source_suffix.empty()) {
    base::UmaHistogramCustomCounts(
        base::StrCat({base_histogram, ".", source_suffix}), delta.InSeconds(),
        1, base::Days(365).InSeconds(), 100);
  }
}

void PermissionUmaUtil::RecordTimeElapsedBetweenGrantAndRevoke(
    ContentSettingsType type,
    base::TimeDelta delta) {
  base::UmaHistogramCustomCounts(
      "Permissions.Revocation.ElapsedTimeSinceGrant." +
          PermissionUtil::GetPermissionString(type),
      delta.InSeconds(), 1, base::Days(365).InSeconds(), 100);
}

// static
void PermissionUmaUtil::RecordAutoDSEPermissionReverted(
    ContentSettingsType permission_type,
    ContentSetting backed_up_setting,
    ContentSetting effective_setting,
    ContentSetting end_state_setting) {
  std::string permission_string =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(permission_type)));
  auto transition = GetAutoDSEPermissionRevertedTransition(
      backed_up_setting, effective_setting, end_state_setting);
  base::UmaHistogramEnumeration(
      "Permissions.DSE.AutoPermissionRevertTransition." + permission_string,
      transition);
}

// static
void PermissionUmaUtil::RecordDSEEffectiveSetting(
    ContentSettingsType permission_type,
    ContentSetting setting) {
  std::string permission_string =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(permission_type)));
  base::UmaHistogramEnumeration(
      "Permissions.DSE.EffectiveSetting." + permission_string, setting,
      CONTENT_SETTING_NUM_SETTINGS);
}

// static
void PermissionUmaUtil::RecordPermissionPredictionConcurrentRequests(
    RequestType request_type) {
  permissions::PermissionPredictionSupportedType prediction_supported_type =
      request_type == permissions::RequestType::kNotifications
          ? permissions::PermissionPredictionSupportedType::kNotifications
          : permissions::PermissionPredictionSupportedType::kGeolocation;

  base::UmaHistogramEnumeration(
      "Permissions.PredictionService.ConcurrentRequests",
      prediction_supported_type);
}

// static
void PermissionUmaUtil::RecordPermissionPredictionSource(
    PermissionPredictionSource prediction_source,
    RequestType request_type) {
  std::string permission_string = GetPermissionRequestString(
      PermissionUtil::GetUmaValueForRequestType(request_type));
  base::UmaHistogramEnumeration(
      "Permissions.PredictionServiceSource." + permission_string,
      prediction_source);
}

// static
void PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
    RequestType request_type,
    PredictionModelType model_type,
    bool is_heldback) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Permissions.", GetPredictionModelString(model_type), ".Response.",
           GetPermissionRequestString(
               PermissionUtil::GetUmaValueForRequestType(request_type))}),
      is_heldback);
}

// static
std::string PermissionUmaUtil::GetOneTimePermissionEventHistogram(
    ContentSettingsType type) {
  // `FILE_SYSTEM_WRITE_GUARD` is not part of `OneTimePermission`,
  // (i.e. `DoesSupportTemporaryGrants()`), but it uses its background expiry
  // flow. As a result, allow logging for this event.
  DCHECK(permissions::PermissionUtil::DoesSupportTemporaryGrants(type) ||
         type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  std::string permission_type =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(type)));
  return base::StrCat(
      {"Permissions.OneTimePermission.", permission_type, ".Event"});
}

// static
std::string PermissionUmaUtil::GetPredictionModelString(
    PredictionModelType model_type) {
  switch (model_type) {
    case PredictionModelType::kServerSideCpssV3Model:
      return "PredictionService";
    case PredictionModelType::kOnDeviceCpssV1Model:
      return "OnDevicePredictionService";
    case PredictionModelType::kOnDeviceAiV1Model:
      return "AIv1";
    case PredictionModelType::kOnDeviceAiV3Model:
      return "AIv3";
    case PredictionModelType::kOnDeviceAiV4Model:
      return "AIv4";
    default:
      NOTREACHED();
  }
}

// static
void PermissionUmaUtil::RecordOneTimePermissionEvent(
    ContentSettingsType type,
    OneTimePermissionEvent event) {
  base::UmaHistogramEnumeration(GetOneTimePermissionEventHistogram(type),
                                event);
}

// static
void PermissionUmaUtil::RecordPageInfoPermissionChangeWithin1m(
    ContentSettingsType type,
    PermissionAction previous_action,
    ContentSetting setting_after) {
  DCHECK(IsRequestablePermissionType(type));
  std::string permission_type =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(type)));
  std::string histogram_name =
      "Permissions.PageInfo.ChangedWithin1m." + permission_type;
  switch (previous_action) {
    case PermissionAction::GRANTED:
    case PermissionAction::GRANTED_ONCE:
      if (setting_after == ContentSetting::CONTENT_SETTING_BLOCK) {
        base::UmaHistogramEnumeration(histogram_name,
                                      PermissionChangeAction::REVOKED);
      } else if (setting_after == ContentSetting::CONTENT_SETTING_DEFAULT) {
        base::UmaHistogramEnumeration(
            histogram_name, PermissionChangeAction::RESET_FROM_ALLOWED);
      }
      break;
    case PermissionAction::DENIED:
      if (setting_after == ContentSetting::CONTENT_SETTING_ALLOW) {
        base::UmaHistogramEnumeration(histogram_name,
                                      PermissionChangeAction::REALLOWED);
      } else if (setting_after == ContentSetting::CONTENT_SETTING_DEFAULT) {
        base::UmaHistogramEnumeration(
            histogram_name, PermissionChangeAction::RESET_FROM_DENIED);
      }
      break;
    case PermissionAction::DISMISSED:  // dismissing had no effect on content
                                       // setting
    case PermissionAction::IGNORED:    // ignoring has no effect on content
                                       // settings
    case PermissionAction::REVOKED:  // not a relevant use case for this metric
    case PermissionAction::NUM:      // placeholder
      break;                         // NOP
  }
}

// static
void PermissionUmaUtil::RecordPageInfoPermissionChange(
    ContentSettingsType type,
    ContentSetting setting_before,
    ContentSetting setting_after,
    bool suppress_reload_page_bar) {
  DCHECK(IsRequestablePermissionType(type));
  // Currently only Camera and Mic are supported.
  DCHECK(type == ContentSettingsType::MEDIASTREAM_MIC ||
         type == ContentSettingsType::MEDIASTREAM_CAMERA);
  std::string permission_type =
      GetPermissionRequestString(PermissionUtil::GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(type)));
  std::string histogram_name =
      "Permissions.PageInfo.Changed." + permission_type;

  if (suppress_reload_page_bar) {
    histogram_name = histogram_name + ".ReloadInfobarNotShown";
  } else {
    histogram_name = histogram_name + ".ReloadInfobarShown";
  }

  if (setting_before == ContentSetting::CONTENT_SETTING_BLOCK) {
    if (setting_after == ContentSetting::CONTENT_SETTING_ALLOW) {
      base::UmaHistogramEnumeration(histogram_name,
                                    PermissionChangeAction::REALLOWED);
    } else if (setting_after == ContentSetting::CONTENT_SETTING_ASK ||
               setting_after == ContentSetting::CONTENT_SETTING_DEFAULT) {
      base::UmaHistogramEnumeration(histogram_name,
                                    PermissionChangeAction::RESET_FROM_DENIED);
    } else {
      DUMP_WILL_BE_NOTREACHED() << setting_before << " " << setting_after;
    }
  } else if (setting_before == ContentSetting::CONTENT_SETTING_ALLOW) {
    if (setting_after == ContentSetting::CONTENT_SETTING_BLOCK) {
      base::UmaHistogramEnumeration(histogram_name,
                                    PermissionChangeAction::REVOKED);
    } else if (setting_after == ContentSetting::CONTENT_SETTING_ASK ||
               setting_after == ContentSetting::CONTENT_SETTING_DEFAULT) {
      base::UmaHistogramEnumeration(histogram_name,
                                    PermissionChangeAction::RESET_FROM_ALLOWED);
    } else if (setting_after == ContentSetting::CONTENT_SETTING_ALLOW) {
      base::UmaHistogramEnumeration(
          histogram_name, PermissionChangeAction::REMEMBER_CHECKBOX_TOGGLED);
    } else {
      NOTREACHED();
    }
  }
}

// static
void PermissionUmaUtil::RecordPageReloadInfoBarShown(bool shown) {
  base::UmaHistogramBoolean(
      "Permissions.QuietPrompt.Preignore.PageReloadInfoBar", shown);
}

// static
std::string PermissionUmaUtil::GetPermissionActionString(
    PermissionAction permission_action) {
  switch (permission_action) {
    case PermissionAction::GRANTED:
      return "Accepted";
    case PermissionAction::DENIED:
      return "Denied";
    case PermissionAction::DISMISSED:
      return "Dismissed";
    case PermissionAction::IGNORED:
      return "Ignored";
    case PermissionAction::GRANTED_ONCE:
      return "AcceptedOnce";
    default:
      NOTREACHED();
  }
}

// static
std::string PermissionUmaUtil::GetPromptDispositionString(
    PermissionPromptDisposition ui_disposition) {
  switch (ui_disposition) {
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
      return "AnchoredBubble";
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
      return "CustomModalDialog";
    case PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE:
      return "ElementAnchoredBubble";
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP:
      return "LocationBarLeftQuietChip";
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP:
      return "LocationBarLeftQuietAbusiveChip";
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE:
      return "LocationBarLeftChipAutoBubble";
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON:
      return "LocationBarRightAnimatedIcon";
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON:
      return "LocationBarRightStaticIcon";
    case PermissionPromptDisposition::MINI_INFOBAR:
      return "MiniInfobar";
    case PermissionPromptDisposition::MESSAGE_UI:
      return "MessageUI";
    case PermissionPromptDisposition::MODAL_DIALOG:
      return "ModalDialog";
    case PermissionPromptDisposition::NONE_VISIBLE:
      return "NoneVisible";
    case PermissionPromptDisposition::NOT_APPLICABLE:
      return "NotApplicable";
    case PermissionPromptDisposition::MAC_OS_PROMPT:
      return "MacOsPrompt";
    case PermissionPromptDisposition::MESSAGE_UI_LOUD:
      return "MessageUILoud";
  }

  NOTREACHED();
}

// static
std::string PermissionUmaUtil::GetPromptDispositionReasonString(
    PermissionPromptDispositionReason ui_disposition_reason) {
  switch (ui_disposition_reason) {
    case PermissionPromptDispositionReason::DEFAULT_FALLBACK:
      return "DefaultFallback";
    case PermissionPromptDispositionReason::ON_DEVICE_PREDICTION_MODEL:
      return "OnDevicePredictionModel";
    case PermissionPromptDispositionReason::PREDICTION_SERVICE:
      return "PredictionService";
    case PermissionPromptDispositionReason::SAFE_BROWSING_VERDICT:
      return "SafeBrowsingVerdict";
    case PermissionPromptDispositionReason::USER_PREFERENCE_IN_SETTINGS:
      return "UserPreferenceInSettings";
  }

  NOTREACHED();
}

// static
std::string PermissionUmaUtil::GetRequestTypeString(RequestType request_type) {
  return GetPermissionRequestString(
      PermissionUtil::GetUmaValueForRequestType(request_type));
}

// static
bool PermissionUmaUtil::IsPromptDispositionQuiet(
    PermissionPromptDisposition prompt_disposition) {
  switch (prompt_disposition) {
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP:
    case PermissionPromptDisposition::MINI_INFOBAR:
    case PermissionPromptDisposition::MESSAGE_UI:
      return true;
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
    case PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE:
    case PermissionPromptDisposition::MODAL_DIALOG:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE:
    case PermissionPromptDisposition::NONE_VISIBLE:
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
    case PermissionPromptDisposition::NOT_APPLICABLE:
    case PermissionPromptDisposition::MAC_OS_PROMPT:
    case PermissionPromptDisposition::MESSAGE_UI_LOUD:
      return false;
  }
}

// static
bool PermissionUmaUtil::IsPromptDispositionLoud(
    PermissionPromptDisposition prompt_disposition) {
  switch (prompt_disposition) {
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
    case PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE:
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
    case PermissionPromptDisposition::MODAL_DIALOG:
    case PermissionPromptDisposition::MAC_OS_PROMPT:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE:
    case PermissionPromptDisposition::MESSAGE_UI_LOUD:
      return true;
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP:
    case PermissionPromptDisposition::MINI_INFOBAR:
    case PermissionPromptDisposition::MESSAGE_UI:
    case PermissionPromptDisposition::NONE_VISIBLE:
    case PermissionPromptDisposition::NOT_APPLICABLE:
      return false;
  }
}

// static
void PermissionUmaUtil::RecordIgnoreReason(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    PermissionPromptDisposition prompt_disposition,
    PermissionIgnoredReason reason) {
  RequestTypeForUma request_type =
      PermissionUtil::GetUmaValueForRequests(requests);

  std::string histogram_name = base::StrCat(
      {"Permissions.Prompt.", GetPermissionRequestString(request_type), ".",
       GetPromptDispositionString(prompt_disposition), ".IgnoredReason"});
  base::UmaHistogramEnumeration(histogram_name, reason,
                                PermissionIgnoredReason::NUM);
}

// static
void PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
    ContentSettingsType content_settings_type,
    content::RenderFrameHost* render_frame_host) {
  const bool is_cross_origin_subframe =
      IsCrossOriginSubframe(render_frame_host);
  const auto usage_histogram = base::StrCat(
      {kPermissionsExperimentalUsagePrefix,
       PermissionUtil::GetPermissionString(content_settings_type)});
  base::UmaHistogramBoolean(
      base::StrCat({usage_histogram, ".IsCrossOriginFrame"}),
      is_cross_origin_subframe);
  if (is_cross_origin_subframe) {
    RecordTopLevelPermissionsHeaderPolicy(
        content_settings_type,
        base::StrCat(
            {usage_histogram, ".CrossOriginFrame.TopLevelHeaderPolicy"}),
        render_frame_host);
  }
}

// static
void PermissionUmaUtil::RecordElementAnchoredBubbleDismiss(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    DismissedReason reason) {
  CHECK(!requests.empty());

  RequestTypeForUma type = PermissionUtil::GetUmaValueForRequests(requests);

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Prompt.", GetPermissionRequestString(type),
                    ".ElementAnchoredBubble.DismissedReason"}),
      reason);
}

// static
void PermissionUmaUtil::RecordElementAnchoredBubbleOsMetrics(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    OsScreen screen,
    OsScreenAction action,
    base::TimeDelta time_to_action) {
  CHECK(!requests.empty());

  RequestTypeForUma type = PermissionUtil::GetUmaValueForRequests(requests);

  std::string screen_type;
  switch (screen) {
    case OsScreen::kOsPrompt:
      screen_type = "OsPrompt";
      break;
    case OsScreen::kOsSystemSettings:
      screen_type = "OsSystemSettings";
      break;
    default:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Prompt.", GetPermissionRequestString(type),
                    ".ElementAnchoredBubble.", screen_type, ".OsScreenAction"}),
      action);

  std::string screen_action;
  if (!time_to_action.is_zero()) {
    switch (action) {
      case OsScreenAction::kSystemSettings:
        screen_action = "SystemSettings";
        break;
      case OsScreenAction::kDismissedXButton:
        screen_action = "DismissedXButton";
        break;
      case OsScreenAction::kDismissedScrim:
        screen_action = "DismissedScrim";
        break;
      case OsScreenAction::kOsPromptDenied:
        screen_action = "OsPromptDenied";
        break;
      case OsScreenAction::kOsPromptAllowed:
        screen_action = "OsPromptAllowed";
        break;
      default:
        NOTREACHED();
    }

    base::UmaHistogramLongTimes(
        base::StrCat({"Permissions.Prompt.", GetPermissionRequestString(type),
                      ".ElementAnchoredBubble.", screen_type, ".",
                      screen_action, ".TimeToAction"}),
        time_to_action);
  }
}

void PermissionUmaUtil::RecordElementAnchoredBubbleVariantUMA(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    ElementAnchoredBubbleVariant variant) {
  CHECK(!requests.empty());

  RequestTypeForUma type = PermissionUtil::GetUmaValueForRequests(requests);

  base::UmaHistogramEnumeration(
      base::StrCat({"Permissions.Prompt.", GetPermissionRequestString(type),
                    ".ElementAnchoredBubble.Variant"}),
      variant);
}

// static
void PermissionUmaUtil::RecordCrossOriginFrameActionAndPolicyConfiguration(
    ContentSettingsType content_settings_type,
    PermissionAction action,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(IsCrossOriginSubframe(render_frame_host));

  const auto histogram =
      base::StrCat({kPermissionsActionPrefix,
                    PermissionUtil::GetPermissionString(content_settings_type),
                    ".CrossOriginFrame"});
  base::UmaHistogramEnumeration(histogram, action, PermissionAction::NUM);
  RecordTopLevelPermissionsHeaderPolicy(
      content_settings_type, base::StrCat({histogram, ".TopLevelHeaderPolicy"}),
      render_frame_host);
}

// static
void PermissionUmaUtil::RecordTopLevelPermissionsHeaderPolicyOnNavigation(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  const ContentSettingsType kContentSettingsTypesForMetrics[] = {
      base::FeatureList::IsEnabled(
          content_settings::features::kApproximateGeolocationPermission)
          ? ContentSettingsType::GEOLOCATION_WITH_OPTIONS
          : ContentSettingsType::GEOLOCATION,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::MEDIASTREAM_MIC};

  for (const auto content_settings_type : kContentSettingsTypesForMetrics) {
    const auto feature =
        PermissionUtil::GetPermissionsPolicyFeature(content_settings_type);
    DCHECK(feature.has_value());
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Permissions.Experimental.PrimaryMainNavigationFinished.",
             PermissionUtil::GetPermissionString(content_settings_type),
             ".TopLevelHeaderPolicy"}),
        GetTopLevelPermissionHeaderPolicyForUMA(render_frame_host,
                                                feature.value()),
        PermissionHeaderPolicyForUMA::NUM);
  }
}

void PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
    const GURL& origin,
    ContentSettingsType content_settings_type,
    PermissionSourceUI source_ui,
    content::BrowserContext* browser_context,
    base::Time current_time) {
  auto* hcsm = PermissionsClient::Get()->GetSettingsMap(browser_context);
  std::optional<uint32_t> days_since_revocation =
      GetDaysSinceUnusedSitePermissionRevocation(origin, content_settings_type,
                                                 current_time, hcsm);
  if (!days_since_revocation.has_value()) {
    return;
  }
  std::string source_ui_string;
  // We are only interested in permission updates through the UI that go from
  // Ask to Allow. This can only be done through the permission prompt and the
  // site settings page.
  switch (source_ui) {
    case PermissionSourceUI::PROMPT:
      source_ui_string = "Prompt";
      break;
    case PermissionSourceUI::SITE_SETTINGS:
      source_ui_string = "Settings";
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Settings.SafetyCheck.UnusedSitePermissionsRegrantDays",
           source_ui_string, ".",
           PermissionUtil::GetPermissionString(content_settings_type)}),
      days_since_revocation.value(), 31);
  base::UmaHistogramExactLinear(
      base::StrCat({"Settings.SafetyCheck.UnusedSitePermissionsRegrantDays",
                    source_ui_string, ".All"}),
      days_since_revocation.value(), 31);
}

// static
std::optional<uint32_t>
PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
    const GURL& origin,
    ContentSettingsType content_settings_type,
    base::Time current_time,
    HostContentSettingsMap* hcsm) {
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm->GetWebsiteSetting(
      origin, origin, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      &info));
  if (!stored_value.is_dict()) {
    return std::nullopt;
  }
  base::Value::List* permission_type_list =
      stored_value.GetDict().FindList(permissions::kRevokedKey);
  if (!permission_type_list) {
    return std::nullopt;
  }
  base::Time revoked_time =
      info.metadata.expiration() -
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  uint32_t days_since_revoked = (current_time - revoked_time).InDays();

  for (auto& permission_type : *permission_type_list) {
    auto type_int = permission_type.GetIfInt();
    if (!type_int.has_value()) {
      continue;
    }
    if (content_settings_type ==
        static_cast<ContentSettingsType>(type_int.value())) {
      return days_since_revoked;
    }
  }

  return std::nullopt;
}

// static
void PermissionUmaUtil::RecordElementAnchoredPermissionPromptAction(
    const std::vector<std::unique_ptr<PermissionRequest>>& requests,
    const std::vector<base::WeakPtr<permissions::PermissionRequest>>&
        screen_requests,
    ElementAnchoredBubbleAction action,
    ElementAnchoredBubbleVariant variant,
    int screen_counter,
    const GURL& requesting_origin,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  CHECK(requests.size());
  CHECK(screen_requests.size());
  auto first_request_type =
      RequestTypeToContentSettingsType(requests[0]->request_type());
  PermissionsClient::Get()->GetUkmSourceId(
      first_request_type.value(), browser_context, web_contents,
      requesting_origin,
      base::BindOnce(&RecordElementAnchoredPermissionPromptActionUkm,
                     PermissionUtil::GetUmaValueForRequests(requests),
                     PermissionUtil::GetUmaValueForRequests(screen_requests),
                     action, variant, screen_counter));
}

// static
void PermissionUmaUtil::RecordPermissionIndicatorElapsedTimeSinceLastUsage(
    RequestTypeForUma request_type,
    base::TimeDelta time_delta) {
  base::UmaHistogramLongTimes100(
      "Permissions.Usage.ElapsedTimeSinceLastUsage." +
          GetPermissionRequestString(request_type),
      time_delta);
}

// static
void PermissionUmaUtil::RecordPermissionRequestRelevance(
    permissions::RequestType permission_request_type,
    PermissionRequestRelevance permission_request_relevance,
    PredictionModelType model_type) {
  switch (model_type) {
    case permissions::PredictionModelType::kOnDeviceAiV1Model:
      [[fallthrough]];
    case permissions::PredictionModelType::kOnDeviceAiV3Model:
      [[fallthrough]];
    case permissions::PredictionModelType::kOnDeviceAiV4Model: {
      std::string permission_request_type_string =
          permission_request_type == permissions::RequestType::kNotifications
              ? "Notifications"
              : "Geolocation";
      base::UmaHistogramEnumeration(
          base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                        ".", permission_request_type_string,
                        ".PermissionRequestRelevance"}),
          permission_request_relevance);
      break;
    }
    default:
      NOTREACHED();
  }
}

// static
void PermissionUmaUtil::RecordPromptShownInActiveBrowser(
    RequestTypeForUma request_type,
    bool embedded_permission_element_initiated,
    bool active) {
  std::string histogram_name = base::StrCat(
      {"Permissions.Prompt.", GetPermissionRequestString(request_type),
       embedded_permission_element_initiated
           ? ".ElementAnchoredBubble.ShownInActiveBrowser"
           : ".ShownInActiveBrowser"});
  base::UmaHistogramBoolean(histogram_name, active);
}

// static
void PermissionUmaUtil::RecordBrowserAlwaysActiveWhilePrompting(
    RequestTypeForUma request_type,
    bool embedded_permission_element_initiated,
    bool always_active) {
  std::string histogram_name = base::StrCat(
      {"Permissions.Prompt.", GetPermissionRequestString(request_type),
       embedded_permission_element_initiated
           ? ".ElementAnchoredBubble.BrowserAlwaysActiveWhileShowing"
           : ".BrowserAlwaysActiveWhileShowing"});
  base::UmaHistogramBoolean(histogram_name, always_active);
}

// static
void PermissionUmaUtil::RecordActionBrowserAlwaysActive(
    RequestTypeForUma request_type,
    std::string permission_action,
    bool always_active) {
  std::string histogram_name = base::StrCat(
      {"Permissions.Prompt.", GetPermissionRequestString(request_type), ".",
       permission_action, ".WithBrowser"});
  base::UmaHistogramBoolean(histogram_name, always_active);
}

// static
void PermissionUmaUtil::RecordRenderedTextSize(PredictionModelType model_type,
                                               RequestType request_type,
                                               size_t text_size) {
  base::UmaHistogramCounts10000(
      base::StrCat({"Permissions.", GetPredictionModelString(model_type), ".",
                    GetRequestTypeString(request_type), ".RenderedTextSize"}),
      text_size);
}

// static
void PermissionUmaUtil::RecordPredictionModelInquireTime(
    PredictionModelType model_type,
    base::TimeTicks model_inquire_start_time) {
  std::string histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".InquiryDuration"});
  base::UmaHistogramMediumTimes(
      histogram_name, base::TimeTicks::Now() - model_inquire_start_time);
}

// static
void PermissionUmaUtil::RecordRenderedTextAcquireSuccessForAivX(
    PredictionModelType model_type,
    bool success) {
  // Only AIv1 and AIv4 models use the rendered text as input.
  DCHECK(model_type == PredictionModelType::kOnDeviceAiV1Model ||
         model_type == PredictionModelType::kOnDeviceAiV4Model);

  std::string success_histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".RenderedTextAcquireSuccess"});
  base::UmaHistogramBoolean(success_histogram_name, success);
}

// static
void PermissionUmaUtil::RecordTryCancelPreviousEmbeddingsModelExecution(
    PredictionModelType model_type,
    bool cancel_previous_task) {
  // Only the AIv4 model requires the passage embedding model.
  DCHECK_EQ(model_type, PredictionModelType::kOnDeviceAiV4Model);

  std::string success_histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".TryCancelPreviousEmbeddingsModelExecution"});
  base::UmaHistogramBoolean(success_histogram_name, cancel_previous_task);
}

// static
void PermissionUmaUtil::RecordFinishedPassageEmbeddingsTaskOutdated(
    PredictionModelType model_type,
    bool outdated) {
  // Only the AIv4 model requires the passage embedding model.
  DCHECK_EQ(model_type, PredictionModelType::kOnDeviceAiV4Model);

  std::string success_histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".FinishedPassageEmbeddingsTaskOutdated"});
  base::UmaHistogramBoolean(success_histogram_name, outdated);
}

// static
void PermissionUmaUtil::RecordPassageEmbeddingModelExecutionTimeAndStatus(
    PredictionModelType model_type,
    base::TimeTicks model_inquire_start_time,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  // Only the AIv4 model requires the passage embedding model.
  DCHECK_EQ(model_type, PredictionModelType::kOnDeviceAiV4Model);

  std::string status_histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".ComputeEmbeddingsStatus"});
  base::UmaHistogramEnumeration(status_histogram_name, status);

  std::string time_histogram_name =
      base::StrCat({"Permissions.", GetPredictionModelString(model_type),
                    ".ComputeEmbeddingsDuration"});
  base::UmaHistogramMediumTimes(
      time_histogram_name, base::TimeTicks::Now() - model_inquire_start_time);
}

// static
void PermissionUmaUtil::RecordSnapshotTakenTimeAndSuccessForAivX(
    PredictionModelType model_type,
    base::TimeTicks snapshot_inquire_start_time,
    bool success) {
  // Only AIv3 and AIv4 models use snapshots as input.
  DCHECK(model_type == PredictionModelType::kOnDeviceAiV3Model ||
         model_type == PredictionModelType::kOnDeviceAiV4Model);

  std::string success_histogram_name = base::StrCat(
      {"Permissions.", GetPredictionModelString(model_type), ".SnapshotTaken"});
  base::UmaHistogramBoolean(success_histogram_name, success);

  std::string duration_histogram_name =
      base::StrCat({success_histogram_name, "Duration"});
  base::UmaHistogramMediumTimes(
      duration_histogram_name,
      base::TimeTicks::Now() - snapshot_inquire_start_time);
}

// static
void PermissionUmaUtil::RecordLanguageDetectionStatus(
    LanguageDetectionStatus status) {
  base::UmaHistogramEnumeration("Permissions.AIv4.LanguageDetectionStatus",
                                status);
}

// static
void PermissionUmaUtil::RecordPassageEmbeddingsCalculationTimeout(
    bool timeout) {
  base::UmaHistogramBoolean(
      "Permissions.AIv4.PassageEmbeddingsComputationTimeout", timeout);
}

// static
void PermissionUmaUtil::RecordPassageEmbedderMetadataValid(bool valid) {
  base::UmaHistogramBoolean("Permissions.AIv4.EmbedderMetadataValid", valid);
}

// static
void PermissionUmaUtil::RecordPredictionServiceTimeout(bool timeout) {
  base::UmaHistogramBoolean("Permissions.PredictionService.Timeout", timeout);
}

// static
void PermissionUmaUtil::RecordPermissionAutoRejectForActor(
    ContentSettingsType permission,
    bool is_actor_operating) {
  base::UmaHistogramBoolean(
      base::StrCat({"Permissions.Experimental.Usage.",
                    PermissionUtil::GetPermissionString(permission),
                    ".IsBlockedDueToActuation"}),
      is_actor_operating);
}

// static
void PermissionUmaUtil::RecordPrePromptSessionDuration(
    ContentSettingsType permission,
    base::TimeTicks request_first_display_time) {
  if (request_first_display_time.is_null()) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - request_first_display_time;
  std::string permission_string =
      PermissionUtil::GetPermissionString(permission);

  // Record finer-grained histograms for the first minute.
  if (duration <= base::Minutes(1)) {
    //  1 second granularity.
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PrePromptSessionDuration1m"}),
        duration, base::Milliseconds(0), base::Minutes(1), 60);
  } else if (duration <= base::Minutes(5)) {
    // 2 seconds granularity.
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PrePromptSessionDuration5m"}),
        duration, base::Minutes(1), base::Minutes(5), 120);
  } else if (duration <= base::Hours(1)) {
    // 15 seconds granularity.
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PrePromptSessionDuration1h"}),
        duration, base::Minutes(5), base::Hours(1), 220);
  }
}

// static
void PermissionUmaUtil::RecordPostPromptSessionDuration(
    ContentSettingsType permission,
    base::TimeTicks request_first_display_time) {
  if (request_first_display_time.is_null()) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - request_first_display_time;
  std::string permission_string =
      PermissionUtil::GetPermissionString(permission);

  // Record the original histogram for up to 1 hour.
  base::UmaHistogramLongTimes100(
      base::StrCat({"Permissions.PredictionService.", permission_string,
                    ".PostPromptSessionDuration"}),
      duration);

      // UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 100);

  // Record finer-grained histograms for the first minute.
  if (duration <= base::Seconds(10)) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PostPromptSessionDuration10s"}),
        duration, base::Milliseconds(1), base::Milliseconds(10),
        /*buckets=*/10);
  } else if (duration <= base::Minutes(1)) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PostPromptSessionDuration1m"}),
        duration, base::Milliseconds(11), base::Minutes(1), /*buckets=*/25);
  } else if (duration <= base::Minutes(5)) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PostPromptSessionDuration5m"}),
        duration, base::Minutes(1), base::Minutes(5), /*buckets=*/15);
  } else if (duration <= base::Minutes(10)) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PostPromptSessionDuration10m"}),
        duration, base::Minutes(5), base::Minutes(10), /*buckets=*/10);
  } else if (duration <= base::Minutes(30)) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Permissions.PredictionService.", permission_string,
                      ".PostPromptSessionDuration30m"}),
        duration, base::Minutes(10), base::Minutes(30), /*buckets=*/20);
  }
}

}  // namespace permissions
