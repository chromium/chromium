// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_

#include <map>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_type.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class RenderProcessHost;
}

namespace permissions {
class PermissionContextBase;
struct PermissionResult;

class PermissionManager : public KeyedService,
                          public content::PermissionControllerDelegate,
                          public permissions::Observer {
 public:
  using PermissionContextMap =
      std::unordered_map<ContentSettingsType,
                         std::unique_ptr<PermissionContextBase>,
                         ContentSettingsTypeHash>;
  PermissionManager(content::BrowserContext* browser_context,
                    PermissionContextMap permission_contexts);

  PermissionManager(const PermissionManager&) = delete;
  PermissionManager& operator=(const PermissionManager&) = delete;

  ~PermissionManager() override;

  // Converts from |url|'s actual origin to the "canonical origin" that should
  // be used for the purpose of requesting/storing permissions. For example, the
  // origin of the local NTP gets mapped to the Google base URL instead. With
  // Permission Delegation it will transform the requesting origin into
  // the embedding origin because all permission checks happen on the top level
  // origin.
  //
  // All the public methods below, such as RequestPermission or
  // GetPermissionStatus, take the actual origin and do the canonicalization
  // internally. You only need to call this directly if you do something else
  // with the origin, such as display it in the UI.
  GURL GetCanonicalOrigin(ContentSettingsType permission,
                          const GURL& requesting_origin,
                          const GURL& embedding_origin) const;

  // Callers from within chrome/ should use the methods which take the
  // ContentSettingsType enum. The methods which take PermissionType values
  // are for the content::PermissionControllerDelegate overrides and shouldn't
  // be used from chrome/.
  // Deprecated. Use `RequestPermissionFromCurrentDocument` instead.
  void RequestPermission(ContentSettingsType permission,
                         content::RenderFrameHost* render_frame_host,
                         const GURL& requesting_origin,
                         bool user_gesture,
                         base::OnceCallback<void(ContentSetting)> callback);
  // Deprecated. Use `RequestPermissionsFromCurrentDocument` instead.
  void RequestPermissions(
      const std::vector<ContentSettingsType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(const std::vector<ContentSetting>&)> callback);
  void RequestPermissionFromCurrentDocument(
      ContentSettingsType permission,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(ContentSetting)> callback);
  // Requests the given `permission` on behalf of the last committed document in
  // `render_frame_host`, also performing additional checks such as Permission
  // Policy.
  void RequestPermissionsFromCurrentDocument(
      const std::vector<ContentSettingsType>& permissions,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(const std::vector<ContentSetting>&)> callback);

  PermissionResult GetPermissionStatus(ContentSettingsType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin);

  // Returns the permission status for a given `permission` and displayed,
  // top-level `origin`. This should be used only for displaying on the
  // browser's native UI (PageInfo, Settings, etc.). This method does not take
  // context specific restrictions (e.g. permission policy) into consideration.
  PermissionResult GetPermissionStatusForDisplayOnSettingsUI(
      ContentSettingsType permission,
      const GURL& origin);

  // Returns the permission status for a given frame. This should be preferred
  // over GetPermissionStatus as additional checks can be performed when we know
  // the exact context the request is coming from.
  // TODO(raymes): Currently we still pass the |requesting_origin| as a separate
  // parameter because we can't yet guarantee that it matches the last committed
  // origin of the RenderFrameHost. See crbug.com/698985.
  // Deprecated. Use `GetPermissionStatusForCurrentDocument` instead.
  PermissionResult GetPermissionStatusForFrame(
      ContentSettingsType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin);

  // Returns the status for the given `permission` on behalf of the last
  // committed document in `render_frame_host`, also performing additional
  // checks such as Permission Policy.
  PermissionResult GetPermissionStatusForCurrentDocument(
      ContentSettingsType permission,
      content::RenderFrameHost* render_frame_host);

  // Returns the status of the given `permission` for a worker on `origin`
  // running in the renderer corresponding to `render_process_host`.
  PermissionResult GetPermissionStatusForWorker(
      ContentSettingsType permission,
      content::RenderProcessHost* render_process_host,
      const url::Origin& worker_origin);

  // content::PermissionControllerDelegate implementation.
  void RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForCurrentDocument(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host) override;
  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      content::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      const GURL& worker_origin) override;
  bool IsPermissionOverridableByDevTools(
      content::PermissionType permission,
      const absl::optional<url::Origin>& origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  // TODO(raymes): Rather than exposing this, use the denial reason from
  // GetPermissionStatus in callers to determine whether a permission is
  // denied due to the kill switch.
  bool IsPermissionKillSwitchOn(ContentSettingsType);

  // For the given |origin|, overrides permissions that belong to |overrides|.
  // These permissions are in-sync with the PermissionController.
  void SetPermissionOverridesForDevTools(
      const absl::optional<url::Origin>& origin,
      const PermissionOverrides& overrides) override;
  void ResetPermissionOverridesForDevTools() override;

  // KeyedService implementation
  void Shutdown() override;

  // Helper method to convert PermissionType to ContentSettingType.
  static ContentSettingsType PermissionTypeToContentSetting(
      content::PermissionType permission);

  PermissionContextBase* GetPermissionContextForTesting(
      ContentSettingsType type);

 private:
  friend class PermissionManagerTest;

  // The `PendingRequestLocalId` will be unique within the `PermissionManager`
  // instance, thus within a `BrowserContext`, which overachieves the
  // requirement from `PermissionRequestID` that the `RequestLocalId` be unique
  // within each frame.
  class PendingRequest;
  using PendingRequestLocalId = PermissionRequestID::RequestLocalId;
  using PendingRequestsMap =
      base::IDMap<std::unique_ptr<PendingRequest>, PendingRequestLocalId>;

  class PermissionResponseCallback;

  struct Subscription;
  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<Subscription>, SubscriptionId>;
  using SubscriptionTypeCounts = base::flat_map<ContentSettingsType, size_t>;

  PermissionContextBase* GetPermissionContext(ContentSettingsType type);

  // Called when a permission was decided for a given PendingRequest. The
  // PendingRequest is identified by its |request_local_id| and the permission
  // is identified by its |permission_id|. If the PendingRequest contains more
  // than one permission, it will wait for the remaining permissions to be
  // resolved. When all the permissions have been resolved, the PendingRequest's
  // callback is run.
  void OnPermissionsRequestResponseStatus(
      PendingRequestLocalId request_local_id,
      int permission_id,
      ContentSetting status);

  // permissions::Observer:
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsTypeSet content_type_set) override;

