// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_context_base.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/guest_view/browser/guest_view_base.h"
#endif

namespace permissions {
namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

const char kPermissionBlockedKillSwitchMessage[] =
    "%s permission has been blocked.";

#if BUILDFLAG(IS_ANDROID)
const char kPermissionBlockedRepeatedDismissalsMessage[] =
    "%s permission has been blocked as the user has dismissed the permission "
    "prompt several times. This can be reset in Site Settings. See "
    "https://www.chromestatus.com/feature/6443143280984064 for more "
    "information.";

const char kPermissionBlockedRepeatedIgnoresMessage[] =
    "%s permission has been blocked as the user has ignored the permission "
    "prompt several times. This can be reset in Site Settings. See "
    "https://www.chromestatus.com/feature/6443143280984064 for more "
    "information.";
#else
const char kPermissionBlockedRepeatedDismissalsMessage[] =
    "%s permission has been blocked as the user has dismissed the permission "
    "prompt several times. This can be reset in Page Info which can be "
    "accessed by clicking the tune icon next to the URL. See "
    "https://www.chromestatus.com/feature/6443143280984064 for more "
    "information.";

const char kPermissionBlockedRepeatedIgnoresMessage[] =
    "%s permission has been blocked as the user has ignored the permission "
    "prompt several times. This can be reset in Page Info which can be "
    "accessed by clicking the tune icon next to the URL. See "
    "https://www.chromestatus.com/feature/6443143280984064 for more "
    "information.";
#endif

const char kPermissionBlockedRecentDisplayMessage[] =
    "%s permission has been blocked as the prompt has already been displayed "
    "to the user recently.";

const char kPermissionBlockedPermissionsPolicyMessage[] =
    "%s permission has been blocked because of a permissions policy applied to"
    " the current document. See https://goo.gl/EuHzyv for more details.";

void LogPermissionBlockedMessage(content::RenderFrameHost* rfh,
                                 const char* message,
                                 ContentSettingsType type) {
  rfh->GetOutermostMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintfNonConstexpr(
          message, PermissionUtil::GetPermissionString(type).c_str()));
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
    blink::mojom::PermissionsPolicyFeature permissions_policy_feature)
    : browser_context_(browser_context),
      content_settings_type_(content_settings_type),
      permissions_policy_feature_(permissions_policy_feature) {
  CHECK(permissions::PermissionUtil::IsPermission(content_settings_type_));
  PermissionDecisionAutoBlocker::UpdateFromVariations();
}

