// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace permissions {
namespace {

// Helper methods to convert ContentSetting to PermissionStatus and vice versa.
PermissionStatus ContentSettingToPermissionStatus(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return PermissionStatus::ASK;
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return PermissionStatus::DENIED;
}

ContentSetting PermissionStatusToContentSetting(PermissionStatus status) {
  switch (status) {
    case PermissionStatus::GRANTED:
      return CONTENT_SETTING_ALLOW;
    case PermissionStatus::ASK:
      return CONTENT_SETTING_ASK;
    case PermissionStatus::DENIED:
    default:
      return CONTENT_SETTING_BLOCK;
  }

  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

// Helper method to convert PermissionType to ContentSettingType.
// If PermissionType is not supported or found, returns
// ContentSettingsType::DEFAULT.
ContentSettingsType PermissionTypeToContentSettingSafe(
    PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI:
      return ContentSettingsType::MIDI;
    case PermissionType::MIDI_SYSEX:
      return ContentSettingsType::MIDI_SYSEX;
    case PermissionType::NOTIFICATIONS:
      return ContentSettingsType::NOTIFICATIONS;
    case PermissionType::GEOLOCATION:
      return ContentSettingsType::GEOLOCATION;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_WIN)
      return ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER;
#else
      break;
#endif
    case PermissionType::DURABLE_STORAGE:
      return ContentSettingsType::DURABLE_STORAGE;
    case PermissionType::AUDIO_CAPTURE:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case PermissionType::VIDEO_CAPTURE:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case PermissionType::BACKGROUND_SYNC:
      return ContentSettingsType::BACKGROUND_SYNC;
    case PermissionType::SENSORS:
      return ContentSettingsType::SENSORS;
    case PermissionType::ACCESSIBILITY_EVENTS:
      return ContentSettingsType::ACCESSIBILITY_EVENTS;
    case PermissionType::CLIPBOARD_READ_WRITE:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      return ContentSettingsType::CLIPBOARD_SANITIZED_WRITE;
    case PermissionType::PAYMENT_HANDLER:
      return ContentSettingsType::PAYMENT_HANDLER;
    case PermissionType::BACKGROUND_FETCH:
      return ContentSettingsType::BACKGROUND_FETCH;
    case PermissionType::IDLE_DETECTION:
      return ContentSettingsType::IDLE_DETECTION;
    case PermissionType::PERIODIC_BACKGROUND_SYNC:
      return ContentSettingsType::PERIODIC_BACKGROUND_SYNC;
    case PermissionType::WAKE_LOCK_SCREEN:
      return ContentSettingsType::WAKE_LOCK_SCREEN;
    case PermissionType::WAKE_LOCK_SYSTEM:
      return ContentSettingsType::WAKE_LOCK_SYSTEM;
    case PermissionType::NFC:
      return ContentSettingsType::NFC;
    case PermissionType::VR:
      return ContentSettingsType::VR;
    case PermissionType::AR:
      return ContentSettingsType::AR;
    case PermissionType::STORAGE_ACCESS_GRANT:
      return ContentSettingsType::STORAGE_ACCESS;
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
    case PermissionType::WINDOW_PLACEMENT:
      return ContentSettingsType::WINDOW_PLACEMENT;
    case PermissionType::FONT_ACCESS:
      return ContentSettingsType::FONT_ACCESS;
    case PermissionType::DISPLAY_CAPTURE:
      return ContentSettingsType::DISPLAY_CAPTURE;
    case PermissionType::NUM:
      break;
  }

  return ContentSettingsType::DEFAULT;
}

void SubscriptionCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    ContentSetting content_setting) {
  std::move(callback).Run(ContentSettingToPermissionStatus(content_setting));
}

void PermissionStatusCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<ContentSetting>& vector) {
  DCHECK_EQ(1ul, vector.size());
  std::move(callback).Run(ContentSettingToPermissionStatus(vector.at(0)));
}

