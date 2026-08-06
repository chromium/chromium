// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_constants.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "content/public/browser/permission_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace permissions {
enum class PermissionRequestGestureType;
enum class PermissionAction;
class PermissionRequest;


// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static const char kPermissionsPromptShown[];
  static const char kPermissionsPromptShownGesture[];
  static const char kPermissionsPromptShownNoGesture[];
  static const char kPermissionsPromptAccepted[];
  static const char kPermissionsPromptAcceptedGesture[];
  static const char kPermissionsPromptAcceptedNoGesture[];
  static const char kPermissionsPromptAcceptedOnce[];
  static const char kPermissionsPromptAcceptedOnceGesture[];
  static const char kPermissionsPromptAcceptedOnceNoGesture[];
  static const char kPermissionsPromptDenied[];
  static const char kPermissionsPromptDeniedGesture[];
  static const char kPermissionsPromptDeniedNoGesture[];
  static const char kPermissionsPromptDismissed[];

  static const char kPermissionsExperimentalUsagePrefix[];
  static const char kPermissionsActionPrefix[];

  PermissionUmaUtil() = delete;
  PermissionUmaUtil(const PermissionUmaUtil&) = delete;
  PermissionUmaUtil& operator=(const PermissionUmaUtil&) = delete;

  static void PermissionRequested(ContentSettingsType permission);

  static void RecordActivityIndicator(std::set<ContentSettingsType> permissions,
                                      bool blocked,
                                      bool blocked_system_level,
                                      bool clicked);

  static void RecordDismissalType(
      const std::vector<base::SafeRef<permissions::PermissionRequest>>&
          requests,
      PermissionPromptDisposition ui_disposition,
      DismissalType dismissalType);

  static void RecordPermissionRequestedFromFrame(
      ContentSettingsType content_settings_type,
      content::RenderFrameHost* rfh);

  static void PermissionRequestPreignored(blink::PermissionType permission);

  // Records the revocation UMA and UKM metrics for ContentSettingsTypes that
  // have user facing permission prompts. The passed in `permission` must be
  // such that PermissionUtil::IsPermission(permission) returns true.
  static void PermissionRevoked(ContentSettingsType permission,
                                PermissionSourceUI source_ui,
                                const GURL& revoked_origin,
                                content::BrowserContext* browser_context);

  static void RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus embargo_status);

  static void RecordEmbargoPromptSuppressionFromSource(
      content::PermissionStatusSource source);

  static void RecordEmbargoStatus(PermissionEmbargoStatus embargo_status);

  static void RecordPermissionRecoverySuccessRate(
      ContentSettingsType permission,
      bool is_used,
      bool show_infobar,
      bool page_reload);

  // This gets recorded during the creation process of a prompt, but only for
  // prompts that aren't labeled as abusive or disruptive.
  static void RecordPermissionPromptAttempt(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      bool can_display_prompt);

  // UMA specifically for when permission prompts are shown. This should be
  // roughly equivalent to the metrics above, however it is
  // useful to have separate UMA to a few reasons:
  // - to account for, and get data on coalesced permission bubbles
  // - there are other types of permissions prompts (e.g. download limiting)
  //   which don't go through PermissionContext
  // - the above metrics don't always add up (e.g. sum of
  //   granted+denied+dismissed+ignored is not equal to requested), so it is
  //   unclear from those metrics alone how many prompts are seen by users.
  static void PermissionPromptShown(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests);

  static void PermissionPromptResolved(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      content::BrowserContext* browser_context,
      PermissionAction permission_action,
      const PromptOptions& prompt_options,
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
      bool did_click_manage,
      bool did_click_learn_more,
      std::optional<GeolocationAccuracy>
          initial_geolocation_accuracy_selection);

  static void RecordCrowdDenyDelayedPushNotification(base::TimeDelta delay);

  static void RecordCrowdDenyVersionAtAbuseCheckTime(
      const std::optional<base::Version>& version);

  static void RecordElementAnchoredBubbleDismiss(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      DismissedReason reason);

  static void RecordElementAnchoredBubbleOsMetrics(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      OsScreen screen,
      OsScreenAction action,
      base::TimeDelta time_to_action);

  static void RecordElementAnchoredBubbleVariantUMA(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      ElementAnchoredBubbleVariant variant);

  // Record UMAs related to the Android "Missing permissions" infobar.
  static void RecordMissingPermissionInfobarShouldShow(
      bool should_show,
      const std::vector<ContentSettingsType>& content_settings_types);
  static void RecordMissingPermissionInfobarAction(
      PermissionAction action,
      const std::vector<ContentSettingsType>& content_settings_types);

  static void RecordPermissionUsage(ContentSettingsType permission_type,
                                    content::BrowserContext* browser_context,
                                    content::RenderFrameHost* render_frame_host,
                                    const GURL& requesting_origin);

  static void RecordPermissionUsageNotificationShown(
      bool did_user_always_allow_notifications,
      bool is_allowlisted,
      int suspicious_score,
      content::BrowserContext* browser_context,
      const GURL& requesting_origin,
      uint64_t site_engagement_level);

  static void RecordTimeElapsedBetweenGrantAndUse(
      ContentSettingsType type,
      base::TimeDelta delta,
      content_settings::SettingSource source);

  static void RecordTimeElapsedBetweenGrantAndRevoke(ContentSettingsType type,
                                                     base::TimeDelta delta);

  static void RecordDSEEffectiveSetting(ContentSettingsType permission_type,
                                        PermissionSetting setting);

  static void RecordPermissionPredictionConcurrentRequests(
      RequestType request_type);

  static void RecordPermissionPredictionSource(
      PermissionPredictionSource prediction_source,
      const PermissionRequest& request);

  static void RecordPermissionPredictionServiceHoldback(
      RequestType request_type,
      PredictionModelType model_type,
      bool is_heldback);

  static std::string GetOneTimePermissionEventHistogram(
      ContentSettingsType type);

  static void RecordOneTimePermissionEvent(ContentSettingsType type,
                                           OneTimePermissionEvent event);

  static void RecordPageInfoPermissionChangeWithin1m(
      ContentSettingsType type,
      PermissionAction previous_action,
      ContentSetting setting_after);

  static void RecordPageInfoCameraMicPermissionChange(
      ContentSettingsType type,
      ContentSetting setting_before,
      ContentSetting setting_after,
      bool is_subscribed_to_permission_change_event);

  static void RecordPageInfoPermissionChange(
      ContentSettingsType type,
      PermissionSetting setting_before,
      PermissionSetting setting_after,
      bool is_subscribed_to_permission_change_event);

  static std::string GetPermissionActionString(
      PermissionAction permission_action);

  static std::string GetPredictionModelString(PredictionModelType model_type);

  static std::string GetPromptDispositionString(
      PermissionPromptDisposition ui_disposition);

  static std::string GetPromptDispositionReasonString(
      PermissionPromptDispositionReason ui_disposition_reason);

  static std::string GetRequestTypeString(RequestType request_type);

  static bool IsPromptDispositionQuiet(
      PermissionPromptDisposition prompt_disposition);

  static bool IsPromptDispositionLoud(
      PermissionPromptDisposition prompt_disposition);

  static void RecordIgnoreReason(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      PermissionPromptDisposition prompt_disposition,
      PermissionIgnoredReason reason);

  // Record metrics related to usage of permissions delegation.
  static void RecordPermissionsUsageSourceAndPolicyConfiguration(
      ContentSettingsType content_settings_type,
      content::RenderFrameHost* render_frame_host);

  static void RecordCrossOriginFrameActionAndPolicyConfiguration(
      ContentSettingsType content_settings_type,
      PermissionAction action,
      content::RenderFrameHost* render_frame_host);

  static void RecordTopLevelPermissionsHeaderPolicyOnNavigation(
      content::RenderFrameHost* render_frame_host);

  // Logs a metric that captures how long since revocation, due to a site being
  // considered unused, the user regrants a revoked permission.
  static void RecordPermissionRegrantForUnusedSites(
      const GURL& origin,
      ContentSettingsType request_type,
      PermissionSourceUI source_ui,
      content::BrowserContext* browser_context,
      base::Time current_time);

  static std::optional<uint32_t> GetDaysSinceUnusedSitePermissionRevocation(
      const GURL& origin,
      ContentSettingsType content_settings_type,
      base::Time current_time,
      HostContentSettingsMap* hcsm);

  // Records whether the 'Reload this page' info bar was shown after a quiet
  // permission prompt was granted.
  static void RecordPageReloadInfoBarShown(bool shown);

  // Records whether the page that requested a permission is subscribed to the
  // permission status change listener.
  static void RecordOnPermissionStatusChangedEventSubscribed(RequestType type,
                                                             bool subscribed);

  // Records UKM metrics for ContentSettingsTypes that have user facing
  // permission prompts triggered by the user clicking on the Embedded
  // Permission Element. The passed in `permission` must be such that
  // PermissionUtil::IsPermission(permission) returns true.
  static void RecordElementAnchoredPermissionPromptAction(
      const PermissionRequest& first_request,
      RequestTypeForUma permission,
      RequestTypeForUma screen_permission,
      ElementAnchoredBubbleAction action,
      ElementAnchoredBubbleVariant variant,
      int screen_counter,
      const GURL& requesting_origin,
      content::BrowserContext* browser_context);

  // Records `TimeDelta` between two consecutive indicators of the same
  // `RequestTypeForUma`.
  static void RecordPermissionIndicatorElapsedTimeSinceLastUsage(
      RequestTypeForUma request_type,
      base::TimeDelta time_delta);

  static void RecordPermissionRequestRelevance(
      permissions::RequestType permission_request_type,
      PermissionRequestRelevance permission_request_relevance,
      PredictionModelType model_type);

  // Records if the browser was always active while the prompt was
  // displaying.
  static void RecordBrowserAlwaysActiveWhilePrompting(
      RequestTypeForUma request_type,
      bool embedded_permission_element_initiated,
      bool always_active);

  // Records if the browser was always active before user's interaction.
  static void RecordActionBrowserAlwaysActive(
      RequestTypeForUma request_type,
      std::string_view permission_action,
      bool always_active);

  // Records the execution time of prediction model inquiries.
  static void RecordPredictionModelInquireTime(
      PredictionModelType model_type,
      base::TimeTicks model_inquire_start_time);

  // Records the success and duration of taking a screenshot for AIvX models.
  static void RecordSnapshotTakenTimeAndSuccessForAivX(
      PredictionModelType model_type,
      base::TimeTicks snapshot_inquire_start_time,
      bool success);

  // Records whether we could fetch the rendered text successfully and it was
  // useful for prediction (i.e. longer than 10 characters).
  static void RecordRenderedTextAcquireSuccessForAivX(
      PredictionModelType model_type,
      bool success);

  // Records the size of the rendered text when it was fetched successfully and
  // was suitable as input for model execution.
  static void RecordRenderedTextSize(PredictionModelType model_type,
                                     RequestType request_type,
                                     size_t text_size);

  // Records whether we needed to cancel the previous passage embeddings model
  // call before starting a new one.
  static void RecordTryCancelPreviousEmbeddingsModelExecution(
      PredictionModelType model_type,
      bool cancel_previous_job);

  // Records whether the returning passage embedder job is outdated (a new
  // passage embedder job has started).
  static void RecordFinishedPassageEmbeddingsJobOutdated(
      PredictionModelType model_type,
      bool outdated);

  // Records the success and duration of taking a screenshot for AIvX models.
  static void RecordPassageEmbeddingModelExecutionTimeAndStatus(
      PredictionModelType model_type,
      base::TimeTicks snapshot_inquire_start_time,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Records the status of language detection during the Aiv4 workflow.
  static void RecordLanguageDetectionStatus(LanguageDetectionStatus status);

  // Records whether the passage embeddings calculation ran into a timeout
  // during the Aiv4 workflow.
  static void RecordPassageEmbeddingsCalculationTimeout(bool timeout);

  // Records whether the passage embedder metadata was valid when the AIv4
  // workflow was initiated.
  static void RecordPassageEmbedderMetadataValid(bool valid);

  // Records whether the UI selection logic of the
  // PermissionBasedPredictionUiSelector ran into a timeout.
  static void RecordPredictionServiceTimeout(bool timeout);

  // Records if the browser was active at the time the prompt started displaying
  static void RecordPromptShownInActiveBrowser(
      RequestTypeForUma request_type,
      bool embedded_permission_element_initiated,
      bool active);

  // Records that a permission prompt was auto-rejected because an actor
  // is operating on the tab.
  static void RecordPermissionAutoRejectForActor(ContentSettingsType permission,
                                                 bool is_actor_operating);

  // Records the duration of the browsing session before a permission prompt
  // was displayed.
  static void RecordPrePromptSessionDuration(
      ContentSettingsType permission,
      base::TimeTicks request_first_display_time);

  // Records the duration of the browsing session after a permission prompt has
  // been displayed.
  static void RecordPostPromptSessionDuration(
      ContentSettingsType permission,
      base::TimeTicks request_first_display_time);

  // A scoped class that will check the current resolved content setting on
  // construction and report a revocation metric accordingly if the revocation
  // condition is met (from ALLOW to something else).
  class ScopedRevocationReporter {
   public:
    ScopedRevocationReporter(content::BrowserContext* browser_context,
                             const GURL& primary_url,
                             const GURL& secondary_url,
                             ContentSettingsType content_type,
                             PermissionSourceUI source_ui);

    ScopedRevocationReporter(content::BrowserContext* browser_context,
                             const ContentSettingsPattern& primary_pattern,
                             const ContentSettingsPattern& secondary_pattern,
                             ContentSettingsType content_type,
                             PermissionSourceUI source_ui);

    ~ScopedRevocationReporter();

    // Returns true if a ScopedRevocationReporter instance is in scope.
    static bool IsInstanceInScope();

   private:
    raw_ptr<content::BrowserContext> browser_context_;
    const GURL primary_url_;
    const GURL secondary_url_;
    ContentSettingsType content_type_;
    PermissionSourceUI source_ui_;
    bool is_initially_allowed_;
    base::Time last_modified_date_;
  };

 private:
  friend class PermissionUmaUtilTest;

  // Records UMA and UKM metrics for ContentSettingsTypes that have user
  // facing permission prompts. The passed in `permission` must be such that
  // PermissionUtil::IsPermission(permission) returns true.
  // web_contents may be null when for recording non-prompt actions.
  static void RecordPermissionAction(
      ContentSettingsType permission,
      PermissionAction action,
      PermissionSourceUI source_ui,
      PermissionRequestGestureType gesture_type,
      base::TimeDelta time_to_action,
      PermissionPromptDisposition ui_disposition,
      std::optional<PermissionPromptDispositionReason> ui_reason,
      std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
      const GURL& requesting_origin,
      content::BrowserContext* browser_context,
      content::RenderFrameHost* render_frame_host,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          predicted_grant_likelihood,
      std::optional<PermissionRequestRelevance> permission_request_relevance,
      std::optional<permissions::PermissionAiRelevanceModel>
          permission_ai_relevance_model,
      std::optional<bool> prediction_decision_held_back,
      const PromptOptions& prompt_options,
      std::optional<GeolocationAccuracy> initial_geolocation_accuracy_selection,
      std::optional<GeolocationPromptType> geolocation_prompt_type,
      std::optional<ukm::SourceId> source_id);

  // Records |count| total prior actions for a prompt of type |permission|
  // for a single origin using |prefix| for the metric.
  static void RecordPermissionPromptPriorCount(ContentSettingsType permission,
                                               const std::string& prefix,
                                               int count);

  static void RecordPromptDecided(
      const std::vector<std::unique_ptr<PermissionRequest>>& requests,
      bool accepted,
      bool is_one_time);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
