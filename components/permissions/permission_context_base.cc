// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_context_base.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/browser/guest_view_base.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#include "ui/android/window_android.h"
#endif

namespace permissions {
namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

void LogPermissionBlockedMessage(content::RenderFrameHost* rfh,
                                 std::string_view reason,
                                 ContentSettingsType type) {
  rfh->GetOutermostMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StrCat({PermissionUtil::GetPermissionString(type),
                    " permission has been blocked", reason}));
}

}  // namespace

// static
const char PermissionContextBase::kPermissionsKillSwitchFieldStudy[] =
    "PermissionsKillSwitch";
// static
const char PermissionContextBase::kPermissionsKillSwitchBlockedValue[] =
    "blocked";

PermissionContextBase::PermissionContextBase(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type,
    network::mojom::PermissionsPolicyFeature permissions_policy_feature)
    : browser_context_(browser_context),
      content_settings_type_(content_settings_type),
      permissions_policy_feature_(permissions_policy_feature) {
  CHECK(permissions::PermissionUtil::IsPermission(content_settings_type_))
      << content_settings_type_;
}

PermissionContextBase::~PermissionContextBase() {
  DCHECK(permission_observers_.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PermissionContextBase::RequestPermission(
    std::unique_ptr<PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* const rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());

  if (!rfh) {
    // Permission request is not allowed without a valid RenderFrameHost.
    std::move(callback).Run(content::PermissionResult(
        PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  request_data
      ->WithRequestingOrigin(
          request_data->requesting_origin.DeprecatedGetOriginAsURL())
      .WithEmbeddingOrigin(GetEffectiveEmbedderOrigin(rfh));

  if (!request_data->requesting_origin.is_valid() ||
      !request_data->embedding_origin.is_valid()) {
    std::string type_name =
        PermissionUtil::GetPermissionString(content_settings_type_);

    DVLOG(1) << "Attempt to use " << type_name
             << " from an invalid URL: " << request_data->requesting_origin
             << "," << request_data->embedding_origin << " (" << type_name
             << " is not supported in popups)";
    NotifyPermissionSet(*request_data, std::move(callback),
                        /*persist=*/false, PermissionDecision::kDeny,
                        /*is_final_decision=*/true);
    return;
  }

  // Check the content setting to see if the user has already made a decision,
  // or if the origin is under embargo. If so, respect that decision.
  DCHECK(rfh);
  content::PermissionResult result = GetPermissionStatus(*request_data, rfh);

  bool status_ignorable = PermissionUtil::CanPermissionRequestIgnoreStatus(
      request_data, result.source);

  if (!status_ignorable && (result.status == PermissionStatus::GRANTED ||
                            result.status == PermissionStatus::DENIED)) {
    static constexpr char kResetInstructions[] =
        " This can be reset in "
#if BUILDFLAG(IS_ANDROID)
        "Site Settings"
#else
        "Page Info which can be accessed by clicking the tune icon next to "
        "the URL"
#endif
        ". See https://www.chromestatus.com/feature/6443143280984064 for "
        "more information.";
    switch (result.source) {
      case content::PermissionStatusSource::KILL_SWITCH:
        // Block the request and log to the developer console.
        static constexpr char kPermissionBlockedKillSwitchReason[] = ".";
        LogPermissionBlockedMessage(rfh, kPermissionBlockedKillSwitchReason,
                                    content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        std::move(callback).Run(content::PermissionResult(
            PermissionStatus::DENIED,
            content::PermissionStatusSource::UNSPECIFIED));
        return;
      case content::PermissionStatusSource::MULTIPLE_DISMISSALS:
        static constexpr char kPermissionBlockedRepeatedDismissalsReason[] =
            " as the user has dismissed the permission prompt several times.";
        LogPermissionBlockedMessage(
            rfh,
            base::StrCat({kPermissionBlockedRepeatedDismissalsReason,
                          kResetInstructions}),
            content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::MULTIPLE_IGNORES:
        static constexpr char kPermissionBlockedRepeatedIgnoresReason[] =
            " as the user has ignored the permission prompt several times.";
        LogPermissionBlockedMessage(
            rfh,
            base::StrCat(
                {kPermissionBlockedRepeatedIgnoresReason, kResetInstructions}),
            content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::FEATURE_POLICY:
        static constexpr char kPermissionBlockedPermissionsPolicyReason[] =
            " because of a permissions policy applied to the current document. "
            "See https://crbug.com/414348233 for more details.";
        LogPermissionBlockedMessage(rfh,
                                    kPermissionBlockedPermissionsPolicyReason,
                                    content_settings_type_);
        break;
      case content::PermissionStatusSource::RECENT_DISPLAY:
        static constexpr char kPermissionBlockedRecentDisplayReason[] =
            " as the prompt has already been displayed to the user recently.";
        LogPermissionBlockedMessage(rfh, kPermissionBlockedRecentDisplayReason,
                                    content_settings_type_);
        break;
      case content::PermissionStatusSource::UNSPECIFIED:
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::APP_LEVEL_SETTINGS:
        static constexpr char kPermissionBlockedAppLevelSettingsReason[] =
            " because Chrome does not have and cannot acquire app-level "
            "permissions for the corresponding capability.";
        LogPermissionBlockedMessage(rfh,
                                    kPermissionBlockedAppLevelSettingsReason,
                                    content_settings_type_);
        break;
      case content::PermissionStatusSource::ACTOR_OVERRIDE:
      case content::PermissionStatusSource::FENCED_FRAME:
      case content::PermissionStatusSource::INSECURE_ORIGIN:
      case content::PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
      case content::PermissionStatusSource::HEURISTIC_GRANT:
        break;
    }

    // If we are under embargo, record the embargo reason for which we have
    // suppressed the prompt.
    PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(result.source);
    bool persist =
        result.source == content::PermissionStatusSource::HEURISTIC_GRANT;
    PermissionDecision allow_decision =
        result.source == content::PermissionStatusSource::HEURISTIC_GRANT
            ? PermissionDecision::kAllowThisTime
            : PermissionDecision::kAllow;
    NotifyPermissionSet(*request_data, std::move(callback), persist,
                        result.status == blink::mojom::PermissionStatus::GRANTED
                            ? allow_decision
                            : PermissionDecision::kDeny,
                        /*is_final_decision=*/true);
    return;
  }
  PermissionUmaUtil::RecordPermissionRequestedFromFrame(content_settings_type_,
                                                        rfh);

  // We are going to show a prompt now.
  PermissionUmaUtil::PermissionRequested(content_settings_type_);
  PermissionUmaUtil::RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus::NOT_EMBARGOED);

  DecidePermission(std::move(request_data), std::move(callback));
}

bool PermissionContextBase::IsRestrictedToSecureOrigins() const {
  return true;
}

void PermissionContextBase::UserMadePermissionDecision(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    PermissionDecision decision) {}

std::unique_ptr<PermissionRequest>
PermissionContextBase::CreatePermissionRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PermissionRequestData> request_data,
    PermissionRequest::PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure request_finished_callback) const {
  return std::make_unique<PermissionRequest>(
      std::move(request_data), std::move(permission_decided_callback),
      std::move(request_finished_callback), UsesAutomaticEmbargo());
}

bool PermissionContextBase::UsesAutomaticEmbargo() const {
  return true;
}

const PermissionRequest* PermissionContextBase::FindPermissionRequest(
    const PermissionRequestID& id) const {
  const auto request = pending_requests_.find(id.ToString());

  if (request == pending_requests_.end()) {
    return nullptr;
  }

  return request->second.first.get();
}

GURL PermissionContextBase::GetEffectiveEmbedderOrigin(
    content::RenderFrameHost* rfh) const {
  return PermissionUtil::GetLastCommittedOriginAsURL(rfh->GetMainFrame());
}

content::PermissionResult PermissionContextBase::GetPermissionStatus(
    const PermissionRequestData& request_data,
    content::RenderFrameHost* render_frame_host) const {
  if (base::FeatureList::IsEnabled(blink::features::kGeolocationElement) &&
      request_data.IsEligibleForHeuristicAutoGrant() &&
      PermissionsClient::Get()
          ->GetPermissionActionsHistory(browser_context_)
          ->CheckHeuristicallyAutoGranted(request_data.requesting_origin,
                                          content_settings_type_)) {
    return content::PermissionResult(
        PermissionStatus::GRANTED,
        content::PermissionStatusSource::HEURISTIC_GRANT);
  }
  return GetPermissionStatus(*request_data.resolver, render_frame_host,
                             request_data.requesting_origin,
                             request_data.embedding_origin);
}

content::PermissionResult PermissionContextBase::GetPermissionStatus(
    const PermissionResolver& resolver,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // If the permission has been disabled through Finch, block all requests.
  if (IsPermissionKillSwitchOn()) {
    return content::PermissionResult(
        PermissionStatus::DENIED, content::PermissionStatusSource::KILL_SWITCH);
  }

  if (base::FeatureList::IsEnabled(features::kGlicActorPermissionsAutoReject) &&
      render_frame_host) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    bool is_actor_operating =
        PermissionsClient::Get()->IsActorOperatingOnWebContents(web_contents);
    PermissionUmaUtil::RecordPermissionAutoRejectForActor(
        content_settings_type_, is_actor_operating);
    if (is_actor_operating) {
      return content::PermissionResult(
          PermissionStatus::DENIED,
          content::PermissionStatusSource::ACTOR_OVERRIDE);
    }
  }

  if (!IsPermissionAvailableToOrigins(requesting_origin, embedding_origin)) {
    return content::PermissionResult(
        PermissionStatus::DENIED,
        content::PermissionStatusSource::INSECURE_ORIGIN);
  }

  // Check whether the feature is enabled for the frame by permissions policy.
  // We can only do this when a RenderFrameHost has been provided.
  if (render_frame_host &&
      !PermissionAllowedByPermissionsPolicy(render_frame_host)) {
    return content::PermissionResult(
        PermissionStatus::DENIED,
        content::PermissionStatusSource::FEATURE_POLICY);
  }

  if (render_frame_host) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);

    // Automatically deny all HTTP or HTTPS requests where the virtual URL and
    // the loaded URL are for different origins. The loaded URL is the one
    // actually in the renderer, but the virtual URL is the one
    // seen by the user. This may be very confusing for a user to see in a
    // permissions request.
    content::NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    if (entry) {
      const GURL virtual_url = entry->GetVirtualURL();
      const GURL loaded_url = entry->GetURL();
      if (virtual_url.SchemeIsHTTPOrHTTPS() &&
          loaded_url.SchemeIsHTTPOrHTTPS() &&
          !url::IsSameOriginWith(virtual_url, loaded_url)) {
        return content::PermissionResult(
            PermissionStatus::DENIED,
            content::PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN);
      }
    }
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromRenderFrameHost(render_frame_host);
  if (guest) {
    // Content inside GuestView instances may have different permission
    // behavior.
    std::optional<content::PermissionResult> maybe_result =
        guest->OverridePermissionResult(content_settings_type_);
    if (maybe_result.has_value()) {
      return maybe_result.value();
    }
    // Some GuestViews are loaded in a separate StoragePartition. Given that
    // permissions are scoped to a BrowserContext, not a StoragePartition, we
    // may have a situation where a user has granted a permission to an origin
    // in a tab and then visits the same origin in a guest. This would lead to
    // inappropriate sharing of the permission with the guest. To mitigate this,
    // we drop permission requests from guests for cases where it's not possible
    // for the guest to have been granted the permission. Note that sharing of
    // permissions that the guest could legitimately be granted is still
    // possible.
    // TODO(crbug.com/40068594): Scope granted permissions to a
    // StoragePartition.
    if (!guest->IsPermissionRequestable(content_settings_type_)) {
      return content::PermissionResult(
          PermissionStatus::DENIED,
          content::PermissionStatusSource::UNSPECIFIED);
    }
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kReturnDeniedForNotificationsWhenNoAppLevelSettings)) {
    if (content_settings_type_ == ContentSettingsType::NOTIFICATIONS) {
      bool app_level_settings_allow_site_notifications =
          enabled_app_level_notification_permission_for_testing_.value_or(
              DoesAppLevelSettingsAllowSiteNotifications());
      base::UmaHistogramBoolean(
          "Permissions.Status.Notifications.EnabledAppLevel",
          app_level_settings_allow_site_notifications);

      if (!app_level_settings_allow_site_notifications) {
        // Chrome is not able to send notifications at Android level.
        return content::PermissionResult(
            PermissionStatus::DENIED,
            content::PermissionStatusSource::APP_LEVEL_SETTINGS);
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  PermissionSetting retrieved_permission_data = GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);

  auto permission_status =
      resolver.DeterminePermissionStatus(retrieved_permission_data);

  if (permission_status != PermissionStatus::ASK) {
    return content::PermissionResult(
        permission_status, content::PermissionStatusSource::UNSPECIFIED,
        retrieved_permission_data);
  }

  if (UsesAutomaticEmbargo()) {
    std::optional<content::PermissionResult> result =
        PermissionsClient::Get()
            ->GetPermissionDecisionAutoBlocker(browser_context_)
            ->GetEmbargoResult(requesting_origin, content_settings_type_);
    if (result) {
      result->retrieved_permission_setting = retrieved_permission_data;
      return std::move(result).value();
    }
  }
  return content::PermissionResult(PermissionStatus::ASK,
                                   content::PermissionStatusSource::UNSPECIFIED,
                                   retrieved_permission_data);
}

content::PermissionResult PermissionContextBase::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return GetPermissionStatus(*CreatePermissionResolver(permission_descriptor),
                             render_frame_host, requesting_origin,
                             embedding_origin);
}

bool PermissionContextBase::IsPermissionAvailableToOrigins(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (IsRestrictedToSecureOrigins()) {
    if (!network::IsUrlPotentiallyTrustworthy(requesting_origin)) {
      return false;
    }

    // TODO(raymes): We should check the entire chain of embedders here whenever
    // possible as this corresponds to the requirements of the secure contexts
    // spec and matches what is implemented in blink. Right now we just check
    // the top level and requesting origins.
    if (!PermissionsClient::Get()->CanBypassEmbeddingOriginCheck(
            requesting_origin, embedding_origin) &&
        !network::IsUrlPotentiallyTrustworthy(embedding_origin)) {
      return false;
    }
  }
  return true;
}

content::PermissionResult
PermissionContextBase::UpdatePermissionStatusWithDeviceStatus(
    content::WebContents* web_contents,
    content::PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  MaybeUpdateCachedHasDevicePermission(web_contents);

  // If the site content setting is ASK/BLOCKED the device-level permission
  // won't affect it.
  if (result.status != blink::mojom::PermissionStatus::GRANTED) {
    return result;
  }

  // If the device-level permission is granted, it has no effect on the result.
  if (last_has_device_permission_result_.has_value() &&
      last_has_device_permission_result_.value()) {
    return result;
  }

#if BUILDFLAG(IS_ANDROID)
  if (!web_contents || !web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return result;
  }
  result.status =
      CanRequestSystemPermission(content_settings_type(), web_contents)
          ? blink::mojom::PermissionStatus::ASK
          : blink::mojom::PermissionStatus::DENIED;
#else
  // Otherwise the result will be "ASK" if the browser can ask for the
  // device-level permission, and "BLOCKED" otherwise.
  result.status = PermissionsClient::Get()->CanRequestDevicePermission(
                      content_settings_type())
                      ? blink::mojom::PermissionStatus::ASK
                      : blink::mojom::PermissionStatus::DENIED;
#endif
  return result;
}

void PermissionContextBase::ResetPermission(const GURL& requesting_origin,
                                            const GURL& embedding_origin) {
  if (!content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          content_settings_type())) {
    return;
  }

  PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetWebsiteSettingDefaultScope(requesting_origin, embedding_origin,
                                      content_settings_type(), base::Value());
}

bool PermissionContextBase::AlwaysIncludeDeviceStatus() const {
  return false;
}

bool PermissionContextBase::IsPermissionKillSwitchOn() const {
  const std::string param = base::GetFieldTrialParamValue(
      kPermissionsKillSwitchFieldStudy,
      PermissionUtil::GetPermissionString(content_settings_type_));

  return param == kPermissionsKillSwitchBlockedValue;
}

PermissionSetting PermissionContextBase::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->GetPermissionSetting(requesting_origin, embedding_origin,
                             content_settings_type());
}

void PermissionContextBase::DecidePermission(
    std::unique_ptr<PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Under permission delegation, when we display a permission prompt, the
  // origin displayed in the prompt should never differ from the top-level
  // origin. Storage access API requests are excluded as they are expected to
  // request permissions from the frame origin needing access.
  DCHECK(PermissionsClient::Get()->CanBypassEmbeddingOriginCheck(
             request_data->requesting_origin, request_data->embedding_origin) ||
         request_data->requesting_origin == request_data->embedding_origin ||
         content_settings_type_ == ContentSettingsType::STORAGE_ACCESS);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data->id.global_render_frame_host_id());
  DCHECK(rfh);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  // TODO(felt): sometimes |permission_request_manager| is null. This check is
  // meant to prevent crashes. See crbug.com/457091.
  if (!permission_request_manager) {
    std::move(callback).Run(content::PermissionResult(
        PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED));
    return;
  }

  auto decided_cb = base::BindRepeating(
      &PermissionContextBase::PermissionDecided, weak_factory_.GetWeakPtr());
  auto cleanup_cb =
      base::BindOnce(&PermissionContextBase::CleanUpRequest,
                     weak_factory_.GetWeakPtr(), web_contents, request_data->id,
                     request_data->IsEmbeddedPermissionElementInitiated());
  PermissionRequestID permission_request_id = request_data->id;

  std::unique_ptr<PermissionRequest> request =
      CreatePermissionRequest(web_contents, std::move(request_data),
                              std::move(decided_cb), std::move(cleanup_cb));

  bool inserted =
      pending_requests_
          .insert(std::make_pair(
              permission_request_id.ToString(),
              std::make_pair(request->GetWeakPtr(), std::move(callback))))
          .second;

  DCHECK(inserted) << "Duplicate id " << permission_request_id.ToString();

  permission_request_manager->AddRequest(rfh, std::move(request));
}

