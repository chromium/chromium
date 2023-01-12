// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#endif

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

RequestTypeForUma GetUmaValueForRequestType(RequestType request_type) {
  switch (request_type) {
    case RequestType::kAccessibilityEvents:
      return RequestTypeForUma::PERMISSION_ACCESSIBILITY_EVENTS;
    case RequestType::kArSession:
      return RequestTypeForUma::PERMISSION_AR;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kCameraPanTiltZoom:
      return RequestTypeForUma::PERMISSION_CAMERA_PAN_TILT_ZOOM;
#endif
    case RequestType::kCameraStream:
      return RequestTypeForUma::PERMISSION_MEDIASTREAM_CAMERA;
    case RequestType::kClipboard:
      return RequestTypeForUma::PERMISSION_CLIPBOARD_READ_WRITE;
    case RequestType::kDiskQuota:
      return RequestTypeForUma::QUOTA;
#if !BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/1296792): Enable on Android
    case RequestType::kLocalFonts:
      return RequestTypeForUma::PERMISSION_LOCAL_FONTS;
#endif
    case RequestType::kGeolocation:
      return RequestTypeForUma::PERMISSION_GEOLOCATION;
    case RequestType::kIdleDetection:
      return RequestTypeForUma::PERMISSION_IDLE_DETECTION;
    case RequestType::kMicStream:
      return RequestTypeForUma::PERMISSION_MEDIASTREAM_MIC;
    case RequestType::kMidiSysex:
      return RequestTypeForUma::PERMISSION_MIDI_SYSEX;
    case RequestType::kMultipleDownloads:
      return RequestTypeForUma::DOWNLOAD;
#if BUILDFLAG(IS_ANDROID)
    case RequestType::kNfcDevice:
      return RequestTypeForUma::PERMISSION_NFC;
#endif
    case RequestType::kNotifications:
      return RequestTypeForUma::PERMISSION_NOTIFICATIONS;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      return RequestTypeForUma::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
#endif
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kRegisterProtocolHandler:
      return RequestTypeForUma::REGISTER_PROTOCOL_HANDLER;
    case RequestType::kSecurityAttestation:
      return RequestTypeForUma::PERMISSION_SECURITY_KEY_ATTESTATION;
    case RequestType::kU2fApiRequest:
      return RequestTypeForUma::PERMISSION_U2F_API_REQUEST;
#endif
    case RequestType::kStorageAccess:
      return RequestTypeForUma::PERMISSION_STORAGE_ACCESS;
    case RequestType::kVrSession:
      return RequestTypeForUma::PERMISSION_VR;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kWindowManagement:
      return RequestTypeForUma::PERMISSION_WINDOW_MANAGEMENT;
#endif
    case RequestType::kTopLevelStorageAccess:
      return RequestTypeForUma::PERMISSION_TOP_LEVEL_STORAGE_ACCESS;
  }
}

const int kPriorCountCap = 10;

std::string GetPermissionRequestString(RequestTypeForUma type) {
  switch (type) {
    case RequestTypeForUma::MULTIPLE:
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
    case RequestTypeForUma::PERMISSION_SECURITY_KEY_ATTESTATION:
      return "SecurityKeyAttestation";
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
    case RequestTypeForUma::PERMISSION_U2F_API_REQUEST:
      return "U2fApiRequest";
    default:
      NOTREACHED();
      return "";
  }
}

void RecordEngagementMetric(const std::vector<PermissionRequest*>& requests,
                            content::WebContents* web_contents,
                            const std::string& action) {
  RequestTypeForUma type =
      GetUmaValueForRequestType(requests[0]->request_type());
  if (requests.size() > 1)
    type = RequestTypeForUma::MULTIPLE;

  DCHECK(action == "Accepted" || action == "Denied" || action == "Dismissed" ||
         action == "Ignored" || action == "AcceptedOnce");
  std::string name = "Permissions.Engagement." + action + '.' +
                     GetPermissionRequestString(type);

  double engagement_score = PermissionsClient::Get()->GetSiteEngagementScore(
      web_contents->GetBrowserContext(), requests[0]->requesting_origin());
  base::UmaHistogramPercentageObsoleteDoNotUse(name, engagement_score);
}