void PermissionStatusVectorCallbackWrapper(
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback,
    const std::vector<ContentSetting>& content_settings) {
  std::vector<PermissionStatus> permission_statuses;
  std::transform(content_settings.begin(), content_settings.end(),
                 back_inserter(permission_statuses),
                 ContentSettingToPermissionStatus);
  std::move(callback).Run(permission_statuses);
}

void ContentSettingCallbackWrapper(
    base::OnceCallback<void(ContentSetting)> callback,
    const std::vector<ContentSetting>& vector) {
  DCHECK_EQ(1ul, vector.size());
  std::move(callback).Run(vector.at(0));
}

// TODO(crbug.com/698985): As part of the permission API refactoring, this
// method should be used in all places where wer check or request permissions.
// Currently it is used only in `RequestPermissions` and
// `GetPermissionStatusForFrame`.
GURL GetEmbeddingOrigin(content::WebContents* web_contents,
                        const GURL& requesting_origin) {
  if (PermissionsClient::Get()->DoOriginsMatchNewTabPage(
          requesting_origin,
          web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL())) {
    return web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  } else {
    return PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
  }
}

}  // anonymous namespace

class PermissionManager::PendingRequest {
 public:
  PendingRequest(
      content::RenderFrameHost* render_frame_host,
      const std::vector<ContentSettingsType>& permissions,
      base::OnceCallback<void(const std::vector<ContentSetting>&)> callback)
      : render_process_id_(render_frame_host->GetProcess()->GetID()),
        render_frame_id_(render_frame_host->GetRoutingID()),
        callback_(std::move(callback)),
        permissions_(permissions),
        results_(permissions.size(), CONTENT_SETTING_BLOCK),
        remaining_results_(permissions.size()) {}

  void SetContentSetting(int permission_id, ContentSetting content_setting) {
    DCHECK(!IsComplete());

    results_[permission_id] = content_setting;
    --remaining_results_;
  }

  bool IsComplete() const { return remaining_results_ == 0; }

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

  base::OnceCallback<void(const std::vector<ContentSetting>&)> TakeCallback() {
    return std::move(callback_);
  }

  std::vector<ContentSettingsType> permissions() const { return permissions_; }

  std::vector<ContentSetting> results() const { return results_; }

 private:
  int render_process_id_;
  int render_frame_id_;
  base::OnceCallback<void(const std::vector<ContentSetting>&)> callback_;
  std::vector<ContentSettingsType> permissions_;
  std::vector<ContentSetting> results_;
  size_t remaining_results_;
};

// Object to track the callback passed to
// PermissionContextBase::RequestPermission. The callback passed in will never
// be run when a permission prompt has been ignored, but it's important that we
// know when a prompt is ignored to clean up |pending_requests_| correctly.
// If the callback is destroyed without being run, the destructor here will
// cancel the request to clean up. |permission_manager| must outlive this
// object.
class PermissionManager::PermissionResponseCallback {
 public:
  PermissionResponseCallback(PermissionManager* permission_manager,
                             PendingRequestLocalId request_local_id,
                             int permission_id)
      : permission_manager_(permission_manager),
        request_local_id_(request_local_id),
        permission_id_(permission_id),
        request_answered_(false) {}

  PermissionResponseCallback(const PermissionResponseCallback&) = delete;
  PermissionResponseCallback& operator=(const PermissionResponseCallback&) =
      delete;

  ~PermissionResponseCallback() {
    if (!request_answered_ &&
        permission_manager_->pending_requests_.Lookup(request_local_id_)) {
      permission_manager_->pending_requests_.Remove(request_local_id_);
    }
  }

  void OnPermissionsRequestResponseStatus(ContentSetting content_setting) {
    request_answered_ = true;
    permission_manager_->OnPermissionsRequestResponseStatus(
        request_local_id_, permission_id_, content_setting);
  }

 private:
  raw_ptr<PermissionManager> permission_manager_;
  PendingRequestLocalId request_local_id_;
  int permission_id_;
  bool request_answered_;
};