void PermissionContextBase::PermissionDecided(
    PermissionDecision decision,
    bool is_final_decision,
    const PermissionRequestData& request_data) {
  UserMadePermissionDecision(request_data.id, request_data.requesting_origin,
                             request_data.embedding_origin, decision);
  //  If a permission request originates from an embedded element, its
  //  cancellation would be a result of a "system permission changed event."
  //  In this case, we will dispatch `OnPermissionChanged` early here.
  if (request_data.IsEmbeddedPermissionElementInitiated() &&
      decision == PermissionDecision::kNone) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        request_data.id.global_render_frame_host_id());
    DCHECK(rfh);
    MaybeUpdateCachedHasDevicePermission(
        content::WebContents::FromRenderFrameHost(rfh));
  }

  bool persist = decision != PermissionDecision::kNone;

  auto request = pending_requests_.find(request_data.id.ToString());
  CHECK(request->second.first);
  CHECK(request != pending_requests_.end());
  // Check if `request` has `BrowserPermissionCallback`. The call back might be
  // missing if a permission prompt was preignored and we already notified an
  // origin about it.
  if (request->second.second) {
    NotifyPermissionSet(request_data, std::move(request->second.second),
                        persist, decision, is_final_decision);
  } else {
    NotifyPermissionSet(request_data, base::DoNothing(), persist, decision,
                        is_final_decision);
  }
}

