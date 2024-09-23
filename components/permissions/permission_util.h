// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_

#include <string>

#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_prompt.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class RenderFrameHost;
class RenderProcessHost;
}  // namespace content

class GURL;

namespace permissions {
class PermissionRequest;

// This enum backs a UMA histogram, so it must be treated as append-only.
enum class PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  GRANTED_ONCE = 5,

  // Always keep this at the end.
  NUM,
};

enum PermissionPromptViewID {
  VIEW_ID_PERMISSION_PROMPT_NONE = 0,
  VIEW_ID_PERMISSION_PROMPT_EXTRA_TEXT,
  VIEW_ID_PERMISSION_PROMPT_LINK,
};

// A utility class for permissions.
class PermissionUtil {
 public:
  PermissionUtil() = delete;
  PermissionUtil(const PermissionUtil&) = delete;
  PermissionUtil& operator=(const PermissionUtil&) = delete;

  // Returns the permission string for the given permission.
  static std::string GetPermissionString(ContentSettingsType);

  // Returns the gesture type corresponding to whether a permission request is
  // made with or without a user gesture.
  static PermissionRequestGestureType GetGestureType(bool user_gesture);

  // Limited conversion of ContentSettingsType to PermissionType. Returns true
  // if the conversion was performed.
  // TODO(timloh): Try to remove this function. Mainly we need to work out how
  // to remove the usage in PermissionUmaUtil, which uses PermissionType as a
  // histogram value to count permission request metrics.
  static bool GetPermissionType(ContentSettingsType type,
                                blink::PermissionType* out);

  // Returns the corresponding permissions policy feature to the given content
  // settings type, or nullopt if there is none.
  static std::optional<blink::mojom::PermissionsPolicyFeature>
  GetPermissionsPolicyFeature(ContentSettingsType type);

  // Checks whether the given ContentSettingsType is a permission. Use this
  // to determine whether a specific ContentSettingsType is supported by the
  // PermissionManager.
  static bool IsPermission(ContentSettingsType type);

  // Check whether the given permission request has low priority, based on the
  // acceptance rates data (notifications and geolocations have the lowest
  // acceptance data)
  static bool IsLowPriorityPermissionRequest(const PermissionRequest* request);

  // Checks whether the given ContentSettingsType is a guard content setting,
  // meaning it does not support allow setting and toggles between "ask" and
  // "block" instead. This is primarily used for chooser-based permissions.
  static bool IsGuardContentSetting(ContentSettingsType type);

  // Returns true if the permission for `type` can be granted for a short period
  // of time. This means the following:
  // - Permission prompts will have a button that is labeled along the lines of
  //   "Allow this time".
  // - The `permissions.query` API will report PermissionStatus.state as
  //   "granted" within this short time window.
  // - Subsequent requests to the permission-gated API in this time window will
  //   succeed without user mediation.
  static bool DoesSupportTemporaryGrants(ContentSettingsType type);

  // For a permission `type` that `DoesSupportTemporaryGrants()`, returns true
  // if that temporary grant is stored in the `OneTimePermissionProvider` in
  // `HostContentSettingMap`, and false elsewhere.
  static bool DoesStoreTemporaryGrantsInHcsm(ContentSettingsType type);

  // Returns the authoritative `embedding origin`, as a GURL, to be used for
  // permission decisions in `render_frame_host`.
  // TODO(crbug.com/40226169): Remove this method when possible.
  static GURL GetLastCommittedOriginAsURL(
      content::RenderFrameHost* render_frame_host);

  // Helper method to convert `PermissionType` to `ContentSettingType`.
  // If `PermissionType` is not supported or found, returns
  // ContentSettingsType::DEFAULT.
  static ContentSettingsType PermissionTypeToContentSettingTypeSafe(
      blink::PermissionType permission);

  // Helper method to convert `PermissionType` to `ContentSettingType`.
  static ContentSettingsType PermissionTypeToContentSettingType(
      blink::PermissionType permission);

  // Helper method to convert `ContentSettingType` to `PermissionType`.
  static blink::PermissionType ContentSettingTypeToPermissionType(
      ContentSettingsType permission);

  // Helper method to convert PermissionStatus to ContentSetting.
  static ContentSetting PermissionStatusToContentSetting(
      blink::mojom::PermissionStatus status);

  // Helper methods to convert ContentSetting to PermissionStatus and vice
  // versa.
  static blink::mojom::PermissionStatus ContentSettingToPermissionStatus(
      ContentSetting setting);

  // If an iframed document/worker inherits a different StoragePartition from
  // its embedder than it would use if it were a main frame, we should block
  // undelegated permissions. Because permissions are scoped to BrowserContext
  // instead of StoragePartition, without this check the aforementioned iframe
  // would be given undelegated permissions if the user had granted its origin
  // access when it was loaded as a main frame.
  static bool IsPermissionBlockedInPartition(
      ContentSettingsType permission,
      const GURL& requesting_origin,
      content::RenderProcessHost* render_process_host);

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
  static GURL GetCanonicalOrigin(ContentSettingsType permission,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin);

  // Returns `true` if at least one of the `delegate->Requests()` was requested
  // with a user gesture.
  static bool HasUserGesture(PermissionPrompt::Delegate* delegate);

  static bool CanPermissionRequestIgnoreStatus(
      const PermissionRequestData& request,
      content::PermissionStatusSource source);

  // Returns `true` if the current platform support permission chips.
  static bool DoesPlatformSupportChip();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_