struct PermissionManager::Subscription {
  ContentSettingsType permission;
  GURL requesting_origin;
  int render_frame_id = -1;
  int render_process_id = -1;
  base::RepeatingCallback<void(ContentSetting)> callback;
  ContentSetting current_value;
};

PermissionManager::PermissionManager(content::BrowserContext* browser_context,
                                     PermissionContextMap permission_contexts)
    : browser_context_(browser_context),
      permission_contexts_(std::move(permission_contexts)) {}

PermissionManager::~PermissionManager() {
  DCHECK(pending_requests_.IsEmpty());
  DCHECK(subscriptions_.IsEmpty());
}

void PermissionManager::Shutdown() {
  is_shutting_down_ = true;

  if (!subscriptions_.IsEmpty()) {
    subscriptions_.Clear();
    for (const auto& type_to_count : subscription_type_counts_) {
      if (type_to_count.second > 0) {
        PermissionContextBase* context =
            GetPermissionContext(type_to_count.first);
        context->RemoveObserver(this);
      }
    }
    subscription_type_counts_.clear();
  }
}

// static
ContentSettingsType PermissionManager::PermissionTypeToContentSetting(
    PermissionType permission) {
  ContentSettingsType content_setting =
      PermissionTypeToContentSettingSafe(permission);
  DCHECK_NE(content_setting, ContentSettingsType::DEFAULT)
      << "Unknown content setting for permission "
      << static_cast<int>(permission);
  return content_setting;
}

PermissionContextBase* PermissionManager::GetPermissionContextForTesting(
    ContentSettingsType type) {
  return GetPermissionContext(type);
}

GURL PermissionManager::GetCanonicalOrigin(ContentSettingsType permission,
                                           const GURL& requesting_origin,
                                           const GURL& embedding_origin) const {
  absl::optional<GURL> override_origin =
      PermissionsClient::Get()->OverrideCanonicalOrigin(requesting_origin,
                                                        embedding_origin);
  if (override_origin)
    return override_origin.value();

  // TODO(crbug.com/987654): Generalize this to other "background permissions",
  // that is, permissions that can be used by a service worker. This includes
  // durable storage, background sync, etc.
  if (permission == ContentSettingsType::NOTIFICATIONS)
    return requesting_origin;

  // Storage access grants from the storage access API are paired between the
  // origin of the frame making the request and the top level origin so we need
  // to ensure we return the proper origin here.
  if (permission == ContentSettingsType::STORAGE_ACCESS)
    return requesting_origin;

  return embedding_origin;
}

void PermissionManager::RequestPermission(
    ContentSettingsType content_settings_type,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(ContentSetting)> callback) {
  RequestPermissions(
      std::vector<ContentSettingsType>(1, content_settings_type),
      render_frame_host, requesting_origin, user_gesture,
      base::BindOnce(&ContentSettingCallbackWrapper, std::move(callback)));
}

void PermissionManager::RequestPermissions(
    const std::vector<ContentSettingsType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<ContentSetting>&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (permissions.empty()) {
    std::move(callback).Run(std::vector<ContentSetting>());
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  auto request_local_id = request_local_id_generator_.GenerateNextId();
  pending_requests_.AddWithID(
      std::make_unique<PendingRequest>(render_frame_host, permissions,
                                       std::move(callback)),
      request_local_id);

  const PermissionRequestID request_id(render_frame_host, request_local_id);
  GURL embedding_origin = GetEmbeddingOrigin(web_contents, requesting_origin);

  for (size_t i = 0; i < permissions.size(); ++i) {
    const ContentSettingsType permission = permissions[i];
    const GURL canonical_requesting_origin =
        GetCanonicalOrigin(permission, requesting_origin, embedding_origin);

    auto response_callback =
        std::make_unique<PermissionResponseCallback>(this, request_local_id, i);
    auto status = GetPermissionOverrideForDevTools(
        url::Origin::Create(canonical_requesting_origin), permission);
    if (status != CONTENT_SETTING_DEFAULT) {
      response_callback->OnPermissionsRequestResponseStatus(
          CONTENT_SETTING_ALLOW);
      continue;
    }

    PermissionContextBase* context = GetPermissionContext(permission);
    DCHECK(context);

    context->RequestPermission(
        web_contents, request_id, canonical_requesting_origin, user_gesture,
        base::BindOnce(
            &PermissionResponseCallback::OnPermissionsRequestResponseStatus,
            std::move(response_callback)));
  }
}

void PermissionManager::RequestPermissionFromCurrentDocument(
    ContentSettingsType content_settings_type,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(ContentSetting)> callback) {
  RequestPermissionsFromCurrentDocument(
      std::vector<ContentSettingsType>(1, content_settings_type),
      render_frame_host, user_gesture,
      base::BindOnce(&ContentSettingCallbackWrapper, std::move(callback)));
}

void PermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<ContentSettingsType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<ContentSetting>&)> callback) {
  RequestPermissions(
      permissions, render_frame_host,
      PermissionUtil::GetLastCommittedOriginAsURL(render_frame_host),
      user_gesture, std::move(callback));
}