PermissionContextBase::~PermissionContextBase() {
  DCHECK(permission_observers_.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PermissionContextBase::RequestPermission(
    PermissionRequestData request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* const rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  if (!rfh) {
    // Permission request is not allowed without a valid RenderFrameHost.
    std::move(callback).Run(CONTENT_SETTING_ASK);
    return;
  }

  request_data
      .WithRequestingOrigin(
          request_data.requesting_origin.DeprecatedGetOriginAsURL())
      .WithEmbeddingOrigin(
          PermissionUtil::GetLastCommittedOriginAsURL(rfh->GetMainFrame()));

  if (!request_data.requesting_origin.is_valid() ||
      !request_data.embedding_origin.is_valid()) {
    std::string type_name =
        PermissionUtil::GetPermissionString(content_settings_type_);

    DVLOG(1) << "Attempt to use " << type_name
             << " from an invalid URL: " << request_data.requesting_origin
             << "," << request_data.embedding_origin << " (" << type_name
             << " is not supported in popups)";
    NotifyPermissionSet(request_data.id, request_data.requesting_origin,
                        request_data.embedding_origin, std::move(callback),
                        /*persist=*/false, CONTENT_SETTING_BLOCK,
                        /*is_one_time=*/false,
                        /*is_final_decision=*/true);
    return;
  }

  // Check the content setting to see if the user has already made a decision,
  // or if the origin is under embargo. If so, respect that decision.
  DCHECK(rfh);
  content::PermissionResult result = GetPermissionStatus(
      rfh, request_data.requesting_origin, request_data.embedding_origin);

  bool status_ignorable = PermissionUtil::CanPermissionRequestIgnoreStatus(
      request_data, result.source);

  if (!status_ignorable && (result.status == PermissionStatus::GRANTED ||
                            result.status == PermissionStatus::DENIED)) {
    switch (result.source) {
      case content::PermissionStatusSource::KILL_SWITCH:
        // Block the request and log to the developer console.
        LogPermissionBlockedMessage(rfh, kPermissionBlockedKillSwitchMessage,
                                    content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        std::move(callback).Run(CONTENT_SETTING_BLOCK);
        return;
      case content::PermissionStatusSource::MULTIPLE_DISMISSALS:
        LogPermissionBlockedMessage(rfh,
                                    kPermissionBlockedRepeatedDismissalsMessage,
                                    content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::MULTIPLE_IGNORES:
        LogPermissionBlockedMessage(rfh,
                                    kPermissionBlockedRepeatedIgnoresMessage,
                                    content_settings_type_);
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::FEATURE_POLICY:
        LogPermissionBlockedMessage(rfh,
                                    kPermissionBlockedPermissionsPolicyMessage,
                                    content_settings_type_);
        break;
      case content::PermissionStatusSource::RECENT_DISPLAY:
        LogPermissionBlockedMessage(rfh, kPermissionBlockedRecentDisplayMessage,
                                    content_settings_type_);
        break;
      case content::PermissionStatusSource::UNSPECIFIED:
        PermissionUmaUtil::RecordPermissionRequestedFromFrame(
            content_settings_type_, rfh);
        break;
      case content::PermissionStatusSource::FENCED_FRAME:
      case content::PermissionStatusSource::INSECURE_ORIGIN:
      case content::PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
        break;
    }

    // If we are under embargo, record the embargo reason for which we have
    // suppressed the prompt.
    PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(result.source);
    NotifyPermissionSet(
        request_data.id, request_data.requesting_origin,
        request_data.embedding_origin, std::move(callback),
        /*persist=*/false,
        PermissionUtil::PermissionStatusToContentSetting(result.status),
        /*is_one_time=*/false,
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
    ContentSetting content_setting) {}

std::unique_ptr<PermissionRequest>
PermissionContextBase::CreatePermissionRequest(
    content::WebContents* web_contents,
    PermissionRequestData request_data,
    PermissionRequest::PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback) const {
  return std::make_unique<PermissionRequest>(
      std::move(request_data), std::move(permission_decided_callback),
      std::move(delete_callback), UsesAutomaticEmbargo());
}

bool PermissionContextBase::UsesAutomaticEmbargo() const {
  return true;
}

content::PermissionResult PermissionContextBase::GetPermissionStatus(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // If the permission has been disabled through Finch, block all requests.
  if (IsPermissionKillSwitchOn()) {
    return content::PermissionResult(
        PermissionStatus::DENIED, content::PermissionStatusSource::KILL_SWITCH);
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
    if (base::FeatureList::IsEnabled(
            features::kMitigateUnpartitionedWebviewPermissions) &&
        !guest->IsPermissionRequestable(content_settings_type_)) {
      return content::PermissionResult(
          PermissionStatus::DENIED,
          content::PermissionStatusSource::UNSPECIFIED);
    }
  }
#endif

  ContentSetting content_setting = GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);

  if (content_setting != CONTENT_SETTING_ASK) {
    return content::PermissionResult(
        PermissionUtil::ContentSettingToPermissionStatus(content_setting),
        content::PermissionStatusSource::UNSPECIFIED);
  }

  if (UsesAutomaticEmbargo()) {
    std::optional<content::PermissionResult> result =
        PermissionsClient::Get()
            ->GetPermissionDecisionAutoBlocker(browser_context_)
            ->GetEmbargoResult(requesting_origin, content_settings_type_);
    if (result) {
      DCHECK(result->status == PermissionStatus::DENIED);
      return result.value();
    }
  }
  return content::PermissionResult(
      PermissionStatus::ASK, content::PermissionStatusSource::UNSPECIFIED);
}

bool PermissionContextBase::IsPermissionAvailableToOrigins(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (IsRestrictedToSecureOrigins()) {
    if (!network::IsUrlPotentiallyTrustworthy(requesting_origin))
      return false;

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
    content::PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  MaybeUpdatePermissionStatusWithDeviceStatus();

  // If the site content setting is ASK/BLOCKED the device-level permission
  // won't affect it.
  if (result.status != blink::mojom::PermissionStatus::GRANTED) {
    return result;
  }

  // If the device-level permission is granted, it has no effect on the result.
  if (last_has_device_permission_result_.value()) {
    return result;
  }

  // Otherwise the result will be "ASK" if the browser can ask for the
  // device-level permission, and "BLOCKED" otherwise.
  result.status = PermissionsClient::Get()->CanRequestDevicePermission(
                      content_settings_type())
                      ? blink::mojom::PermissionStatus::ASK
                      : blink::mojom::PermissionStatus::DENIED;

  return result;
}

void PermissionContextBase::ResetPermission(const GURL& requesting_origin,
                                            const GURL& embedding_origin) {
  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_settings_type_)) {
    return;
  }
  PermissionsClient::Get()
      ->GetSettingsMap(browser_context_)
      ->SetContentSettingDefaultScope(requesting_origin, embedding_origin,
                                      content_settings_type_,
                                      CONTENT_SETTING_DEFAULT);
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

ContentSetting PermissionContextBase::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context_)
      ->GetContentSetting(requesting_origin, embedding_origin,
                          content_settings_type_);
}

void PermissionContextBase::DecidePermission(
    PermissionRequestData request_data,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Under permission delegation, when we display a permission prompt, the
  // origin displayed in the prompt should never differ from the top-level
  // origin. Storage access API requests are excluded as they are expected to
  // request permissions from the frame origin needing access.
  DCHECK(PermissionsClient::Get()->CanBypassEmbeddingOriginCheck(
             request_data.requesting_origin, request_data.embedding_origin) ||
         request_data.requesting_origin == request_data.embedding_origin ||
         content_settings_type_ == ContentSettingsType::STORAGE_ACCESS);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());
  DCHECK(rfh);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  // TODO(felt): sometimes |permission_request_manager| is null. This check is
  // meant to prevent crashes. See crbug.com/457091.
  if (!permission_request_manager) {
    std::move(callback).Run(CONTENT_SETTING_ASK);
    return;
  }

  auto decided_cb = base::BindRepeating(
      &PermissionContextBase::PermissionDecided, weak_factory_.GetWeakPtr(),
      request_data.id, request_data.requesting_origin,
      request_data.embedding_origin);
  auto cleanup_cb = base::BindOnce(
      &PermissionContextBase::CleanUpRequest, weak_factory_.GetWeakPtr(),
      request_data.id, request_data.embedded_permission_element_initiated);
  PermissionRequestID permission_request_id = request_data.id;

  std::unique_ptr<PermissionRequest> request_ptr =
      CreatePermissionRequest(web_contents, std::move(request_data),
                              std::move(decided_cb), std::move(cleanup_cb));
  PermissionRequest* request = request_ptr.get();

  bool inserted =
      pending_requests_
          .insert(std::make_pair(
              permission_request_id.ToString(),
              std::make_pair(std::move(request_ptr), std::move(callback))))
          .second;
  DCHECK(inserted) << "Duplicate id " << permission_request_id.ToString();

  permission_request_manager->AddRequest(rfh, request);
}

void PermissionContextBase::PermissionDecided(const PermissionRequestID& id,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin,
                                              ContentSetting content_setting,
                                              bool is_one_time,
                                              bool is_final_decision) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_DEFAULT);
  UserMadePermissionDecision(id, requesting_origin, embedding_origin,
                             content_setting);

  bool persist = content_setting != CONTENT_SETTING_DEFAULT;

  auto request = pending_requests_.find(id.ToString());
  CHECK(request != pending_requests_.end(), base::NotFatalUntil::M130);
  // Check if `request` has `BrowserPermissionCallback`. The call back might be
  // missing if a permission prompt was preignored and we already notified an
  // origin about it.
  if (request->second.second) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(request->second.second), persist,
                        content_setting, is_one_time, is_final_decision);
  } else {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        base::DoNothing(), persist, content_setting,
                        is_one_time, is_final_decision);
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