void RecordPermissionUsageUkm(ContentSettingsType permission_type,
                              absl::optional<ukm::SourceId> source_id) {
  if (!source_id.has_value())
    return;

  size_t num_values = 0;

  ukm::builders::PermissionUsage builder(source_id.value());
  builder.SetPermissionType(static_cast<int64_t>(
      ContentSettingTypeToHistogramValue(permission_type, &num_values)));

  builder.Record(ukm::UkmRecorder::Get());
}

void RecordPermissionActionUkm(
    PermissionAction action,
    PermissionRequestGestureType gesture_type,
    ContentSettingsType permission,
    int dismiss_count,
    int ignore_count,
    PermissionSourceUI source_ui,
    base::TimeDelta time_to_decision,
    PermissionPromptDisposition ui_disposition,
    absl::optional<PermissionPromptDispositionReason> ui_reason,
    absl::optional<bool> has_three_consecutive_denies,
    absl::optional<bool> has_previously_revoked_permission,
    absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
        predicted_grant_likelihood,
    PredictionRequestFeatures::ActionCounts
        loud_ui_actions_counts_for_request_type,
    PredictionRequestFeatures::ActionCounts loud_ui_actions_counts,
    PredictionRequestFeatures::ActionCounts actions_counts_for_request_type,
    PredictionRequestFeatures::ActionCounts actions_counts,
    absl::optional<bool> prediction_decision_held_back,
    absl::optional<ukm::SourceId> source_id) {
  // Only record the permission change if the origin is in the history.
  if (!source_id.has_value())
    return;

  size_t num_values = 0;

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
          ContentSettingTypeToHistogramValue(permission, &num_values)))
      .SetPriorDismissals(std::min(kPriorCountCap, dismiss_count))
      .SetPriorIgnores(std::min(kPriorCountCap, ignore_count))
      .SetSource(static_cast<int64_t>(source_ui))
      .SetPromptDisposition(static_cast<int64_t>(ui_disposition));

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

  if (ui_reason.has_value())
    builder.SetPromptDispositionReason(static_cast<int64_t>(ui_reason.value()));

  if (predicted_grant_likelihood.has_value()) {
    builder.SetPredictionsApiResponse_GrantLikelihood(
        static_cast<int64_t>(predicted_grant_likelihood.value()));
  }

  if (prediction_decision_held_back.has_value()) {
    builder.SetPredictionsApiResponse_Heldback(
        prediction_decision_held_back.value());
  }

  if (has_three_consecutive_denies.has_value()) {
    int64_t satisfied_adaptive_triggers = 0;
    if (has_three_consecutive_denies.value())
      satisfied_adaptive_triggers |=
          static_cast<int64_t>(AdaptiveTriggers::THREE_CONSECUTIVE_DENIES);
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

  if (!time_to_decision.is_zero()) {
    builder.SetTimeToDecision(ukm::GetExponentialBucketMinForUserTiming(
        time_to_decision.InMilliseconds()));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

// |full_version| represented in the format `YYYY.M.D.m`, where m is the
// minute-of-day. Return int represented in the format `YYYYMMDD`.
// CrowdDeny versions published before 2020 will be reported as 1.
// Returns 0 if no version available.
// Returns 1 if a version has invalid format.
int ConvertCrowdDenyVersionToInt(const absl::optional<base::Version>& version) {
  if (!version.has_value() || !version.value().IsValid())
    return 0;

  const std::vector<uint32_t>& full_version = version.value().components();
  if (full_version.size() != 4)
    return 1;

  const int kCrowdDenyMinYearLimit = 2020;
  const int year = base::checked_cast<int>(full_version.at(0));
  if (year < kCrowdDenyMinYearLimit)
    return 1;

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

// Make sure you update histograms.xml permission histogram_suffix if you
// add new permission
void PermissionUmaUtil::PermissionRequested(ContentSettingsType content_type) {
  PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  base::UmaHistogramEnumeration("ContentSettings.PermissionRequested",
                                permission, PermissionType::NUM);
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
  RecordPermissionAction(permission, PermissionAction::REVOKED, source_ui,
                         PermissionRequestGestureType::UNKNOWN,
                         /*time_to_decision=*/base::TimeDelta(),
                         PermissionPromptDisposition::NOT_APPLICABLE,
                         /*ui_reason=*/absl::nullopt, revoked_origin,
                         /*web_contents=*/nullptr, browser_context,
                         /*predicted_grant_likelihood=*/absl::nullopt,
                         /*prediction_decision_held_back=*/absl::nullopt);
}

void PermissionUmaUtil::RecordEmbargoPromptSuppression(
    PermissionEmbargoStatus embargo_status) {
  base::UmaHistogramEnumeration(
      "Permissions.AutoBlocker.EmbargoPromptSuppression", embargo_status,
      PermissionEmbargoStatus::NUM);
}

void PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(
    PermissionStatusSource source) {
  // Explicitly switch to ensure that any new
  // PermissionStatusSource values are dealt with appropriately.
  switch (source) {
    case PermissionStatusSource::MULTIPLE_DISMISSALS:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_DISMISSALS);
      break;
    case PermissionStatusSource::MULTIPLE_IGNORES:
      PermissionUmaUtil::RecordEmbargoPromptSuppression(
          PermissionEmbargoStatus::REPEATED_IGNORES);
      break;
    case PermissionStatusSource::UNSPECIFIED:
    case PermissionStatusSource::KILL_SWITCH:
    case PermissionStatusSource::INSECURE_ORIGIN:
    case PermissionStatusSource::FEATURE_POLICY:
    case PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
    case PermissionStatusSource::PORTAL:
    case PermissionStatusSource::FENCED_FRAME:
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

void PermissionUmaUtil::RecordPermissionPromptAttempt(
    const std::vector<PermissionRequest*>& requests,
    bool IsLocationBarEditingOrEmpty) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type = RequestTypeForUma::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = GetUmaValueForRequestType(requests[0]->request_type());
    gesture_type = requests[0]->GetGestureType();
  }

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

  std::string histogram_name =
      "Permissions.Prompt." + permission_type + "." + gesture + ".Attempt";

  base::UmaHistogramBoolean(histogram_name, IsLocationBarEditingOrEmpty);
}

void PermissionUmaUtil::PermissionPromptShown(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type = RequestTypeForUma::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = GetUmaValueForRequestType(requests[0]->request_type());
    gesture_type = requests[0]->GetGestureType();
  }

  PERMISSION_BUBBLE_TYPE_UMA(kPermissionsPromptShown, request_type);
  PERMISSION_BUBBLE_GESTURE_TYPE_UMA(kPermissionsPromptShownGesture,
                                     kPermissionsPromptShownNoGesture,
                                     gesture_type, request_type);
}

void PermissionUmaUtil::PermissionPromptResolved(
    const std::vector<PermissionRequest*>& requests,
    content::WebContents* web_contents,
    PermissionAction permission_action,
    base::TimeDelta time_to_decision,
    PermissionPromptDisposition ui_disposition,
    absl::optional<PermissionPromptDispositionReason> ui_reason,
    absl::optional<PredictionGrantLikelihood> predicted_grant_likelihood,
    absl::optional<bool> prediction_decision_held_back,
    absl::optional<permissions::PermissionIgnoredReason> ignored_reason,
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
      break;
  }
  std::string action_string = GetPermissionActionString(permission_action);
  RecordEngagementMetric(requests, web_contents, action_string);

  PermissionDecisionAutoBlocker* autoblocker =
      PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          web_contents->GetBrowserContext());

  for (PermissionRequest* request : requests) {
    ContentSettingsType permission = request->GetContentSettingsType();
    // TODO(timloh): We only record these metrics for permissions which have a
    // ContentSettingsType, as otherwise they don't support GetGestureType.
    if (permission == ContentSettingsType::DEFAULT)
      continue;

    PermissionRequestGestureType gesture_type = request->GetGestureType();
    const GURL& requesting_origin = request->requesting_origin();

    RecordPermissionAction(
        permission, permission_action, PermissionSourceUI::PROMPT, gesture_type,
        time_to_decision, ui_disposition, ui_reason, requesting_origin,
        web_contents, web_contents->GetBrowserContext(),
        predicted_grant_likelihood, prediction_decision_held_back);

    std::string priorDismissPrefix =
        "Permissions.Prompt." + action_string + ".PriorDismissCount2.";
    std::string priorIgnorePrefix =
        "Permissions.Prompt." + action_string + ".PriorIgnoreCount2.";
    RecordPermissionPromptPriorCount(
        permission, priorDismissPrefix,
        autoblocker->GetDismissCount(requesting_origin, permission));
    RecordPermissionPromptPriorCount(
        permission, priorIgnorePrefix,
        autoblocker->GetIgnoreCount(requesting_origin, permission));
  }

  base::UmaHistogramEnumeration("Permissions.Action.WithDisposition." +
                                    GetPromptDispositionString(ui_disposition),
                                permission_action, PermissionAction::NUM);

  RequestTypeForUma type =
      GetUmaValueForRequestType(requests[0]->request_type());
  if (requests.size() > 1)
    type = RequestTypeForUma::MULTIPLE;

  std::string permission_type = GetPermissionRequestString(type);
  std::string permission_disposition =
      GetPromptDispositionString(ui_disposition);

  base::UmaHistogramEnumeration("Permissions.Prompt." + permission_type + "." +
                                    permission_disposition + ".Action",
                                permission_action, PermissionAction::NUM);

  if (!time_to_decision.is_zero()) {
    base::UmaHistogramLongTimes("Permissions.Prompt." + permission_type + "." +
                                    permission_disposition + "." +
                                    action_string + ".TimeToAction",
                                time_to_decision);
  }

  if (permission_action == PermissionAction::IGNORED &&
      ui_disposition !=
          PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE &&
      ui_disposition != PermissionPromptDisposition::ANCHORED_BUBBLE) {
    base::UmaHistogramBoolean("Permissions.Prompt." + permission_type + "." +
                                  permission_disposition +
                                  ".Ignored.DidShowBubble",
                              did_show_prompt);
  }

  if (requests[0]->request_type() == RequestType::kGeolocation ||
      requests[0]->request_type() == RequestType::kNotifications) {
    if (ui_disposition ==
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP ||
        ui_disposition == PermissionPromptDisposition::MESSAGE_UI ||
        ui_disposition == PermissionPromptDisposition::MINI_INFOBAR) {
      base::UmaHistogramBoolean("Permissions.Prompt." + permission_type + "." +
                                    permission_disposition + "." +
                                    action_string + ".DidClickManage",
                                did_click_managed);
    } else if (ui_disposition == PermissionPromptDisposition::
                                     LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP ||
               ui_disposition == PermissionPromptDisposition::MESSAGE_UI ||
               ui_disposition == PermissionPromptDisposition::MINI_INFOBAR) {
      base::UmaHistogramBoolean("Permissions.Prompt." + permission_type + "." +
                                    permission_disposition + "." +
                                    action_string + ".DidClickLearnMore",
                                did_click_learn_more);
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

void PermissionUmaUtil::RecordInfobarDetailsExpanded(bool expanded) {
  base::UmaHistogramBoolean("Permissions.Prompt.Infobar.DetailsExpanded",
                            expanded);
}

void PermissionUmaUtil::RecordCrowdDenyDelayedPushNotification(
    base::TimeDelta delay) {
  base::UmaHistogramTimes(
      "Permissions.CrowdDeny.PreloadData.DelayedPushNotification", delay);
}

void PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
    const absl::optional<base::Version>& version) {
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
      (!secondary_url_.is_valid() && !secondary_url_.is_empty())) {
    is_initially_allowed_ = false;
    return;
  }
  HostContentSettingsMap* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  ContentSetting initial_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_);
  is_initially_allowed_ = initial_content_setting == CONTENT_SETTING_ALLOW;
  content_settings::SettingInfo setting_info;
  settings_map->GetWebsiteSetting(primary_url, secondary_url, content_type_,
                                  &setting_info);
  last_modified_date_ = setting_info.metadata.last_modified;
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
  if (!is_initially_allowed_)
    return;
  if (!IsRequestablePermissionType(content_type_))
    return;
  HostContentSettingsMap* settings_map =
      PermissionsClient::Get()->GetSettingsMap(browser_context_);
  ContentSetting final_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_);
  if (final_content_setting != CONTENT_SETTING_ALLOW) {
    // PermissionUmaUtil takes origins, even though they're typed as GURL.
    GURL requesting_origin = primary_url_.DeprecatedGetOriginAsURL();
    PermissionRevoked(content_type_, source_ui_, requesting_origin,
                      browser_context_);
    if ((content_type_ == ContentSettingsType::GEOLOCATION ||
         content_type_ == ContentSettingsType::MEDIASTREAM_CAMERA ||
         content_type_ == ContentSettingsType::MEDIASTREAM_MIC) &&
        !last_modified_date_.is_null()) {
      RecordTimeElapsedBetweenGrantAndRevoke(
          content_type_, base::Time::Now() - last_modified_date_);
    }
  }
}