PermissionResult PermissionManager::GetPermissionStatus(
    ContentSettingsType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // With permission delegation enabled, this function should only ever be
  // called for the top level origin (or a service worker origin).
  // GetPermissionStatusForFrame should be called when to determine the status
  // for an embedded frame.
  DCHECK_EQ(requesting_origin, embedding_origin);

  return GetPermissionStatusHelper(permission, nullptr /* render_frame_host */,
                                   requesting_origin, embedding_origin);
}

PermissionResult PermissionManager::GetPermissionStatusForFrame(
    ContentSettingsType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  GURL embedding_origin = GetEmbeddingOrigin(web_contents, requesting_origin);

  return GetPermissionStatusHelper(permission, render_frame_host,
                                   requesting_origin, embedding_origin);
}

PermissionResult PermissionManager::GetPermissionStatusForCurrentDocument(
    ContentSettingsType permission,
    content::RenderFrameHost* render_frame_host) {
  GURL origin = PermissionUtil::GetLastCommittedOriginAsURL(render_frame_host);
  return GetPermissionStatusHelper(permission, render_frame_host, origin,
                                   origin);
}

void PermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(PermissionStatus)> callback) {
  ContentSettingsType content_settings_type =
      PermissionTypeToContentSetting(permission);
  RequestPermissions(
      std::vector<ContentSettingsType>(1, content_settings_type),
      render_frame_host, requesting_origin, user_gesture,
      base::BindOnce(&PermissionStatusCallbackWrapper, std::move(callback)));
}

void PermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  std::vector<ContentSettingsType> content_settings_types;
  std::transform(permissions.begin(), permissions.end(),
                 back_inserter(content_settings_types),
                 PermissionTypeToContentSetting);
  RequestPermissions(content_settings_types, render_frame_host,
                     requesting_origin, user_gesture,
                     base::BindOnce(&PermissionStatusVectorCallbackWrapper,
                                    std::move(callback)));
}

PermissionContextBase* PermissionManager::GetPermissionContext(
    ContentSettingsType type) {
  const auto& it = permission_contexts_.find(type);
  return it == permission_contexts_.end() ? nullptr : it->second.get();
}

void PermissionManager::OnPermissionsRequestResponseStatus(
    PendingRequestLocalId request_local_id,
    int permission_id,
    ContentSetting content_setting) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_local_id);
  if (!pending_request)
    return;

  pending_request->SetContentSetting(permission_id, content_setting);

  if (!pending_request->IsComplete())
    return;

  pending_request->TakeCallback().Run(pending_request->results());
  pending_requests_.Remove(request_local_id);
}

void PermissionManager::ResetPermission(PermissionType permission,
                                        const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSettingsType type = PermissionTypeToContentSetting(permission);
  PermissionContextBase* context = GetPermissionContext(type);
  if (!context)
    return;
  context->ResetPermission(
      GetCanonicalOrigin(type, requesting_origin, embedding_origin),
      embedding_origin.DeprecatedGetOriginAsURL());
}

