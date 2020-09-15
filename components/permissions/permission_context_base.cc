// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_context_base.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_impl.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "url/gurl.h"

namespace permissions {
namespace {

const char kPermissionBlockedKillSwitchMessage[] =
    "%s permission has been blocked.";

#if defined(OS_ANDROID)
const char kPermissionBlockedRepeatedDismissalsMessage[] =
    "%s permission has been blocked as the user has dismissed the permission "
    "prompt several times. This can be reset in Site Settings. See "
    "https://www.chromestatus.com/features/6443143280984064 for more "
    "information.";

const char kPermissionBlockedRepeatedIgnoresMessage[] =
    "%s permission has been blocked as the user has ignored the permission "
    "prompt several times. This can be reset in Site Settings. See "
    "https://www.chromestatus.com/features/6443143280984064 for more "
    "information.";
#else
const char kPermissionBlockedRepeatedDismissalsMessage[] =
    "%s permission has been blocked as the user has dismissed the permission "
    "prompt several times. This can be reset in Page Info which can be "
    "accessed by clicking the lock icon next to the URL. See "
    "https://www.chromestatus.com/features/6443143280984064 for more "
    "information.";

const char kPermissionBlockedRepeatedIgnoresMessage[] =
    "%s permission has been blocked as the user has ignored the permission "
    "prompt several times. This can be reset in Page Info which can be "
    "accessed by clicking the lock icon next to the URL. See "
    "https://www.chromestatus.com/features/6443143280984064 for more "
    "information.";
#endif

const char kPermissionBlockedFeaturePolicyMessage[] =
    "%s permission has been blocked because of a Feature Policy applied to the "
    "current document. See https://goo.gl/EuHzyv for more details.";

const char kPermissionBlockedPortalsMessage[] =
    "%s permission has been blocked because it was requested inside a portal. "
    "Portals don't currently support permission requests.";

void LogPermissionBlockedMessage(content::WebContents* web_contents,
                                 const char* message,
                                 ContentSettingsType type) {
  web_contents->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf(message,
                         PermissionUtil::GetPermissionString(type).c_str()));
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
    blink::mojom::FeaturePolicyFeature feature_policy_feature)
    : browser_context_(browser_context),
      content_settings_type_(content_settings_type),
      feature_policy_feature_(feature_policy_feature) {
  PermissionDecisionAutoBlocker::UpdateFromVariations();
}

PermissionContextBase::~PermissionContextBase() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PermissionContextBase::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL requesting_origin = requesting_frame.GetOrigin();
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid()) {
    std::string type_name =
        PermissionUtil::GetPermissionString(content_settings_type_);

    DVLOG(1) << "Attempt to use " << type_name
             << " from an invalid URL: " << requesting_origin << ","
             << embedding_origin << " (" << type_name
             << " is not supported in popups)";
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), false /* persist */,
                        CONTENT_SETTING_BLOCK);
    return;
  }

  // Check the content setting to see if the user has already made a decision,
  // or if the origin is under embargo. If so, respect that decision.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      id.render_process_id(), id.render_frame_id());
  PermissionResult result =
      GetPermissionStatus(rfh, requesting_origin, embedding_origin);

  if (result.content_setting == CONTENT_SETTING_ALLOW ||
      result.content_setting == CONTENT_SETTING_BLOCK) {
    switch (result.source) {
      case PermissionStatusSource::KILL_SWITCH:
        // Block the request and log to the developer console.
        LogPermissionBlockedMessage(web_contents,
                                    kPermissionBlockedKillSwitchMessage,
                                    content_settings_type_);
        std::move(callback).Run(CONTENT_SETTING_BLOCK);
        return;
      case PermissionStatusSource::MULTIPLE_DISMISSALS:
        LogPermissionBlockedMessage(web_contents,
                                    kPermissionBlockedRepeatedDismissalsMessage,
                                    content_settings_type_);
        break;
      case PermissionStatusSource::MULTIPLE_IGNORES:
        LogPermissionBlockedMessage(web_contents,
                                    kPermissionBlockedRepeatedIgnoresMessage,
                                    content_settings_type_);
        break;
      case PermissionStatusSource::FEATURE_POLICY:
        LogPermissionBlockedMessage(web_contents,
                                    kPermissionBlockedFeaturePolicyMessage,
                                    content_settings_type_);
        break;
      case PermissionStatusSource::PORTAL:
        LogPermissionBlockedMessage(web_contents,
                                    kPermissionBlockedPortalsMessage,
                                    content_settings_type_);
        break;
      case PermissionStatusSource::INSECURE_ORIGIN:
      case PermissionStatusSource::UNSPECIFIED:
      case PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN:
        break;
    }

    // If we are under embargo, record the embargo reason for which we have
    // suppressed the prompt.
    PermissionUmaUtil::RecordEmbargoPromptSuppressionFromSource(result.source);
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), false /* persist */,
                        result.content_setting);
    return;
  }

  // Don't show request permission UI for an inactive RenderFrameHost. If this
  // is called when RenderFrameHost is in BackForwardCache, evict the document
  // as the page might not distinguish properly between user denying the
  // permission and automatic rejection, leading to an inconsistent UX after
  // restoring the page from the cache.
  if (rfh->IsInactiveAndDisallowReactivation()) {
    std::move(callback).Run(result.content_setting);
    return;
  }

  // We are going to show a prompt now.
  PermissionUmaUtil::PermissionRequested(content_settings_type_,
                                         requesting_origin);
  PermissionUmaUtil::RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus::NOT_EMBARGOED);

  DecidePermission(web_contents, id, requesting_origin, embedding_origin,
                   user_gesture, std::move(callback));
}