void PermissionUmaUtil::RecordPermissionUsage(
    ContentSettingsType permission_type,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& requesting_origin) {
  PermissionsClient::Get()->GetUkmSourceId(
      browser_context, web_contents, requesting_origin,
      base::BindOnce(&RecordPermissionUsageUkm, permission_type));
}

void PermissionUmaUtil::RecordPermissionAction(
    ContentSettingsType permission,
    PermissionAction action,
    PermissionSourceUI source_ui,
    PermissionRequestGestureType gesture_type,
    base::TimeDelta time_to_decision,
    PermissionPromptDisposition ui_disposition,
    absl::optional<PermissionPromptDispositionReason> ui_reason,
    const GURL& requesting_origin,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    absl::optional<PredictionGrantLikelihood> predicted_grant_likelihood,
    absl::optional<bool> prediction_decision_held_back) {
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

  PermissionsClient::Get()->GetUkmSourceId(
      browser_context, web_contents, requesting_origin,
      base::BindOnce(
          &RecordPermissionActionUkm, action, gesture_type, permission,
          dismiss_count, ignore_count, source_ui, time_to_decision,
          ui_disposition, ui_reason,
          permission == ContentSettingsType::NOTIFICATIONS
              ? PermissionsClient::Get()
                    ->HadThreeConsecutiveNotificationPermissionDenies(
                        browser_context)
              : absl::nullopt,
          PermissionsClient::Get()->HasPreviouslyAutoRevokedPermission(
              browser_context, requesting_origin, permission),
          predicted_grant_likelihood, loud_ui_actions_counts_per_request_type,
          loud_ui_actions_counts, actions_counts_per_request_type,
          actions_counts, prediction_decision_held_back));

  switch (permission) {
    case ContentSettingsType::GEOLOCATION:
      base::UmaHistogramEnumeration("Permissions.Action.Geolocation", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::NOTIFICATIONS:
      base::UmaHistogramEnumeration("Permissions.Action.Notifications", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::MIDI_SYSEX:
      base::UmaHistogramEnumeration("Permissions.Action.MidiSysEx", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      base::UmaHistogramEnumeration("Permissions.Action.ProtectedMedia", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      base::UmaHistogramEnumeration("Permissions.Action.AudioCapture", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      base::UmaHistogramEnumeration("Permissions.Action.VideoCapture", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      base::UmaHistogramEnumeration("Permissions.Action.ClipboardReadWrite",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::PAYMENT_HANDLER:
      base::UmaHistogramEnumeration("Permissions.Action.PaymentHandler", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::NFC:
      base::UmaHistogramEnumeration("Permissions.Action.Nfc", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::VR:
      base::UmaHistogramEnumeration("Permissions.Action.VR", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::AR:
      base::UmaHistogramEnumeration("Permissions.Action.AR", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      base::UmaHistogramEnumeration("Permissions.Action.StorageAccess", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      base::UmaHistogramEnumeration("Permissions.Action.TopLevelStorageAccess",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      base::UmaHistogramEnumeration("Permissions.Action.CameraPanTiltZoom",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      base::UmaHistogramEnumeration("Permissions.Action.WindowPlacement",
                                    action, PermissionAction::NUM);
      break;
    case ContentSettingsType::LOCAL_FONTS:
      base::UmaHistogramEnumeration("Permissions.Action.LocalFonts", action,
                                    PermissionAction::NUM);
      break;
    case ContentSettingsType::IDLE_DETECTION:
      base::UmaHistogramEnumeration("Permissions.Action.IdleDetection", action,
                                    PermissionAction::NUM);
      break;
    // The user is not prompted for these permissions, thus there is no
    // permission action recorded for them.
    default:
      NOTREACHED() << "PERMISSION "
                   << PermissionUtil::GetPermissionString(permission)
                   << " not accounted for";
  }
}

// static
void PermissionUmaUtil::RecordPromptDecided(
    const std::vector<PermissionRequest*>& requests,
    bool accepted,
    bool is_one_time) {
  DCHECK(!requests.empty());

  RequestTypeForUma request_type = RequestTypeForUma::MULTIPLE;
  PermissionRequestGestureType gesture_type =
      PermissionRequestGestureType::UNKNOWN;
  if (requests.size() == 1) {
    request_type = GetUmaValueForRequestType(requests[0]->request_type());
    gesture_type = requests[0]->GetGestureType();
  }

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
    base::TimeDelta delta) {
  base::UmaHistogramCustomCounts("Permissions.Usage.ElapsedTimeSinceGrant." +
                                     PermissionUtil::GetPermissionString(type),
                                 delta.InSeconds(), 1,
                                 base::Days(365).InSeconds(), 100);
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
      GetPermissionRequestString(GetUmaValueForRequestType(
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
      GetPermissionRequestString(GetUmaValueForRequestType(
          ContentSettingsTypeToRequestType(permission_type)));
  base::UmaHistogramEnumeration(
      "Permissions.DSE.EffectiveSetting." + permission_string, setting,
      CONTENT_SETTING_NUM_SETTINGS);
}

// static
void PermissionUmaUtil::RecordPermissionPredictionSource(
    PermissionPredictionSource prediction_source) {
  base::UmaHistogramEnumeration(
      "Permissions.PredictionService.PredictionSource", prediction_source);
}

// static
void PermissionUmaUtil::RecordPermissionPredictionServiceHoldback(
    RequestType request_type,
    bool is_on_device,
    bool is_heldback) {
  if (is_on_device) {
    base::UmaHistogramBoolean(
        "Permissions.OnDevicePredictionService.Response." +
            GetPermissionRequestString(GetUmaValueForRequestType(request_type)),
        is_heldback);
  } else {
    base::UmaHistogramBoolean(
        "Permissions.PredictionService.Response." +
            GetPermissionRequestString(GetUmaValueForRequestType(request_type)),
        is_heldback);
  }
}

// static
void PermissionUmaUtil::RecordPageInfoDialogAccessType(
    PageInfoDialogAccessType access_type) {
  base::UmaHistogramEnumeration(
      "Permissions.ConfirmationChip.PageInfoDialogAccessType", access_type);
}

// static
void PermissionUmaUtil::RecordPageInfoPermissionChangeWithin1m(
    ContentSettingsType type,
    PermissionAction previous_action,
    ContentSetting setting_after) {
  DCHECK(IsRequestablePermissionType(type));
  std::string permission_type = GetPermissionRequestString(
      GetUmaValueForRequestType(ContentSettingsTypeToRequestType(type)));
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
  NOTREACHED();
  return std::string();
}

// static
std::string PermissionUmaUtil::GetPromptDispositionString(
    PermissionPromptDisposition ui_disposition) {
  switch (ui_disposition) {
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
      return "AnchoredBubble";
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
      return "CustomModalDialog";
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP:
      return "LocationBarLeftChip";
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
  }

  NOTREACHED();
  return std::string();
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
  return std::string();
}

// static
std::string PermissionUmaUtil::GetRequestTypeString(RequestType request_type) {
  return GetPermissionRequestString(GetUmaValueForRequestType(request_type));
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
    case PermissionPromptDisposition::MODAL_DIALOG:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE:
    case PermissionPromptDisposition::NONE_VISIBLE:
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
    case PermissionPromptDisposition::NOT_APPLICABLE:
      return false;
  }
}

// static
bool PermissionUmaUtil::IsPromptDispositionLoud(
    PermissionPromptDisposition prompt_disposition) {
  switch (prompt_disposition) {
    case PermissionPromptDisposition::ANCHORED_BUBBLE:
    case PermissionPromptDisposition::MODAL_DIALOG:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE:
      return true;
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP:
    case PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP:
    case PermissionPromptDisposition::MINI_INFOBAR:
    case PermissionPromptDisposition::MESSAGE_UI:
    case PermissionPromptDisposition::NONE_VISIBLE:
    case PermissionPromptDisposition::CUSTOM_MODAL_DIALOG:
    case PermissionPromptDisposition::NOT_APPLICABLE:
      return false;
  }
}

// static
void PermissionUmaUtil::RecordIgnoreReason(
    const std::vector<PermissionRequest*>& requests,
    PermissionPromptDisposition prompt_disposition,
    PermissionIgnoredReason reason) {
  RequestTypeForUma request_type = RequestTypeForUma::MULTIPLE;
  if (requests.size() == 1) {
    request_type = GetUmaValueForRequestType(requests[0]->request_type());
  }

  std::string histogram_name =
      "Permissions.Prompt." + GetPermissionRequestString(request_type) + "." +
      GetPromptDispositionString(prompt_disposition) + ".IgnoredReason";
  base::UmaHistogramEnumeration(histogram_name, reason,
                                PermissionIgnoredReason::NUM);
}

}  // namespace permissions