PermissionStatus PermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PermissionResult result =
      GetPermissionStatus(PermissionTypeToContentSetting(permission),
                          requesting_origin, embedding_origin);
  ContentSettingsType type = PermissionTypeToContentSetting(permission);
  // TODO(benwells): split this into two functions, GetPermissionStatus and
  // GetPermissionStatusForPermissionsAPI.
  PermissionContextBase* context = GetPermissionContext(type);
  if (context) {
    result = context->UpdatePermissionStatusWithDeviceStatus(
        result, GetCanonicalOrigin(type, requesting_origin, embedding_origin),
        embedding_origin);
  }

  return ContentSettingToPermissionStatus(result.content_setting);
}

PermissionStatus PermissionManager::GetPermissionStatusForFrame(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ContentSettingsType type = PermissionTypeToContentSetting(permission);
  PermissionResult result =
      GetPermissionStatusForFrame(type, render_frame_host, requesting_origin);

  // TODO(benwells): split this into two functions, GetPermissionStatus and
  // GetPermissionStatusForPermissionsAPI.
  PermissionContextBase* context =
      GetPermissionContext(PermissionTypeToContentSetting(permission));
  if (context) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    GURL embedding_origin =
        PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
    result = context->UpdatePermissionStatusWithDeviceStatus(
        result, GetCanonicalOrigin(type, requesting_origin, embedding_origin),
        embedding_origin);
  }

  return ContentSettingToPermissionStatus(result.content_setting);
}

bool PermissionManager::IsPermissionOverridableByDevTools(
    content::PermissionType permission,
    const absl::optional<url::Origin>& origin) {
  ContentSettingsType type = PermissionTypeToContentSettingSafe(permission);
  PermissionContextBase* context = GetPermissionContext(type);

  if (!context || context->IsPermissionKillSwitchOn())
    return false;

  return !origin || context->IsPermissionAvailableToOrigins(origin->GetURL(),
                                                            origin->GetURL());
}

PermissionManager::SubscriptionId
PermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(PermissionStatus)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_)
    return SubscriptionId();

  ContentSettingsType content_type = PermissionTypeToContentSetting(permission);
  auto& type_count = subscription_type_counts_[content_type];
  if (type_count == 0) {
    PermissionContextBase* context = GetPermissionContext(content_type);
    context->AddObserver(this);
  }
  ++type_count;

  auto subscription = std::make_unique<Subscription>();

  // The RFH may be null if the request is for a worker.
  GURL embedding_origin;
  if (render_frame_host) {
    // Permissions API must be deferred during the prerendering.
    DCHECK_NE(render_frame_host->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kPrerendering);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    embedding_origin =
        PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
    subscription->render_frame_id = render_frame_host->GetRoutingID();
    subscription->render_process_id = render_frame_host->GetProcess()->GetID();
    subscription->current_value =
        GetPermissionStatusForFrame(content_type, render_frame_host,
                                    requesting_origin)
            .content_setting;
  } else {
    embedding_origin = requesting_origin;
    subscription->render_frame_id = -1;
    subscription->render_process_id = -1;
    subscription->current_value =
        GetPermissionStatus(content_type, requesting_origin, requesting_origin)
            .content_setting;
  }

  subscription->permission = content_type;
  subscription->requesting_origin =
      GetCanonicalOrigin(content_type, requesting_origin, embedding_origin);
  subscription->callback =
      base::BindRepeating(&SubscriptionCallbackWrapper, std::move(callback));

  auto id = subscription_id_generator_.GenerateNextId();
  subscriptions_.AddWithID(std::move(subscription), id);
  return id;
}

void PermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_)
    return;

  Subscription* subscription = subscriptions_.Lookup(subscription_id);
  if (!subscription)
    return;

  ContentSettingsType type = subscription->permission;
  subscriptions_.Remove(subscription_id);
  auto type_count = subscription_type_counts_.find(type);
  CHECK(type_count != subscription_type_counts_.end());
  CHECK_GT(type_count->second, size_t(0));
  type_count->second--;
  if (type_count->second == 0) {
    PermissionContextBase* context = GetPermissionContext(type);
    context->RemoveObserver(this);
  }
}