void PermissionContextBase::UserMadePermissionDecision(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {}

PermissionResult PermissionContextBase::GetPermissionStatus(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // If the permission has been disabled through Finch, block all requests.
  if (IsPermissionKillSwitchOn()) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::KILL_SWITCH);
  }

  if (!IsPermissionAvailableToOrigins(requesting_origin, embedding_origin)) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::INSECURE_ORIGIN);
  }

  // Check whether the feature is enabled for the frame by feature policy. We
  // can only do this when a RenderFrameHost has been provided.
  if (render_frame_host &&
      !PermissionAllowedByFeaturePolicy(render_frame_host)) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::FEATURE_POLICY);
  }

  if (render_frame_host) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);

    // Permissions are denied for portals.
    if (web_contents && web_contents->IsPortal()) {
      return PermissionResult(CONTENT_SETTING_BLOCK,
                              PermissionStatusSource::PORTAL);
    }

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
          !url::Origin::Create(virtual_url)
               .IsSameOriginWith(url::Origin::Create(loaded_url))) {
        return PermissionResult(
            CONTENT_SETTING_BLOCK,
            PermissionStatusSource::VIRTUAL_URL_DIFFERENT_ORIGIN);
      }
    }
  }

  ContentSetting content_setting = GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);

  if (content_setting != CONTENT_SETTING_ASK) {
    return PermissionResult(content_setting,
                            PermissionStatusSource::UNSPECIFIED);
  }

  PermissionResult result =
      PermissionsClient::Get()
          ->GetPermissionDecisionAutoBlocker(browser_context_)
          ->GetEmbargoResult(requesting_origin, content_settings_type_);
  DCHECK(result.content_setting == CONTENT_SETTING_ASK ||
         result.content_setting == CONTENT_SETTING_BLOCK);
  return result;
}

bool PermissionContextBase::IsPermissionAvailableToOrigins(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (IsRestrictedToSecureOrigins()) {
    if (!blink::network_utils::IsOriginSecure(requesting_origin))
      return false;

    // TODO(raymes): We should check the entire chain of embedders here whenever
    // possible as this corresponds to the requirements of the secure contexts
    // spec and matches what is implemented in blink. Right now we just check
    // the top level and requesting origins.
    if (!PermissionsClient::Get()->CanBypassEmbeddingOriginCheck(
            requesting_origin, embedding_origin) &&
        !blink::network_utils::IsOriginSecure(embedding_origin)) {
      return false;
    }
  }
  return true;
}