void PermissionContextBase::MaybeUpdatePermissionStatusWithDeviceStatus() {
  const bool has_device_permission =
      has_device_permission_for_test_.has_value()
          ? has_device_permission_for_test_.value()
          : PermissionsClient::Get()->HasDevicePermission(
                content_settings_type());
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
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (persist) {
    UpdateContentSetting(requesting_origin, embedding_origin, content_setting,
                         is_one_time);
  }

  if (is_final_decision) {
    UpdateTabContext(id, requesting_origin,
                     content_setting == CONTENT_SETTING_ALLOW);
    if (content_setting == CONTENT_SETTING_ALLOW) {
      if (auto* rfh = content::RenderFrameHost::FromID(
              id.global_render_frame_host_id())) {
        PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
            content_settings_type_, rfh);
      }
    }
  }

  if (content_setting == CONTENT_SETTING_DEFAULT)
    content_setting = CONTENT_SETTING_ASK;

  std::move(callback).Run(content_setting);
}

void PermissionContextBase::CleanUpRequest(
    const PermissionRequestID& id,
    bool embedded_permission_element_initiated) {
  size_t success = pending_requests_.erase(id.ToString());
  // A request from an embedded permission element requires a notification
  // `OnPermissionChanged` when changing the device status, which is currently
  // unavailable. We compare the device status with the cached status and notify
  // `OnPermissionChanged` here. We should remove this line once the device
  // status change observer is implemented.
  if (embedded_permission_element_initiated) {
    MaybeUpdatePermissionStatusWithDeviceStatus();
  }
  DCHECK(success == 1) << "Missing request " << id.ToString();
}