  // Only one of |render_process_host| and |render_frame_host| should be set,
  // or neither. RenderProcessHost will be inferred from |render_frame_host|.
  PermissionResult GetPermissionStatusHelper(
      ContentSettingsType permission,
      content::RenderProcessHost* render_process_host,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  ContentSetting GetPermissionOverrideForDevTools(
      const url::Origin& origin,
      ContentSettingsType permission);

  raw_ptr<content::BrowserContext> browser_context_;

  PendingRequestsMap pending_requests_;
  PendingRequestLocalId::Generator request_local_id_generator_;

  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  // Tracks the number of Subscriptions in |subscriptions_| which have a
  // certain ContentSettingsType. An entry for a given ContentSettingsType key
  // is added on first use and never removed. This is done to utilize the
  // flat_map's efficiency in accessing/editing items and minimize the use of
  // the unefficient addition/removal of items.
  SubscriptionTypeCounts subscription_type_counts_;

  PermissionContextMap permission_contexts_;
  using ContentSettingsTypeOverrides =
      base::flat_map<ContentSettingsType, ContentSetting>;
  std::map<url::Origin, ContentSettingsTypeOverrides>
      devtools_permission_overrides_;
  url::Origin devtools_global_overrides_origin_;

  bool is_shutting_down_ = false;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