PermissionResult PermissionContextBase::UpdatePermissionStatusWithDeviceStatus(
    PermissionResult result,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
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
                                      content_settings_type_, std::string(),
                                      CONTENT_SETTING_DEFAULT);
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
                          content_settings_type_, std::string());
}

void PermissionContextBase::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Under permission delegation, when we display a permission prompt, the
  // origin displayed in the prompt should never differ from the top-level
  // origin. Storage access API requests are excluded as they are expected to
  // request permissions from the frame origin needing access.
  DCHECK(PermissionsClient::Get()->CanBypassEmbeddingOriginCheck(
             requesting_origin, embedding_origin) ||
         requesting_origin == embedding_origin ||
         content_settings_type_ == ContentSettingsType::STORAGE_ACCESS);

  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  // TODO(felt): sometimes |permission_request_manager| is null. This check is
  // meant to prevent crashes. See crbug.com/457091.
  if (!permission_request_manager)
    return;

  std::unique_ptr<PermissionRequest> request_ptr =
      std::make_unique<PermissionRequestImpl>(
          requesting_origin, content_settings_type_, user_gesture,
          base::BindOnce(&PermissionContextBase::PermissionDecided,
                         weak_factory_.GetWeakPtr(), id, requesting_origin,
                         embedding_origin, std::move(callback)),
          base::BindOnce(&PermissionContextBase::CleanUpRequest,
                         weak_factory_.GetWeakPtr(), id));
  PermissionRequest* request = request_ptr.get();

  bool inserted =
      pending_requests_
          .insert(std::make_pair(id.ToString(), std::move(request_ptr)))
          .second;
  DCHECK(inserted) << "Duplicate id " << id.ToString();

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      id.render_process_id(), id.render_frame_id());

  if (!rfh) {
    request->Cancelled();
    request->RequestFinished();
    return;
  }

  permission_request_manager->AddRequest(rfh, request);
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    ContentSetting content_setting) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_DEFAULT);
  UserMadePermissionDecision(id, requesting_origin, embedding_origin,
                             content_setting);

  bool persist = content_setting != CONTENT_SETTING_DEFAULT;
  NotifyPermissionSet(id, requesting_origin, embedding_origin,
                      std::move(callback), persist, content_setting);
}

content::BrowserContext* PermissionContextBase::browser_context() const {
  return browser_context_;
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (persist)
    UpdateContentSetting(requesting_origin, embedding_origin, content_setting);

  UpdateTabContext(id, requesting_origin,
                   content_setting == CONTENT_SETTING_ALLOW);

  if (content_setting == CONTENT_SETTING_DEFAULT)
    content_setting = CONTENT_SETTING_ASK;

  std::move(callback).Run(content_setting);
}

void PermissionContextBase::CleanUpRequest(const PermissionRequestID& id) {
  size_t success = pending_requests_.erase(id.ToString());
  DCHECK(success == 1) << "Missing request " << id.ToString();
}

void PermissionContextBase::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);
  DCHECK(!requesting_origin.SchemeIsFile());
  DCHECK(!embedding_origin.SchemeIsFile());

  PermissionsClient::Get()
      ->GetSettingsMap(browser_context_)
      ->SetContentSettingDefaultScope(requesting_origin, embedding_origin,
                                      content_settings_type_, std::string(),
                                      content_setting);
}

bool PermissionContextBase::PermissionAllowedByFeaturePolicy(
    content::RenderFrameHost* rfh) const {
  // Some features don't have an associated feature policy yet. Allow those.
  if (feature_policy_feature_ == blink::mojom::FeaturePolicyFeature::kNotFound)
    return true;

  return rfh->IsFeatureEnabled(feature_policy_feature_);
}

}  // namespace permissions