content::BrowserContext* PermissionContextBase::browser_context() const {
  return browser_context_;
}

void PermissionContextBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  NotifyObservers(primary_pattern, secondary_pattern, content_type_set);
}

void PermissionContextBase::AddObserver(
    permissions::Observer* permission_observer) {
  if (permission_observers_.empty() &&
      !content_setting_observer_registered_by_subclass_) {
    PermissionsClient::Get()
        ->GetSettingsMap(browser_context_)
        ->AddObserver(this);
  }
  permission_observers_.AddObserver(permission_observer);
}

void PermissionContextBase::RemoveObserver(
    permissions::Observer* permission_observer) {
  permission_observers_.RemoveObserver(permission_observer);
  if (permission_observers_.empty() &&
      !content_setting_observer_registered_by_subclass_) {
    PermissionsClient::Get()
        ->GetSettingsMap(browser_context_)
        ->RemoveObserver(this);
  }
}

std::unique_ptr<PermissionResolver>
PermissionContextBase::CreatePermissionResolver(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor) const {
  return std::make_unique<ContentSettingPermissionResolver>(
      content_settings_type_);
}

void PermissionContextBase::MaybeUpdateCachedHasDevicePermission(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  if (!web_contents || !web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return;
  }
  const bool has_device_permission =
      has_device_permission_for_test_.has_value()
          ? has_device_permission_for_test_.value()
          : HasSystemPermission(content_settings_type(), web_contents);
#else
  const bool has_device_permission =
      has_device_permission_for_test_.has_value()
          ? has_device_permission_for_test_.value()
          : PermissionsClient::Get()->HasDevicePermission(
                content_settings_type());
#endif

  const bool should_notify_observers =
      last_has_device_permission_result_.has_value() &&
      has_device_permission != last_has_device_permission_result_;

  // We need to update |last_has_device_permission_result_| before calling
  // |OnContentSettingChanged| to avoid causing a re-entrancy issue since the
  // |OnContentSettingChanged| will likely end up calling |GetPermissionStatus|.
  last_has_device_permission_result_ = has_device_permission;

  if (should_notify_observers) {
    NotifyObservers(ContentSettingsPattern::Wildcard(),
                    ContentSettingsPattern::Wildcard(),
                    ContentSettingsTypeSet(content_settings_type()));
  }
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestData& request_data,
    BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision,
    bool is_final_decision) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Note that rfh may be null, see crbug.com/426909787.
  auto* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  // Need to reretrieve the persisted value, since the underlying permission
  // status may have changed in the meantime
  auto previous_value = GetPermissionStatusInternal(
      rfh, request_data.requesting_origin, request_data.embedding_origin);
  auto new_value = request_data.resolver->ComputePermissionDecisionResult(
      previous_value, decision, request_data.prompt_options);

  if (persist) {
    // Clone new value, because we need it again for the callback.
    UpdateSetting(request_data, new_value,
                  decision == PermissionDecision::kAllowThisTime);
  }

  if (is_final_decision) {
    UpdateTabContext(request_data,
                     decision == PermissionDecision::kAllow ||
                         decision == PermissionDecision::kAllowThisTime);
    if (rfh && decision == PermissionDecision::kAllow) {
      PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
          content_settings_type_, rfh);
    }
  }

  std::move(callback).Run(content::PermissionResult(
      PermissionUtil::PermissionDecisionToPermissionStatus(decision),
      content::PermissionStatusSource::UNSPECIFIED, new_value));
}