void PermissionContextBase::UpdateContentSetting(const GURL& requesting_origin,
                                                 const GURL& embedding_origin,
                                                 ContentSetting content_setting,
                                                 bool is_one_time) {
  DCHECK_EQ(requesting_origin, requesting_origin.DeprecatedGetOriginAsURL());
  DCHECK_EQ(embedding_origin, embedding_origin.DeprecatedGetOriginAsURL());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(
      is_one_time ? content_settings::mojom::SessionModel::ONE_TIME
                  : content_settings::mojom::SessionModel::DURABLE);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kRecordPermissionExpirationTimestamps)) {
#endif  // BUILDFLAG(IS_ANDROID)
    // The Permissions module in Safety check will revoke permissions after
    // a finite amount of time if the permission can be revoked.
    if (content_settings::CanBeAutoRevoked(content_settings_type_,
                                           content_setting, is_one_time)) {
      // For #2, by definition, that should be all of them. If that changes in
      // the future, consider whether revocation for such permission makes
      // sense, and/or change this to an early return so that we don't
      // unnecessarily record timestamps where we don't need them.
      constraints.set_track_last_visit_for_autoexpiration(true);
    }
#if BUILDFLAG(IS_ANDROID)
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (is_one_time) {
    if (base::FeatureList::IsEnabled(
            content_settings::features::kActiveContentSettingExpiry)) {
      constraints.set_lifetime(kOneTimePermissionMaximumLifetime);
    }
  }

  PermissionsClient::Get()
      ->GetSettingsMap(browser_context_)
      ->SetContentSettingDefaultScope(requesting_origin, embedding_origin,
                                      content_settings_type_, content_setting,
                                      constraints);
}

bool PermissionContextBase::PermissionAllowedByPermissionsPolicy(
    content::RenderFrameHost* rfh) const {
  // Some features don't have an associated permissions policy yet. Allow those.
  if (permissions_policy_feature_ ==
      blink::mojom::PermissionsPolicyFeature::kNotFound)
    return true;

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