bool PermissionManager::IsPermissionKillSwitchOn(
    ContentSettingsType permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return GetPermissionContext(permission)->IsPermissionKillSwitchOn();
}

void PermissionManager::OnPermissionChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());

  std::vector<base::OnceClosure> callbacks;
  callbacks.reserve(subscriptions_.size());
  for (SubscriptionsMap::iterator iter(&subscriptions_); !iter.IsAtEnd();
       iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (!content_type_set.Contains(subscription->permission))
      continue;

    // The RFH may be null if the request is for a worker.
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        subscription->render_process_id, subscription->render_frame_id);
    GURL embedding_origin;
    if (rfh) {
      content::WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(rfh);
      embedding_origin =
          PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
    } else {
      embedding_origin = subscription->requesting_origin;
    }

    if (!primary_pattern.Matches(subscription->requesting_origin) ||
        !secondary_pattern.Matches(embedding_origin)) {
      continue;
    }

    ContentSetting new_value;
    if (rfh) {
      new_value = GetPermissionStatusForFrame(subscription->permission, rfh,
                                              subscription->requesting_origin)
                      .content_setting;
    } else {
      new_value = GetPermissionStatus(subscription->permission,
                                      subscription->requesting_origin,
                                      subscription->requesting_origin)
                      .content_setting;
    }

    if (subscription->current_value == new_value)
      continue;

    subscription->current_value = new_value;

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(base::BindOnce(subscription->callback, new_value));
  }

  for (auto& callback : callbacks)
    std::move(callback).Run();
}

PermissionResult PermissionManager::GetPermissionStatusHelper(
    ContentSettingsType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  GURL canonical_requesting_origin =
      GetCanonicalOrigin(permission, requesting_origin, embedding_origin);
  auto status = GetPermissionOverrideForDevTools(
      url::Origin::Create(canonical_requesting_origin), permission);
  if (status != CONTENT_SETTING_DEFAULT)
    return PermissionResult(status, PermissionStatusSource::UNSPECIFIED);
  PermissionContextBase* context = GetPermissionContext(permission);
  PermissionResult result = context->GetPermissionStatus(
      render_frame_host, canonical_requesting_origin.DeprecatedGetOriginAsURL(),
      embedding_origin.DeprecatedGetOriginAsURL());
  DCHECK(result.content_setting == CONTENT_SETTING_ALLOW ||
         result.content_setting == CONTENT_SETTING_ASK ||
         result.content_setting == CONTENT_SETTING_BLOCK);
  return result;
}

void PermissionManager::SetPermissionOverridesForDevTools(
    const absl::optional<url::Origin>& optional_origin,
    const PermissionOverrides& overrides) {
  ContentSettingsTypeOverrides result;
  for (const auto& item : overrides) {
    ContentSettingsType content_setting =
        PermissionTypeToContentSettingSafe(item.first);
    if (content_setting != ContentSettingsType::DEFAULT)
      result[content_setting] = PermissionStatusToContentSetting(item.second);
  }
  const url::Origin& origin =
      optional_origin.value_or(devtools_global_overrides_origin_);
  devtools_permission_overrides_[origin] = std::move(result);
}

void PermissionManager::ResetPermissionOverridesForDevTools() {
  devtools_permission_overrides_.clear();
}

ContentSetting PermissionManager::GetPermissionOverrideForDevTools(
    const url::Origin& origin,
    ContentSettingsType permission) {
  auto it = devtools_permission_overrides_.find(origin);
  if (it == devtools_permission_overrides_.end())
    it = devtools_permission_overrides_.find(devtools_global_overrides_origin_);
  if (it == devtools_permission_overrides_.end())
    return CONTENT_SETTING_DEFAULT;

  auto setting_it = it->second.find(permission);
  if (setting_it == it->second.end())
    return CONTENT_SETTING_DEFAULT;

  return setting_it->second;
}

}  // namespace permissions