void PermissionContextBase::CleanUpRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    bool embedded_permission_element_initiated) {
  // A request from an embedded permission element requires a notification
  // `OnPermissionChanged` when changing the device status, which is currently
  // unavailable. We compare the device status with the cached status and notify
  // `OnPermissionChanged` here. We should remove this line once the device
  // status change observer is implemented.
  if (embedded_permission_element_initiated) {
    MaybeUpdateCachedHasDevicePermission(web_contents);
  }

  size_t success = pending_requests_.erase(id.ToString());
  DCHECK(success == 1) << "Missing request " << id.ToString();
}

void PermissionContextBase::UpdateSetting(
    const PermissionRequestData& request_data,
    PermissionSetting setting,
    bool is_one_time) {
  DCHECK_EQ(request_data.requesting_origin,
            request_data.requesting_origin.DeprecatedGetOriginAsURL());
  DCHECK_EQ(request_data.embedding_origin,
            request_data.embedding_origin.DeprecatedGetOriginAsURL());

  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(
      is_one_time ? content_settings::mojom::SessionModel::ONE_TIME
                  : content_settings::mojom::SessionModel::DURABLE);

  // The unused permissions module in Safety check will revoke unused site
  // permissions after a finite amount of time if the permission can be revoked.
  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      content_settings_type_);
  if (info && content_settings::CanBeAutoRevokedAsUnusedPermission(
                  content_settings_type(), info->delegate().ToValue(setting),
                  is_one_time)) {
    constraints.set_track_last_visit_for_autoexpiration(true);
  }

  if (is_one_time) {
    if (content_settings::ShouldTypeExpireActively(content_settings_type())) {
      constraints.set_lifetime(kOneTimePermissionMaximumLifetime);
    }
  }
  PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetPermissionSettingDefaultScope(
          request_data.requesting_origin, request_data.embedding_origin,
          content_settings_type(), setting, constraints);
}

bool PermissionContextBase::PermissionAllowedByPermissionsPolicy(
    content::RenderFrameHost* rfh) const {
  // Some features don't have an associated permissions policy yet. Allow those.
  if (permissions_policy_feature_ ==
      network::mojom::PermissionsPolicyFeature::kNotFound) {
    return true;
  }

  return rfh->IsFeatureEnabled(permissions_policy_feature_);
}

void PermissionContextBase::NotifyObservers(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) const {
  if (!content_type_set.Contains(content_settings_type_)) {
    return;
  }

  for (permissions::Observer& obs : permission_observers_) {
    obs.OnPermissionChanged(primary_pattern, secondary_pattern,
                            content_type_set);
  }
}

}  // namespace permissions
